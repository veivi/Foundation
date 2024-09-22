/**
  @Company
    Microchip Technology Inc.

  @Description
    This Source file provides APIs.
    Generation Information :
    Driver Version    :   1.0.0
*/
/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
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


#ifndef TCB0_H_INCLUDED
#define TCB0_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

typedef void (*TCB_cb_t)(TCB_t*);
void TCB_SetCaptIsrCallback(TCB_cb_t cb);
void TCB_SetOvfIsrCallback(TCB_cb_t cb);
int8_t TCB_Initialize(TCB_t*);
void TCB_EnableCaptInterrupt(TCB_t*);
void TCB_DisableCaptInterrupt(TCB_t*);
void TCB_EnableOvfInterrupt(TCB_t*);
void TCB_DisableOvfInterrupt(TCB_t*);
uint16_t TCB_ReadTimer(TCB_t*);
void TCB_WriteTimer(TCB_t*, uint16_t timerVal);
void TCB_ClearCaptInterruptFlag(TCB_t*);
bool TCB_IsCaptInterruptEnabled(TCB_t*);
void TCB_ClearOvfInterruptFlag(TCB_t*);
bool TCB_IsOvfInterruptEnabled(TCB_t*);

#ifdef __cplusplus
}
#endif

#endif /* TCB0_H_INCLUDED */
