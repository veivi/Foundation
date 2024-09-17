#include <stdlib.h>
#include <avr/io.h>
#include "SPI.h"
#include "StaP.h"

typedef struct {
  PORT_t *port;
  uint8_t mask;
} CSDef_t;

CSDef_t csTable[] = {
#ifdef STAP_SPI_CS0_PORT
  { &STAP_SPI_CS0_PORT,
    STAP_SPI_CS0_MASK }
#endif
#ifdef STAP_SPI_CS1_PORT
  ,{ &STAP_SPI_CS1_PORT,
    STAP_SPI_CS1_MASK }
#endif
#ifdef STAP_SPI_CS2_PORT
  , { &STAP_SPI_CS2_PORT,
      STAP_SPI_CS2_MASK }
#endif
};

static void SPI_Select(uint8_t device)
{
  csTable[device].port->OUTCLR = csTable[device].mask;
}
  
static void SPI_Deselect(uint8_t device)
{
  csTable[device].port->OUTSET = csTable[device].mask;
}
  
//#define ALP_GYRO_SELECT      STAP_SPI_CS0_PORT.OUTCLR = STAP_SPI_CS0_MASK
//#define ALP_GYRO_DESELECT    STAP_SPI_CS0_PORT.OUTSET = STAP_SPI_CS0_MASK

void SPI_Initialize(SPI_t *hw)
{
  // CTRLA: MASTER enabled; CLK2X enabled; PRESC DIV4; ENABLE enabled; 
  SPI0.CTRLA = (1<<5) | (1<<4) | (1<<0);

  // CTRLB: BUFEN disabled; BUFWR disabled; SSD disabled; MODE 0; 
  SPI0.CTRLB = 0x00;

  // INTCTRL: all disabled
  hw->INTCTRL = 0x00;
    
  // INTFLAGS: cleared
  hw->INTFLAGS = 0x00;
}

void SPI_Enable(SPI_t *hw)
{
    hw->CTRLA |= SPI_ENABLE_bm;
}

void SPI_Disable(SPI_t *hw)
{
    hw->CTRLA &= ~SPI_ENABLE_bm;
}

uint8_t SPI_ExchangeByte(SPI_t *hw, uint8_t data)
{
    hw->DATA = data;
    while (!(hw->INTFLAGS & SPI_RXCIF_bm));
    return hw->DATA;
}

bool SPI_Selected()
{
/**
 * \brief returns true if SS pin is selected 
 * TODO: Place your code
 */
return true;
}

uint8_t SPI_GetRxData(SPI_t *hw)
{
    return hw->DATA;
}

void SPI_WriteTxData(SPI_t *hw, uint8_t data)
{
    hw->DATA = data;
}

void SPI_WaitDataready(SPI_t *hw)
{
    while (!(hw->INTFLAGS & SPI_RXCIF_bm))
        ;
}

void SPI_ExchangeBlock(SPI_t *hw, void *block, size_t size)
{
    uint8_t *b = (uint8_t *)block;
    while (size--) {
        hw->DATA = *b;
        while (!(hw->INTFLAGS & SPI_RXCIF_bm))
            ;
        *b = hw->DATA;
        b++;
    }
}

void SPI_WriteBlock(SPI_t *hw, void *block, size_t size)
{
    uint8_t *b = (uint8_t *)block;
    uint8_t  rdata;
    while (size--) {
        hw->DATA = *b;
        while (!(hw->INTFLAGS & SPI_RXCIF_bm))
            ;
        rdata = hw->DATA;
        (void)(rdata); // Silence compiler warning
        b++;
    }
}

void SPI_ReadBlock(SPI_t *hw, void *block, size_t size)
{
    uint8_t *b = (uint8_t *)block;
    while (size--) {
        hw->DATA = 0;
        while (!(hw->INTFLAGS & SPI_RXCIF_bm))
            ;
        *b = hw->DATA;
        b++;
    }
}

void SPI_WriteByte(SPI_t *hw, uint8_t data)
{

    hw->DATA = data;
}

uint8_t SPI_ReadByte(SPI_t *hw)
{
    return hw->DATA;
}

void SPI_Transfer(SPI_t *hw, uint8_t device, SPI_TransferUnit_T *transfer, uint8_t num)
{
  uint8_t *buffer = NULL, size = 0;
  int i = 0;

  SPI_Select(device);
  
  while(i < num) {
    if(!buffer) {
      buffer = transfer[i].buffer;
      size = transfer[i].size;
    }
    
    if(size > 0) {
      if(transfer[i].dir == SPI_DIR_TX)
        hw->DATA = *buffer;
      else
        hw->DATA = 0x0;
	
      while (!(hw->INTFLAGS & SPI_RXCIF_bm))
	;
	
      if(transfer[i].dir == SPI_DIR_RX)
	*buffer = hw->DATA;
      else
	(void) hw->DATA;

      buffer++;
      size--;
    } else {
      i++;
      buffer = NULL;
    }
  }

  SPI_Deselect(device);
}

void SPI_ReadWithUInt8(SPI_t *hw, uint8_t device, uint8_t addr, uint8_t *data, size_t size)
{
  SPI_TransferUnit_T transfer[] = {
    { SPI_DIR_TX, sizeof(addr), &addr },
    { SPI_DIR_RX, size, data } };
  
  SPI_Transfer(hw, device, transfer, sizeof(transfer)/sizeof(SPI_TransferUnit_T));
}

void SPI_ReadWithUInt16(SPI_t *hw, uint8_t device, uint16_t addr, uint8_t *data, size_t size)
{
  SPI_TransferUnit_T transfer[] = {
    { SPI_DIR_TX, sizeof(addr), &addr },
    { SPI_DIR_RX, size, data } };
  
  SPI_Transfer(hw, device, transfer, sizeof(transfer)/sizeof(SPI_TransferUnit_T));
}

void SPI_WriteWithUInt8(SPI_t *hw, uint8_t device, uint8_t addr, const uint8_t *data, size_t size)
{
  SPI_TransferUnit_T transfer[] = {
    { SPI_DIR_TX, sizeof(addr), &addr },
    { SPI_DIR_TX, size, (uint8_t*) data } };
  
  SPI_Transfer(hw, device, transfer, sizeof(transfer)/sizeof(SPI_TransferUnit_T));
}

void SPI_WriteWithUInt16(SPI_t *hw, uint8_t device, uint16_t addr, const uint8_t *data, size_t size)
{
  SPI_TransferUnit_T transfer[] = {
    { SPI_DIR_TX, sizeof(addr), &addr },
    { SPI_DIR_TX, size, (uint8_t*) data } };

  SPI_Transfer(hw, device, transfer, sizeof(transfer)/sizeof(SPI_TransferUnit_T));
}

uint8_t SPI_ReadUInt8WithUInt8(SPI_t *hw, uint8_t device, uint8_t addr)
{
  uint8_t buffer = 0;

  SPI_ReadWithUInt8(hw, device, addr, &buffer, sizeof(buffer));

  return buffer;
}

uint16_t SPI_ReadUInt16WithUInt8(SPI_t *hw, uint8_t device, uint8_t addr)
{
  uint16_t buffer = 0;

  SPI_ReadWithUInt8(hw, device, addr, &buffer, sizeof(buffer));

  return buffer;
}

uint16_t SPI_ReadUInt16BEWithUInt8(SPI_t *hw, uint8_t device, uint8_t addr)
{
  uint16_t buffer = 0;

  SPI_ReadWithUInt8(hw, device, addr, &buffer, sizeof(buffer));

  return ((buffer & 0xFF)<<8) | (buffer>>8);
}

void SPI_WriteUInt8WithUInt8(SPI_t *hw, uint8_t device, uint8_t addr, uint8_t value)
{
  SPI_WriteWithUInt8(hw, device, addr, &value, sizeof(value));
}

void SPI_WriteUInt16WithUInt8(SPI_t *hw, uint8_t device, uint8_t addr, uint16_t value)
{
  SPI_WriteWithUInt8(hw, device, addr, &value, sizeof(value));
}





