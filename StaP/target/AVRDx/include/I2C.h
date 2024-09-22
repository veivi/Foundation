/*
    \file   i2c.h

    \brief  TWI I2C Driver - header file

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

#ifndef I2C_H_INCLUDED
#define	I2C_H_INCLUDED

#include <xc.h>
#include <stdint.h>

void    I2C_0_Init(void);
int8_t I2C_0_Transfer(uint8_t device, StaP_TransferUnit_t *upSegment, size_t numSegments, uint8_t *downData, size_t downSize);
void    I2C_0_EndSession(void);
uint8_t I2C_0_Write(uint8_t device, const uint8_t *addr, size_t addrSize, const uint8_t *value, size_t valueSize);
uint8_t I2C_0_Read(uint8_t device, const uint8_t *addr, size_t addrSize, uint8_t *value, size_t valueSize);
uint8_t I2C_0_WriteRegisterUI8(uint8_t device, uint8_t addr, uint8_t value);
uint8_t I2C_0_WriteRegisterUI16(uint8_t device, uint8_t addr, uint16_t value);
uint8_t I2C_0_ReadRegister(uint8_t device, uint8_t addr, uint8_t *data, size_t size);

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif	/* I2C_H_INCLUDED */
