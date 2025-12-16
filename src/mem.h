#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool mem_init(void);
void mem_dump();
uint8_t mem_read(uint16_t addr);
void mem_write(uint16_t addr, uint8_t byte);
void mem_tick(void);