/* 
 * File:   PRNG.h
 * Author: veivi
 *
 * Created on March 31, 2024, 11:39 AM
 */

#ifndef PRNG_H
#define	PRNG_H

#include <stddef.h>
#include <stdint.h>

void randomEntropyInput(uint16_t);
void randomNumber(uint8_t *buffer, size_t size);

uint8_t randomUI8(void);
uint16_t randomUI16(void);

#endif	/* PRNG_H */

