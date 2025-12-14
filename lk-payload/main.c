#include <inttypes.h>

#include "libc.h"

#include "common.h"

void low_uart_put(int ch)
{
    volatile uint32_t *uart_reg0 = (volatile uint32_t *)0x11002014;
    volatile uint32_t *uart_reg1 = (volatile uint32_t *)0x11002000;

    while (!((*uart_reg0) & 0x20))
    {
    }

    *uart_reg1 = ch;
}

void _putchar(char character)
{
    if (character == '\n')
        low_uart_put('\r');
    low_uart_put(character);
}

uint32_t pmic_config_interface(uint32_t reg, uint32_t val, uint32_t mask, uint32_t shift)
{
    return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))(0x4bd137f8 | 1))(reg, val, mask, shift);
}

void arch_clean_invalidate_cache_range(uintptr_t start, uintptr_t size)
{
    uintptr_t end = start + size;
    start &= ~(CACHE_LINE - 1);

    while (start < end)
    {
        __asm__ volatile(
            "mcr p15, 0, %0, c7, c14, 1\n"
            "add %0, %0, %[clsize]\n"
            : "+r"(start)
            : [clsize] "I"(CACHE_LINE)
            : "memory");
    }

    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4\n" ::"r"(0) : "memory");
}

void mtk_wdt_reset(void)
{
    volatile uint32_t *wdt_regs = (volatile uint32_t *)0x10007000;
    wdt_regs[6] = 0x1971;
    wdt_regs[0] = 0x22000014;
    wdt_regs[5] = 0x1209;
}

uint64_t g_swdl, g_misc;

static void parse_gpt(struct device_t *dev)
{
    uint8_t raw[0x800] = {0};
    dev->read(dev, 0x400, raw, sizeof(raw), USER_PART);

    for (int i = 0; i < sizeof(raw) / 0x80; ++i)
    {
        uint8_t *ptr = &raw[i * 0x80];
        uint8_t *name = ptr + 0x38;
        uint64_t start;
        memcpy(&start, ptr + 0x20, 8);

        if (start == 0)
            continue;

        if (strwcmp(name, "swdl") == 0)
        {
            printf("found swdl at 0x%08X\n", start);
            g_swdl = start;
        }

        if (strwcmp(name, "MISC") == 0)
        {
            printf("found misc at 0x%08X\n", start);
            g_misc = start;
        }
    }
}

void mmsys_reset(void)
{
    /*
    // MMSYS_SW0_B_RST is the software reset register for display components
    // including OVL, RDMA and DSI. We perform a software reset of all components
    // (logic is inverted, so writing 0 asserts reset) to clear any configuration
    // done by the first LK, ensuring the secondary LK has a clean state to work
    // from as if it had started as the first LK.
    //
    // Thanks to @bengris32 and @TheVancedGamer for figuring this out!
    */
    volatile uint32_t *reg = (volatile uint32_t *)MMSYS_SW0_B_RST;
    *reg = 0;
    __asm__ __volatile__("dmb sy" ::: "memory");
    *reg = 0xFFFFFFFF;
}

static int inject_microloader(void *data, unsigned sz, const char *partition_name)
{
    uint8_t *image_data = (uint8_t *)data;
    
    fastboot_info("");
    fastboot_info("[amonet] Injecting microloader...");

    if (sz < 0x800) {
        fastboot_info("[amonet] Image too small to inject microloader");
        return -1;
    }
    
    if (memcmp(image_data + 0x400, "ANDROID!", 8) == 0) {
        fastboot_info("[amonet] Microloader already injected");
        return 0;
    }
    
    if (memcmp(image_data, "ANDROID!", 8) != 0) {
        fastboot_info("[amonet] Not a valid Android boot image");
        return -1;
    }

    uint8_t *microloader = (uint8_t *)MICROLOADER_BACKUP;
    memcpy(image_data + 0x400, image_data, 0x400);
    memcpy(image_data, microloader, MICROLOADER_SIZE);
    
    fastboot_info("[amonet] OK");
    return 0;
}

static int flash_payload(void *data, unsigned sz)
{
    struct device_t *dev = get_device();
    
    fastboot_info("");
    fastboot_info("[amonet] Flashing LK payload...");
    
    if (sz > PAYLOAD_SIZE) {
        fastboot_fail("[amonet] Payload too large");
        return -1;
    }
    
    size_t ret = dev->write(dev, data, PAYLOAD_SRC, sz, BOOT0_PART);
    if (ret != sz) {
        fastboot_info("[amonet] Failed to write main payload");
        return -1;
    }
    
    fastboot_info("[amonet] OK");
    
    fastboot_info("[amonet] Flashing backup payload...");
    ret = dev->write(dev, data, BACKUP_SRC, sz, BOOT0_PART);
    if (ret != sz) {
        fastboot_fail("[amonet] Failed to write backup payload");
        return -1;
    }
    
    fastboot_info("[amonet] OK");
    return 0;
}

static void write_boot_command(const char *command)
{
    if (!g_misc) {
        fastboot_fail("No misc partition found!");
        return;
    }
    
    uint8_t bootloader_msg[0x20] = {0};
    strcpy((char *)bootloader_msg, command);

    struct device_t *dev = get_device();
    dev->write(dev, bootloader_msg, g_misc * 0x200, 0x20, USER_PART);

    fastboot_okay("");
    mtk_wdt_reset();
}

void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz)
{
    if (arg && strstr(arg, "recovery")) {
        write_boot_command("boot-recovery");
        return;
    }
    
    if (arg && strstr(arg, "bootloader")) {
        write_boot_command("boot-amonet");
        return;
    }
    
    cmd_reboot(arg, data, sz);
}

void cmd_flash_wrapper(const char *arg, void *data, unsigned sz)
{
    if (strcmp(arg, "boot") == 0 || strcmp(arg, "recovery") == 0) {
        if (inject_microloader(data, sz, arg) < 0) {
            fastboot_fail("Failed to inject microloader");
            return;
        }
    }
    else if (strcmp(arg, "lkp") == 0) {
        if (flash_payload(data, sz) < 0) {
            fastboot_fail("Failed to flash LK payload");
        } else {
            fastboot_okay("");
        }
        return;
    }
    else if (strstr(arg, "_amonet")) {
        char new_arg[32] = {0};
        strncpy(new_arg, arg, strstr(arg, "_amonet") - arg);
        cmd_flash(new_arg, data, sz);
        return;
    }

    cmd_flash(arg, data, sz);
}

void prepare_fastboot()
{
    uint16_t *patch;
    // Disable built-in reboot command(s)
    patch = (void *)0x4bd26e54; // reboot-bootloader
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    patch = (void *)0x4bd26e42; // reboot
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    patch = (void *)0x4bd26e0a; // flash
    *patch++ = 0x46C0; // nop
    *patch = 0x46C0;   // nop

    // Register custom command(s)
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("oem reboot-recovery", cmd_reboot_wrapper, 1);
    fastboot_register("flash:", cmd_flash_wrapper, 1);
}

int main()
{
    int ret;

    printf("This is LK-payload by xyz. Copyright 2019\n");
    printf("Updated version by k4y0z. Copyright 2019\n");
    printf("Ported to cronos by R0rt1z2. Copyright 2025\n");
    printf("Built on %s at %s\n", __DATE__, __TIME__);

    printf("Reset MMSYS\n");
    mmsys_reset();

    printf("Disable long press power off\n");
    pmic_config_interface(0x011A, 0x0, 0x1, 6);

    if (is_key_pressed(KEY_VOLDOWN) &&
        !is_key_pressed(KEY_VOLUP) &&
        !is_key_pressed(KEY_PRIVACY))
    {
        printf("Volume down, entering fastboot...\n");
        goto fastboot;
    }

    uint32_t **argptr = (void *)0x4BD00020;
    uint32_t *arg = *argptr;
    uint32_t *o_boot_mode = (uint32_t *)*argptr + 1; // argptr boot mode

    printf("arg pointer: 0x%08x, arg value: 0x%08x\n", (uint32_t)arg, arg ? arg[0] : 0);
    printf("o_boot_mode pointer: 0x%08x, value: 0x%08x\n", (uint32_t)o_boot_mode, o_boot_mode ? *o_boot_mode : 0);

    // Use advanced meta mode as a fallback
    if (*o_boot_mode == 5) {
        printf("Advanced meta mode detected, entering fastboot...\n");
        goto fastboot;
    }

    void *lk_tmp = (void *)LK_TEMP;
    void *lk_dst = (void *)LK_BASE;

    struct device_t *dev = get_device();
    parse_gpt(dev);

    if (!g_swdl || !g_misc)
    {
        printf("failed to find swdl or misc partition\n");
        goto fastboot;
    }

    if (g_misc) {
        uint8_t bootloader_msg[0x20] = {0};
        dev->read(dev, g_misc * 0x200, bootloader_msg, 0x20, USER_PART);

        if (strncmp(bootloader_msg, "FALLBACK_PLEASE", 15) == 0) {
            memset(bootloader_msg, 0, 0x10);
            dev->write(dev, bootloader_msg, g_misc * 0x200, 0x10, USER_PART);

            printf("Fallback requested, entering fastboot...\n");
            goto fastboot;
        }
    }

    printf("Backup microloader\n");
    memcpy((void *)MICROLOADER_BACKUP, (void *)MICROLOADER_SRC, MICROLOADER_SIZE);

    ret = dev->read(dev, g_swdl * 0x200 + 0x200, lk_tmp, LK_SIZE, USER_PART);
    if (ret < 0)
    {
        printf("Failed to read original LK from storage\n");
        goto fastboot;
    }

    printf("Disable interrupts\n");
    asm volatile("cpsid if");

    printf("Copy original LK\n");
    memcpy(lk_dst, lk_tmp, LK_SIZE);

    printf("Clean LK cache\n");
    arch_clean_invalidate_cache_range((uintptr_t)lk_dst, LK_SIZE);
    __asm__ __volatile__("mcr p15, 0, %0, c7, c5, 0" ::"r"(0) : "memory");
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4" ::"r"(0) : "memory");

    printf("About to jump to LK\n");
    asm volatile(
        "cpsid if\n"
        "mov r4, %0\n"
        "mov r0, %0\n"
        "bx %1\n"
        : : "r"(arg), "r"(lk_dst) : "r0", "r4", "memory");

    printf("Failure\n");

fastboot:
    // This is so original LK doesn't loop to our payload
    unsigned char overwritten[80] = {
        0xF5, 0x0A, 0xD0, 0x4B, 0x91, 0x0E, 0xD0, 0x4B, 0x01, 0x09, 0xD0, 0x4B, 0x41, 0x0B, 0xD0, 0x4B,
        0xB1, 0x0C, 0xD0, 0x4B, 0x00, 0x84, 0xD5, 0x4B, 0x09, 0x0A, 0xD0, 0x4B, 0x79, 0x0A, 0xD0, 0x4B,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x1B, 0xD0, 0x4B,
        0x15, 0x1D, 0xD0, 0x4B, 0xC5, 0x1A, 0xD0, 0x4B, 0xB1, 0x1D, 0xD0, 0x4B, 0x35, 0x1A, 0xD0, 0x4B,
        0x09, 0x1C, 0xD0, 0x4B, 0xC1, 0x19, 0xD0, 0x4B, 0x9D, 0x1C, 0xD0, 0x4B, 0x00, 0x00, 0x00, 0x00
    };
    memcpy((void *)0x4BD5C000, overwritten, sizeof(overwritten));

    uint16_t *patch;
    uint32_t *patch32;

    printf("Enable interrupts\n");
    asm volatile("cpsie if");

    printf("Force fastboot mode\n");
    *g_boot_mode = 99;

    // Enable all commands
    patch = (void *)0x4bd0d854;
    *patch++ = 0x2000; // movs r0, #0
    *patch = 0x4770;   // bx lr

    // Device is unlocked
    patch = (void *)0x4bd01ea0;
    *patch++ = 0x2001; // movs r0, #1
    *patch = 0x4770;   // bx lr

    // Make sure we don't trigger recovery mode
    patch = (void *)0x4bd0da88;
    *patch++ = 0x2000; // movs r0, #0
    *patch = 0x4770;   // bx lr

    // Register our custom fastboot commands
    prepare_fastboot();

    printf("Jumping to fastboot\n");
    int (*app)() = (void *)(0x4bd263e0 | 1);
    app();

    printf("Fatal failure\n");
    while (1) {}
}