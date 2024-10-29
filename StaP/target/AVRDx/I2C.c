/*
    \file   i2c.c

    \brief  TWI I2C Driver

    (c) 2020 Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip software and any
    derivatives exclusively with Microchip products. It is your responsibility to comply with third-party
    license terms applicable to your use of third-party software (including open source software) that
    may accompany Microchip software.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
    FOR A PARTICULAR PURPOSE.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
    SOFTWARE.
*/

#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "I2C.h"
#include "StaP.h"
#include "VPTime.h"

#ifdef TWI0_BAUD
#undef TWI0_BAUD
#endif

#define TWI0_BAUD(F_SCL, T_RISE)        (uint8_t)((((((float)F_CPU / (float)(F_SCL)) - 10 - (((float)(F_CPU) * (T_RISE))/1000000.0))) / 2))

#define I2C_SCL_FREQ                    400000  /* Frequency [Hz] */
#define TIMEOUT_MILLIS                  5

enum {
    I2C_INIT = 0, 
    I2C_ACKED,
    I2C_NACKED,
    I2C_READY,
    I2C_ERROR,
    I2C_TIMEOUT
};

void I2C_0_Init(void)
{
    /* Select I2C pins PA2/PA3 */
    PORTMUX.TWIROUTEA = 0x00;

    // PORTA.DIRSET = 3<<2;

    // PORTA.DIRCLR = 3<<2;
    
    // PORTA.PIN2CTRL |= PORT_PULLUPEN_bm;
    // PORTA.PIN3CTRL |= PORT_PULLUPEN_bm;

    /* Host Baud Rate Control */
    TWI0.MBAUD = TWI0_BAUD((I2C_SCL_FREQ), 0.3);
    
    /* Enable TWI */ 
    TWI0.MCTRLA = TWI_ENABLE_bm | (0x3<<2);  // Timeout 200 us
    
    /* Initialize the address register */
    TWI0.MADDR = 0x00;
    
    /* Initialize the data register */
    TWI0.MDATA = 0x00;
    
    /* Set bus state idle */
    TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
}

void I2C_0_EndSession(void)
{
    if(state == I2C_TIMEOUT) {
      TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
    } else
      TWI0.MCTRLB = TWI_MCMD_STOP_gc;
}

static uint8_t i2c_0_WaitW(void)
{
    VP_TIME_MILLIS_T started = vpTimeMillis();
    
    uint8_t state = I2C_INIT;
    do
    {
        if(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm))
        {
            if(!(TWI0.MSTATUS & TWI_RXACK_bm))
            {
                /* client responded with ack - TWI goes to M1 state */
                state = I2C_ACKED;
            }
            else
            {
                /* address sent but no ack received - TWI goes to M3 state */
                state = I2C_NACKED;
            }
        }
        else if(TWI0.MSTATUS & (TWI_BUSERR_bm | TWI_ARBLOST_bm))
        {
            /* get here only in case of bus error or arbitration lost - M4 state */
            state = I2C_ERROR;
        } else if(VP_ELAPSED_MILLIS(started) > TIMEOUT_MILLIS) {
            // Timed out
            state = I2C_TIMEOUT;
        }
    } while(!state);
    
    return state;
}

static uint8_t i2c_0_WaitR(void)
{
    VP_TIME_MILLIS_T started = vpTimeMillis();
    uint8_t state = I2C_INIT;
    
    do
    {
        if(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm))
        {
            state = I2C_READY;
        }
        else if(TWI0.MSTATUS & (TWI_BUSERR_bm | TWI_ARBLOST_bm))
        {
            /* get here only in case of bus error or arbitration lost - M4 state */
            state = I2C_ERROR;
            
        } else if(VP_ELAPSED_MILLIS(started) > TIMEOUT_MILLIS) {
            // Timed out
            state = I2C_TIMEOUT;
        }

    } while(!state);
    
    return state;
}

#if 0
/* Generic uni/bidirectional transfer */

static uint8_t I2C_0_TransferOld(uint8_t device, const StaP_TransferUnit_t *upSegment, size_t numSegments, uint8_t *downData, size_t downSize)
{
    uint8_t status = 0x00;
    int i = 0;
    
    if(numSegments > 0) {
        /* start transmitting the client device */
        TWI0.MADDR = device<<1;
        if(i2c_0_WaitW() != I2C_ACKED)
            status = 0xE0;

        for(i = 0; i < numSegments; i++) {
            size_t upSize = upSegment[i].size;
            const uint8_t *upData = upSegment[i].data.tx;

            if(!status && (upSize > 0) && (upData != NULL)) {
                while(upSize-- > 0) {
                    TWI0.MDATA = *upData++;
            
                    if(i2c_0_WaitW() != I2C_ACKED) {
                        status = 0xE1;
                        break;
                    }
                }
            }
        }

        if(!status)
            I2C_0_EndSession();    
    }

    if(!status && downSize > 0 && downData != NULL) {
        /* start transmitting the client address */
        TWI0.MADDR = (device<<1) | 0x01;
        if(i2c_0_WaitW() != I2C_ACKED)
            status = 0xF1;
        else {
            while(downSize-- > 0) {
                if(i2c_0_WaitR() == I2C_READY) {
                   *downData++ = TWI0.MDATA;
                    TWI0.MCTRLB = (downSize == 0)? TWI_ACKACT_bm | TWI_MCMD_STOP_gc : TWI_MCMD_RECVTRANS_gc;
                } else {
                    status = 0xF1;
                    break;
                }
            }
        }
    }

    if(status)
        I2C_0_EndSession();    
        
    return status;
}

#endif

//
// Even more generic transfer! 
//   LIMITATIONS:
//     1) All transmits must occur before any receives
//     2) NULL receives are not allowed (null transmits are)
//

uint8_t I2C_0_Transfer(uint8_t device, const StaP_TransferUnit_t *segment, size_t numSegments)
{
  bool transmitting = false, receiving = false;
  int i = 0;

#if 0
  while(i < numSegments && segment[i].dir == transfer_dir_transmit)
    i++;

  if(i < numSegments)
    I2C_0_TransferOld(device, segment, i, segment[i].data.rx, segment[i].size);
  else
    I2C_0_TransferOld(device, segment, numSegments, NULL, 0);

  return;
#endif
  
  for(i = 0; i < numSegments; i++) {
    if(segment[i].dir == transfer_dir_transmit) {
      // Transmit
      
      size_t upSize = segment[i].size;
      const uint8_t *upData = (const uint8_t*) segment[i].data.tx;

      if(upSize > 0 && upData != NULL) {
        if(!transmitting) {
          /* start transmitting the client device */
          TWI0.MADDR = device<<1;

          if(i2c_0_WaitW() != I2C_ACKED) {
            I2C_0_EndSession();    
            return 0xE0;
          }
         
          transmitting = true;
        }
       
        while(upSize-- > 0) {
          TWI0.MDATA = *upData++;
            
          if(i2c_0_WaitW() != I2C_ACKED) {
            I2C_0_EndSession();    
            return 0xE1;
          }
        }
      }        
    } else {
      // Receive
      
      size_t downSize = segment[i].size;
      uint8_t *downData = segment[i].data.rx;

      if(!downSize || !downData)
        STAP_Panicf(STAP_ERR_I2C, "NULL data for reception");

      if(!receiving) {
	if(transmitting) {
	  I2C_0_EndSession();    
	  transmitting = false;        
	}
	
        /* start transmitting the client address */
        TWI0.MADDR = (device<<1) | 0x01;
        if(i2c_0_WaitW() != I2C_ACKED) {
            I2C_0_EndSession();    
            return 0xF0;
        }

        receiving = true;
      }
          
      while(downSize-- > 0) {
        if(i2c_0_WaitR() == I2C_READY) {
          *downData++ = TWI0.MDATA;
          TWI0.MCTRLB = (downSize == 0 && i == numSegments-1) ? TWI_ACKACT_bm | TWI_MCMD_STOP_gc : TWI_MCMD_RECVTRANS_gc;
        } else {
          I2C_0_EndSession();    
          return 0xF1;
        }
      }
    }
  }

  if(transmitting)
    I2C_0_EndSession();    
        
  return 0;
}

#if 0
/*
    \file   i2c.c

    \brief  TWI I2C Driver

    (c) 2020 Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip software and any
    derivatives exclusively with Microchip products. It is your responsibility to comply with third-party
    license terms applicable to your use of third-party software (including open source software) that
    may accompany Microchip software.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
    FOR A PARTICULAR PURPOSE.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
    SOFTWARE.
*/

#include "clkctrl.h"
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "i2c.h"

#ifdef TWI0_BAUD
#undef TWI0_BAUD
#endif

#define TWI0_BAUD(F_SCL, T_RISE)        (uint8_t)((((((float)F_CPU / (float)(F_SCL)) - 10 - (((float)(F_CPU) * (T_RISE))/1000000.0))) / 2))

#define I2C_SCL_FREQ                    100000  /* Frequency [Hz] */

enum {
    I2C_INIT = 0, 
    I2C_ACKED,
    I2C_NACKED,
    I2C_READY,
    I2C_ERROR
};

void I2C_0_Init(void)
{
    /* Select I2C pins PC2/PC3 */
    PORTMUX.TWIROUTEA = 0x02;
    
    /* Host Baud Rate Control */
    TWI0.MBAUD = TWI0_BAUD((I2C_SCL_FREQ), 0.3);
    
    /* Enable TWI */ 
    TWI0.MCTRLA = TWI_ENABLE_bm;
    
    /* Initialize the address register */
    TWI0.MADDR = 0x00;
    
    /* Initialize the data register */
    TWI0.MDATA = 0x00;
    
    /* Set bus state idle */
    TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
}

static uint8_t i2c_0_WaitW(void)
{
    uint8_t state = I2C_INIT;
    do
    {
        if(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm))
        {
            if(!(TWI0.MSTATUS & TWI_RXACK_bm))
            {
                /* client responded with ack - TWI goes to M1 state */
                state = I2C_ACKED;
            }
            else
            {
                /* address sent but no ack received - TWI goes to M3 state */
                state = I2C_NACKED;
            }
        }
        else if(TWI0.MSTATUS & (TWI_BUSERR_bm | TWI_ARBLOST_bm))
        {
            /* get here only in case of bus error or arbitration lost - M4 state */
            state = I2C_ERROR;
        }
    } while(!state);
    
    return state;
}

static uint8_t i2c_0_WaitR(void)
{
    uint8_t state = I2C_INIT;
    
    do
    {
        if(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm))
        {
            state = I2C_READY;
        }
        else if(TWI0.MSTATUS & (TWI_BUSERR_bm | TWI_ARBLOST_bm))
        {
            /* get here only in case of bus error or arbitration lost - M4 state */
            state = I2C_ERROR;
        }
    } while(!state);
    
    return state;
}

 /* Returns how many bytes have been sent, -1 means NACK at address, 0 means client ACKed to client address */
uint8_t I2C_0_SendData(uint8_t address, uint8_t *pData, uint8_t len)
{
    uint8_t retVal = (uint8_t) - 1;
    
    /* start transmitting the client address */
    TWI0.MADDR = address & ~0x01;
    if(i2c_0_WaitW() != I2C_ACKED)
        return retVal;

    retVal = 0;
    if((len != 0) && (pData != NULL))
    {
        while(len--)
        {
            TWI0.MDATA = *pData;
            if(i2c_0_WaitW() == I2C_ACKED)
            {
                retVal++;
                pData++;
                continue;
            }
            else // did not get ACK after client address
            {
                break;
            }
        }
    }
    
    return retVal;
}

/* Returns how many bytes have been received, -1 means NACK at address */
uint8_t I2C_0_GetData(uint8_t address, uint8_t *pData, uint8_t len)
{
    uint8_t retVal = (uint8_t) - 1;
    
    /* start transmitting the client address */
    TWI0.MADDR = address | 0x01;
    if(i2c_0_WaitW() != I2C_ACKED)
        return retVal;

    retVal = 0;
    if((len != 0) && (pData !=NULL ))
    {
        while(len--)
        {
            if(i2c_0_WaitR() == I2C_READY)
            {
               *pData = TWI0.MDATA;
                TWI0.MCTRLB = (len == 0)? TWI_ACKACT_bm | TWI_MCMD_STOP_gc : TWI_MCMD_RECVTRANS_gc;
                retVal++;
                pData++;
                continue;
            }
            else
                break;
        }
    }
    
    return retVal;
}

void I2C_0_EndSession(void)
{
    TWI0.MCTRLB = TWI_MCMD_STOP_gc;
}

#endif
