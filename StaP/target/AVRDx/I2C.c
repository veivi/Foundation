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
    
    // PORTA.DIRCLR = 3<<2;
    
    // PORTA.PIN2CTRL |= PORT_PULLUPEN_bm;
    // PORTA.PIN3CTRL |= PORT_PULLUPEN_bm;

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

static bool timedOut = false;

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
            timedOut = true;
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
            timedOut = true;
        }

    } while(!state);
    
    return state;
}

/* Generic uni/bidirectional transfer */

int8_t I2C_0_Transfer(uint8_t device, const StaP_TransferUnit_t *upSegment, size_t numSegments, uint8_t *downData, size_t downSize)
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
            const uint8_t *upData = upSegment[i].data;

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

void I2C_0_EndSession(void)
{
    if(timedOut) {
        // TWI0.MCTRLB = TWI_FLUSH_bm;
        TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
    } else
        TWI0.MCTRLB = TWI_MCMD_STOP_gc;
    
    timedOut = false;
}

uint8_t I2C_0_Write(uint8_t device, const uint8_t *addr, size_t addrSize, const uint8_t *value, size_t valueSize)
{
    StaP_TransferUnit_t buffer[] = { 
        { addr, addrSize },
        { value, valueSize } 
    };

    return I2C_0_Transfer(device, buffer, sizeof(buffer)/sizeof(StaP_TransferUnit_t), NULL, 0);
}

uint8_t I2C_0_Read(uint8_t device, const uint8_t *addr, size_t addrSize, uint8_t *value, size_t valueSize)
{
    StaP_TransferUnit_t buffer = { addr, addrSize };
     
    if(addrSize > 0)
        return I2C_0_Transfer(device, &buffer, 1, value, valueSize);
    else
        return I2C_0_Transfer(device, NULL, 0, value, valueSize);
}

uint8_t I2C_0_WriteRegisterUI8(uint8_t device, uint8_t addr, uint8_t value)
{
   return I2C_0_Write(device, &addr, sizeof(addr), &value, sizeof(value));
}

uint8_t I2C_0_WriteRegisterUI16(uint8_t device, uint8_t addr, uint16_t value)
{
   return I2C_0_Write(device, &addr, sizeof(addr), (const uint8_t*) &value, sizeof(value));
}

uint8_t I2C_0_ReadRegister(uint8_t device, uint8_t addr, uint8_t *data, size_t size)
{
   return I2C_0_Read(device, &addr, sizeof(addr), data, size);
}
