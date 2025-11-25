#pragma once

#include "common.h"

void prepare_fastboot(void);
void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz);
void cmd_flash_wrapper(const char *arg, void *data, unsigned sz);
