#ifndef STAP_CONFIG_H
#define STAP_CONFIG_H

//
// CONFIGURATION-SPECIFIC DEFINITIONS
//

#define STAP_USE_USART0_RX     1
#define STAP_USE_USART1_TX     1
#define STAP_USE_USART1_RX     1
#define STAP_USE_USART2_TX     1
#define STAP_USE_USART2_RX     1
#define STAP_USE_USART3_TX     1
#define STAP_USE_USART3_RX     1
#define STAP_USE_USART4_TX     1
#define STAP_USE_USART4_RX     1

#define STAP_NUM_LEDS    2

// #define STAP_USE_ICM40609 1
#define STAP_USE_I2C_0    1
#define STAP_USE_SPI_0    1
#define STAP_USE_PWMOUT   1

#define StaP_Activity(id)  activityBlink(id)
void activityBlink(uint8_t id);

//
// Debug settings
//

#define STAP_DEBUG_LEVEL   1
// #define ONLY_TASK_NAME "Blink"

//
// Serial interface link definitions
//

typedef enum { StaP_Link_Invalid,
	       ALP_Link_HostRX,
	       ALP_Link_HostTX,
	       ALP_Link_ALinkRX,
	       ALP_Link_ALinkTX,
	       ALP_Link_SRXLInA,
	       ALP_Link_SRXLOutA,
	       ALP_Link_SRXLInB,
	       ALP_Link_SRXLOutB,
	       ALP_Link_TelemRX,
	       ALP_Link_TelemTX,
	       ALP_Link_SRXLInA_TX,
	       ALP_Link_SRXLInB_TX,
	       ALP_Link_ALinkMon,
	       StaP_NumOfLinks } StaP_LinkId_T;

#define StaP_LinkDir(i) ((bool[StaP_NumOfLinks]) { false, false, true, false, true, false, true, false, true, false, true, true, true, false })[i]

#define STAP_NUM_UARTS     5

//
// Signal definition
//

typedef enum { StaP_Sig_Invalid = 0,
	       ALP_Sig_Control,
	       ALP_Sig_HostRX,
	       ALP_Sig_HostTX,
	       ALP_Sig_AlphaRX,
	       ALP_Sig_AlphaTX,
	       ALP_Sig_TelemRX,
	       ALP_Sig_TelemTX,
	       ALP_Sig_MonitorRX,
	       ALP_Sig_RX1,
	       ALP_Sig_RX1_TX,
	       ALP_Sig_RX2,
	       ALP_Sig_SRXL1,
	       ALP_Sig_SRXL2,
	       StaP_NumOfSignals } StaP_Signal_T;

//
// Default stack size
//

#define STAP_TASK_MIN_STACK    (1<<8)

//
// LED control
//

#define STAP_LED0_PORT    PORTF
#define STAP_LED0_MASK    (1<<3)
#define STAP_LED1_PORT    PORTF
#define STAP_LED1_MASK    (1<<2)

//
// SPI CS output
//

#define STAP_SPI_PORT        PORTA
#define STAP_SPI_MASK_MOSI   (1<<4)
#define STAP_SPI_MASK_CLK    (1<<6)

#define STAP_SPI_CS0_PORT    PORTA
#define STAP_SPI_CS0_MASK    (1<<7)


#endif
