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

#include <avr/io.h>
#include <avr/interrupt.h>
#include "Timer.h"

/**
 * \brief Initialize tcb interface
 *
 * \return Initialization status.
 */

static TCB_cb_t ovf_cb, capt_cb;

void TCB_SetOvfIsrCallback(TCB_cb_t cb)
{
  ovf_cb = cb;
}

void TCB_SetCaptIsrCallback(TCB_cb_t cb)
{
  capt_cb = cb;
}

static void genericISR(TCB_t *instance)
{
	/* Insert your TCB interrupt handling code */

	/**
	 * The interrupt flag is cleared by writing 1 to it, or when the Capture register
	 * is read in Capture mode
	 */
	 if(instance->INTFLAGS & TCB_CAPT_bm)
        {
            if (capt_cb != NULL)
            {
                (*capt_cb)(instance);
            }

            instance->INTFLAGS = TCB_CAPT_bm;
        }

        /**
	 * The Overflow interrupt flag is cleared by writing 1 to it.
	 */
	 if(instance->INTFLAGS & TCB_OVF_bm)
        {
            if (ovf_cb != NULL)
            {
                (*ovf_cb)(instance);
            }

            instance->INTFLAGS = TCB_OVF_bm;
        }
	 
}

ISR(TCB1_INT_vect)
{
  genericISR(&TCB1);	 
}

/**
 * \brief Initialize TCB interface
 */
int8_t TCB_Initialize(TCB_t *instance)
{
    //Compare or Capture
  instance->CCMP = 0xFFFF;

    //Count
    instance->CNT = 0x00;

    //ASYNC disabled; CCMPINIT disabled; CCMPEN disabled; CNTMODE INT; 
    instance->CTRLB = 0x00;

    //DBGRUN disabled; 
    instance->DBGCTRL = 0x00;

    //FILTER disabled; EDGE disabled; CAPTEI disabled; 
    instance->EVCTRL = 0x00;

    //OVF disabled; CAPT enabled; 
    instance->INTCTRL = 0x01;

    //OVF disabled; CAPT disabled; 
    instance->INTFLAGS = 0x00;

    //Temporary Value
    instance->TEMP = 0x00;

    //RUNSTDBY enabled; CASCADE disabled; SYNCUPD disabled; CLKSEL DIV1; ENABLE enabled; 
    instance->CTRLA = 0x41;

    return 0;
}

void TCB_WriteTimer(TCB_t *instance, uint16_t timerVal)
{
	instance->CNT=timerVal;
}

uint16_t TCB_ReadTimer(TCB_t *instance)
{
	uint16_t readVal;

	readVal = instance->CNT;

	return readVal;
}

void TCB_EnableCaptInterrupt(TCB_t *instance)
{
	instance->INTCTRL |= TCB_CAPT_bm; /* Capture or Timeout: enabled */
}

void TCB_EnableOvfInterrupt(TCB_t *instance)
{
	instance->INTCTRL |= TCB_OVF_bm; /* Overflow Interrupt: enabled */
}

void TCB_DisableCaptInterrupt(TCB_t *instance)
{
	instance->INTCTRL &= ~TCB_CAPT_bm; /* Capture or Timeout: disabled */

}

void TCB_DisableOvfInterrupt(TCB_t *instance)
{
	instance->INTCTRL &= ~TCB_OVF_bm; /* Overflow Interrupt: disabled */

}

inline void TCB_ClearCaptInterruptFlag(TCB_t *instance)
{
	instance->INTFLAGS &= ~TCB_CAPT_bm;

}

inline void TCB_ClearOvfInterruptFlag(TCB_t *instance)
{
	instance->INTFLAGS &= ~TCB_OVF_bm;

}

inline bool TCB_IsCaptInterruptEnabled(TCB_t *instance)
{
        return ((instance->INTCTRL & TCB_CAPT_bm) > 0);
}

inline bool TCB_IsOvfInterruptEnabled(TCB_t *instance)
{
        return ((instance->INTCTRL & TCB_OVF_bm) > 0);
}
