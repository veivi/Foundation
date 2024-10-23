#include "StaP.h"

#if STAP_USE_ADC

#include "ADC.h"
#include <avr/io.h>

/* This function initializes the ADC module */
void ADC0_init(void)
{
  /* Disable interrupt and digital input buffer on PD0 */
  PORTD.PIN0CTRL &= ~PORT_ISC_gm;
  PORTD.PIN0CTRL |= PORT_ISC_INPUT_DISABLE_gc;
    
  /* Disable pull-up resistor */
  PORTD.PIN0CTRL &= ~PORT_PULLUPEN_bm;

  VREF.ADC0REF = VREF_REFSEL_4V096_gc;  /* Internal 4.096V reference */

    ADC0.CTRLC = ADC_PRESC_DIV4_gc;        /* CLK_PER divided by 4 */    
    ADC0.CTRLA = ADC_ENABLE_bm             /* ADC Enable: enabled */
               | ADC_RESSEL_12BIT_gc      /* 12-bit mode */
               | ADC_LEFTADJ_bm;
    ADC0.MUXPOS = ADC_MUXPOS_AIN0_gc;      /* Select ADC channel AIN0 <-> PD0 */
}


/* This function returns the ADC conversion result */
uint16_t ADC0_read(void)
{
    // Start
    ADC0.COMMAND = ADC_STCONV_bm;
    
    /* Wait for ADC result to be ready */
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));
    
    /* Clear the interrupt flag by reading the result */
    return ADC0.RES;
}

uint16_t ADC0_readAverage(int sampleTime, int count)
{
    uint32_t adcSum = 0;
    int i = 0;

    for(i = 0; i < count; i++) {
        adcSum += (uint32_t) ADC0_read();
        vpDelayMillis(sampleTime);
	//        blinkTask();
    }
    
    return (uint16_t) (adcSum / count);
}

#endif
