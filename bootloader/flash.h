#pragma once
#include <stdint.h>

void flash_unlock(void);
void flash_lock(void);
int  flash_erase_page(uint32_t addr);
int  flash_program_u64(uint32_t addr, uint32_t lo, uint32_t hi);
