#pragma once

#include "libc.h"

#include "drivers/types.h"
#include "drivers/core.h"
#include "drivers/mt_sd.h"
#include "drivers/errno.h"
#include "drivers/mmc.h"
#include "drivers/timer.h"

void hex_dump(const void* data, size_t size);
void command_loop(struct msdc_host *host);
