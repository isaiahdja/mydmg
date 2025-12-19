#include "byte.h"
#include <stdint.h>
#include <stdbool.h>

bool dma_init(void);
void dma_tick(void);

bool dma_is_active(void);

byte dma_dma_read(void);
void dma_dma_write(byte val);