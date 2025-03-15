#include "PRNG.h"
#include "CRC16.h"

static uint16_t randomState;

void randomEntropyInput(uint16_t input)
{
    randomState = crc16_update(randomState, input);
}

void randomNumber(uint8_t *buffer, size_t size)
{
    while(size-- > 0) {
        randomState = crc16_update(randomState, 0xFF);
        *buffer++ = randomState & 0xFF;
    }
}

uint8_t randomUI8(void)
{
    uint8_t value = 0;
    randomNumber(&value, sizeof(value));
    return value;
}

uint16_t randomUI16(void)
{
    uint16_t value = 0;
    randomNumber((uint8_t*) &value, sizeof(value));
    return value;    
}
