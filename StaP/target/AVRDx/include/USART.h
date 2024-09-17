/* 
 * File:   USART.h
 * Author: veivi
 *
 * Created on January 30, 2024, 5:31 PM
 */

#ifndef USART_H
#define	USART_H

#include <stdint.h>
#include <avr/io.h>
#include "Buffer.h"
#include "VPTime.h"

extern USART_t *USART_Table[];

void USART_Init(volatile USART_t *hw, uint32_t rate, uint8_t mode);
void USART_SetRate(volatile USART_t *hw, uint32_t rate);
void USART_SetMode(volatile USART_t *hw, uint8_t mode);
void USART_Transmit(volatile USART_t *hw, const uint8_t *data, uint8_t size);    
void USART_TransmitStart(volatile USART_t *hw, VPBuffer_t *buffer);   
void USART_ReceiveWorker(volatile USART_t *hw, VPBuffer_t *buffer);    
void USART_TransmitWorker(volatile USART_t *hw, VPBuffer_t *buffer);    
bool USART_Drain(volatile USART_t *hw, VP_TIME_MILLIS_T timeout);

#define USART_TransmitWorker(hw, buffer) \
  if(VPBUFFER_GAUGE(buffer))				\
  hw->TXDATAL = vpbuffer_extractChar(&buffer);	\
  if(!VPBUFFER_GAUGE(buffer))				\
    hw->CTRLA &= ~USART_DREIE_bm

#define USART_ReceiveWorker(hw, buffer) \
  { uint8_t flags = hw->RXDATAH;	\
    if(flags & USART_RXCIF_bm) \
      vpbuffer_insertChar(&buffer, hw->RXDATAL); \
    if(flags & USART_BUFOVF_bm) STAP_Error(STAP_ERR_RX_OVERRUN_H); } \


#endif	/* USART_H */

