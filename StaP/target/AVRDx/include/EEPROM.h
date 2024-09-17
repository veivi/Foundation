/* 
 * File:   EEPROM.h
 * Author: veivi
 *
 * Created on February 4, 2024, 2:46 PM
 */

#ifndef EEPROM_H
#define	EEPROM_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <avr/io.h>

void EEPROM_Read(uint16_t offset, uint8_t *data, size_t size);
void EEPROM_Write(uint16_t offset, const uint8_t *data, size_t size);

#ifdef	__cplusplus
}
#endif

#endif	/* EEPROM_H */

