#include <SDL3/SDL.h>

#define DEBUG true

extern const int gb_width;
extern const int gb_height;

void rom_copy(void *dst, size_t start, size_t size);