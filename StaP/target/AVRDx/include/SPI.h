#ifndef STAP_SPI_H
#define STAP_SPI_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { SPI_DIR_TX, SPI_DIR_RX } SPI_DIR_T;

typedef struct {
  SPI_DIR_T dir;
  uint8_t size;
  uint8_t *buffer;
} SPI_TransferUnit_T;

void SPI_Transfer(SPI_t *hw, uint8_t, SPI_TransferUnit_T *transfer, uint8_t num);

void SPI_ReadWithUInt8(SPI_t *hw, uint8_t, uint8_t addr, uint8_t *data, size_t size);
void SPI_ReadWithUInt16(SPI_t *hw, uint8_t, uint16_t addr, uint8_t *data, size_t size);
void SPI_WriteWithUInt8(SPI_t *hw, uint8_t, uint8_t addr, const uint8_t *data, size_t size);
void SPI_WriteWithUInt16(SPI_t *hw, uint8_t, uint16_t addr, const uint8_t *data, size_t size);
uint8_t SPI_ReadUInt8WithUInt8(SPI_t *hw, uint8_t, uint8_t addr);
void SPI_WriteUInt8WithUInt8(SPI_t *hw, uint8_t, uint8_t addr, uint8_t value);
uint16_t SPI_ReadUInt16WithUInt8(SPI_t *hw, uint8_t, uint8_t addr);
uint16_t SPI_ReadUInt16BEWithUInt8(SPI_t *hw, uint8_t, uint8_t addr);
void SPI_WriteUInt16WithUInt8(SPI_t *hw, uint8_t, uint8_t addr, uint16_t value);

#endif
