#include "ADC.h"
#include <avr/io.h>

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

