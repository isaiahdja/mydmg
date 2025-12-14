#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool mem_init(void);
void mem_dump(int bytes_per_line, const char *file_path);
void mem_read(void *dst, uint16_t start, size_t bytes);
void mem_write(void *src, uint16_t start, size_t bytes);