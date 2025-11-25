#include "../lk-payload/include/common.h"

int main() {
    // we need to clean the cache first, since we jumped straight to the payload
    dprintf("microloader\n");
    arch_clean_invalidate_cache_range(MICROLOADER_SRC, MICROLOADER_SIZE);

    part_dev_t *dev = mt_part_get_device();
    dev->read(dev, PAYLOAD_OFFSET, (uint8_t *)PAYLOAD_ADDR, PAYLOAD_SIZE, 1);
    arch_clean_invalidate_cache_range(PAYLOAD_ADDR, PAYLOAD_SIZE);

    dprintf("jump to payload\n");
    void (*entry)() = (void (*)())(PAYLOAD_ADDR);
    entry();

    // todo: verify if this is actually causing issues
    dprintf("something went wrong, halting\n");
    while (1) {

    }
}