#ifndef STAP_CONFIG_H
#define STAP_CONFIG_H

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

#define StaP_Activity(id)  activityBlink(id)
void activityBlink(uint8_t id);

//
// Debug settings
//

#define STAP_DEBUG_LEVEL   1

//
// FreeRTOS config
//

// #define ONLY_TASK_NAME "Blink"
#define STAP_TASK_MIN_STACK    (1<<7)

//
// Serial interface link definitions
//

typedef enum { StaP_Link_Invalid = 0,
	       GS_Link_AuxRX,
	       GS_Link_AuxTX,
	       GS_Link_HostRX,
	       GS_Link_HostTX,
	       GS_Link_RadioRX,
	       GS_Link_RadioTX,
	       GS_Link_GNSSRX,
	       GS_Link_GNSSTX,
	       GS_Link_BTRX,
	       StaP_NumOfLinks } StaP_LinkId_T;

#define StaP_LinkDir(i) ((bool[StaP_NumOfLinks]) { false, false, true, false, true, false, true, false, true, false })[i]

//
// Signal definition
//

typedef enum { StaP_Sig_Invalid = 0,
	       GS_SIG_ACTIVITY,
	       GS_SIG_RESPONSE,
	       GS_SIG_HOSTRX,
	       GS_SIG_HOSTTX,
	       GS_SIG_GNSSRX,
	       GS_SIG_GNSSTX,
	       GS_SIG_BTRX,
	       GS_SIG_BTTX,
	       GS_SIG_AUXRX,
	       GS_SIG_AUXTX,
	       GS_SIG_RADIORX,
	       GS_SIG_RADIOTX,
	       StaP_NumOfSignals } StaP_Signal_T;

//
// LED pins
//

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

//
// SPI device selection
//

#define ALP_DEV_GYRO   0

#endif
