#include "byte.h"
#include <stdbool.h>

bool interrupt_init(void);

byte interrupt_if_read(void);
void interrupt_if_write(byte val);
byte interrupt_ie_read(void);
void interrupt_ie_write(byte val);