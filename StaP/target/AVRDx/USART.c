#include "USART.h"
#include "StaP.h"

/* Compute the baud rate */
#define USART0_BAUD_RATE(r) (((float)F_CPU * 64 / (16 * (float)r)) + 0.5)

void USART_Init(volatile USART_t *hw, uint32_t rate, uint8_t mode)
{
  hw->CTRLC = USART_CHSIZE_8BIT_gc;
  USART_SetRate(hw, rate);
  USART_SetMode(hw, mode);
}

void USART_SetRate(volatile USART_t *hw, uint32_t rate)
{
    hw->BAUD = (uint16_t) USART0_BAUD_RATE(rate);
}

void USART_SetMode(volatile USART_t *hw, uint8_t mode)
{
  ForbidContext_T c = STAP_FORBID_SAFE;
  
  uint8_t ctrlA = hw->CTRLA, ctrlB = hw->CTRLB;

  if(mode & LINK_MODE_RX) {
    ctrlA |= USART_RXCIE_bm;
    ctrlB |= USART_RXEN_bm;
  } else {
    ctrlA &= ~USART_RXCIE_bm;
    ctrlB &= ~USART_RXEN_bm;
  }
  
  if(mode & LINK_MODE_TX)
    ctrlB |= USART_TXEN_bm;
  else
    ctrlB &= ~USART_TXEN_bm;
    
  hw->CTRLB = ctrlB;
  hw->CTRLA = ctrlA;

  STAP_PERMIT_SAFE(c);
}

/* This function transmits one byte through USART */
void USART_Transmit(volatile USART_t *hw, const uint8_t *data, uint8_t size)
{
  while(size-- > 0) {
    /* Check if USART buffer is ready to transmit data */
    while (!(hw->STATUS & USART_DREIF_bm));
    /* Transmit data using TXDATAL register */
    hw->TXDATAL = *data++;
  }
}

bool USART_Drain(volatile USART_t *hw, VP_TIME_MILLIS_T timeout)
{
  VP_TIME_MILLIS_T started = vpTimeMillisApprox;
  
  if(!(hw->CTRLB & USART_TXEN_bm))
    return false;

  // Wait until HW buffer is empty
  
  while(!(hw->STATUS & USART_DREIF_bm)) {
    if(VP_MILLIS_FINITE(timeout) && VP_ELAPSED_MILLIS(started) > timeout)
      return false;
  }
    
  // Wait until last frame is transmitted
  
  while(!(hw->STATUS & USART_TXCIF_bm)) {
    if(VP_MILLIS_FINITE(timeout) && VP_ELAPSED_MILLIS(started) > timeout)
      return false;
  }

  return true;
}

void USART_TransmitStart(volatile USART_t *hw, VPBuffer_t *buffer)
{
  ForbidContext_T c = STAP_FORBID_SAFE;

  if(VPBUFFER_GAUGE(*buffer) > 0 && !(hw->CTRLA & USART_DREIE_bm)) {
    if(hw->STATUS & USART_DREIF_bm)
      // Place first char in the buffer
      hw->TXDATAL = vpbuffer_extractChar(buffer);

    if(VPBUFFER_GAUGE(*buffer) > 0 && (hw->STATUS & USART_DREIF_bm))
      // The first went into the shft register, the buffer can accept
      // another
      
      hw->TXDATAL = vpbuffer_extractChar(buffer);

    if(VPBUFFER_GAUGE(*buffer) > 0)
      // The rest is transmitted on the interrupt
      hw->CTRLA |= USART_DREIE_bm;
  }

  STAP_PERMIT_SAFE(c);
}

void USART_TransmitWorker(volatile USART_t *hw, VPBuffer_t *buffer)
{
  if((hw->STATUS & USART_DREIF_bm) && VPBUFFER_GAUGE(*buffer) > 0)
    hw->TXDATAL = vpbuffer_extractChar(buffer);

  if(VPBUFFER_GAUGE(*buffer) == 0)
    hw->CTRLA &= ~USART_DREIE_bm;
}

void USART_ReceiveWorker(volatile USART_t *hw, VPBuffer_t *buffer)
{
  uint8_t flags = hw->RXDATAH;
  
  if(flags & USART_RXCIF_bm)
    vpbuffer_insertChar(buffer, hw->RXDATAL);
  
  if(flags & USART_BUFOVF_bm)
    STAP_Error(STAP_ERR_RX_OVERRUN_H);
}


