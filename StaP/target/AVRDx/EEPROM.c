#include "EEPROM.h"
#include <string.h>
#include <avr/eeprom.h>

void EEPROM_Read(uint16_t offset, uint8_t *data, size_t size)
{
  // eeprom_read_block(data, (void*) offset, size);
}

void EEPROM_Write(uint16_t offset, const uint8_t *data, size_t size)
{
  // eeprom_write_block(data, (void*) offset, size);
}
