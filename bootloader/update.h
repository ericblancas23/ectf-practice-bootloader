#pragma once
#include <stdint.h>

/* Receive an image into `target`, verify it, report, and reset. Never returns. */
void update_run(uint32_t target, uint32_t floor) __attribute__((noreturn));
