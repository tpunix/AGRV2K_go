
#ifndef _AGM_AGRV_H
#define _AGM_AGRV_H

#include <inttypes.h>

/******************************************************************************/

typedef struct
{
	uint32_t BOOT_MODE;     // 0x00, Boot mode from BOOT0 and BOOT1 pins
	uint32_t RST_CNTL;      // 0x04, Reset control register
	uint32_t PWR_CNTL;      // 0x08, Power control register
	uint32_t CLK_CNTL;      // 0x0C, Clock control register
	uint32_t BUS_CNTL;      // 0x10, Bus control register
	uint32_t SWJ_CNTL;      // 0x14, SWJ pin control register
	uint32_t MISC_CNTL;     // 0x18, Miscellaneous control register
	uint32_t DBG_CNTL;      // 0x1C, Debug control register
	uint32_t WKP_RISE_TRG;  // 0x20, Wake up rise triggers
	uint32_t WKP_FALL_TRG;  // 0x24, Wake up fall triggers
	uint32_t WKP_PENDING;   // 0x28, Wake up pending register
	uint32_t RESERVED0;     // 0x2C
	uint32_t MTIME_PSC;     // 0x30, Mtime prescaler value
	uint32_t MTIME_COUNTER; // 0x34, Mtime prescaler counter
	uint32_t PBUS_DIVIDER;  // 0x38, Pbus clock divider
	uint32_t RESERVED1;     // 0x3C
	uint32_t APB_RESET;     // 0x40, APB peripheral reset
	uint32_t RESERVED2[3];  // 0x44-0x4C
	uint32_t AHB_RESET;     // 0x50, AHB peripheral reset
	uint32_t RESERVED3[3];  // 0x54-0x5C
	uint32_t APB_CLKENABLE; // 0x60, APB peripheral clock enable
	uint32_t RESERVED4[3];  // 0x64-0x6C
	uint32_t AHB_CLKENABLE; // 0x70, AHB peripheral clock enable
	uint32_t RESERVED5[3];  // 0x74-0x7C
	uint32_t APB_CLKSTOP;   // 0x80, APB peripheral clock stop during debug
	uint32_t RESERVED6[31]; // 0x84-0xFC
	uint32_t DEVICE_ID;     // 0x100, Device ID code
} SYS_ControlTypeDef;


#define SYSTEMCTRL_BASE (0x03000000)
#define SYS ((volatile SYS_ControlTypeDef *) SYSTEMCTRL_BASE)

// AHB:    3    2    1    0
//       MAC  CRC  USB  DMA
// APB:                   28    27    26    25    24    23    22    21    20    19    18    17    16
//                       I2C1  I2C0  CAN0 UART4 UART3 UART2 UART1 UART0 GTIM4 GTIM3 GTIM2 GTIM1 GTIM0
//       15   14    13    12    11    10    9     8     7     6     5     4     3     2     1     0
//      TIM1 TIM0 GPIO9 GPIO8 GPIO7 GPIO6 GPIO5 GPIO4 GPIO3 GPIO2 GPIO1 GPIO0  SPI1  SPI0  WDG   FCB 

/******************************************************************************/

#define DMAC_CHANNELS 8

typedef struct
{
	uint32_t SrcAddr;
	uint32_t DstAddr;
	uint32_t LLI;
	uint32_t Control;
} DMAC_LLI_TypeDef;


typedef struct
{
	uint32_t SrcAddr;       // 0x00, Channel Source Address Register
	uint32_t DstAddr;       // 0x04, Channel Destination Address Register
	uint32_t LLI;           // 0x08, Channel Linked List Item Register
	uint32_t Control;       // 0x0C, Channel Control Register
	uint32_t Configuration; // 0x10, Channel Configuration Register
	uint32_t Reserved[3];   // 0x14-0x1C
} DMAC_ChannelTypeDef;

typedef volatile struct
{
	uint32_t IntStatus;         // 0x000, Interrupt Status Register
	uint32_t IntTCStatus;       // 0x004, Interrupt Terminal Count Status Register
	uint32_t IntTCClear;        // 0x008, Interrupt Terminal Count Clear Register
	uint32_t IntErrorStatus;    // 0x00C, Interrupt Error Status Register
	uint32_t IntErrorClear;     // 0x010, Interrupt Error Clear Register
	uint32_t RawIntTCStatus;    // 0x014, Raw Interrupt Terminal Count Status Register
	uint32_t RawIntErrorStatus; // 0x018, Raw Error Interrupt Status Register
	uint32_t EnabledChannels;   // 0x01C, Enabled Channel Register
	uint32_t SoftBReq;          // 0x020, Software Burst Request Register
	uint32_t SoftSReq;          // 0x024, Software Single Request Register
	uint32_t SoftLBReq;         // 0x028, Software Last Burst Request Register
	uint32_t SoftSBReq;         // 0x02C, Software Last Single Request Register
	uint32_t Configuration;     // 0x030, Configuration Register
	uint32_t Sync;              // 0x034, Synchronization Register
	uint32_t Reserved[(0x100 - 0x038) >> 2];
	DMAC_ChannelTypeDef Channels[DMAC_CHANNELS]; // 0x100, Channel Registers
} DMAC_TypeDef;


#define DMAC0_BASE 0x41000000
#define DMAC ((volatile DMAC_TypeDef *) DMAC0_BASE)
#define AHB_MASK_DMAC0 (1 << 0)
#define DMAC0_INTR_IRQn 32
#define DMAC0_INTTC_IRQn 33
#define DMAC0_INTERR_IRQn 34


/******************************************************************************/

typedef struct
{
	uint32_t CTRL; // 0x00
	uint32_t ADDR; // 0x04
	uint32_t DATA; // 0x08
	uint32_t AUTO; // 0x0c
	uint32_t STAT; // 0x10
	uint32_t INT;  // 0x14
} FCB_TypeDef;


#define FCB0_BASE 0x40010000
#define FCB ((volatile FCB_TypeDef *) FCB0_BASE)
#define APB_MASK_FCB0 (1 << 0)
#define FCB0_DMA_REQ 5
#define FCB0_IRQn 3


/******************************************************************************/

typedef struct
{
	uint32_t RESERVED0; // 0x00
	uint32_t KEYR;      // 0x04, Key register
	uint32_t OPTKEYR;   // 0x08, Option byte key register
	uint32_t SR;        // 0x0c, Status register
	uint32_t CR;        // 0x10, Control register
	uint32_t AR;        // 0x14, Address register
	uint32_t RESERVED1; // 0x18
	uint32_t OBR;       // 0x1c, Option byte register
	uint32_t WRPR;      // 0x20, Write protection register
	uint32_t CONFIG;    // 0x24, Configuration register
	uint32_t DMA_DATA;  // 0x28
	uint32_t READ_CTRL; // 0x2c
	uint32_t READ_DATA; // 0x30
} FLASH_TypeDef;

#define FLASH ((volatile FLASH_TypeDef *) 0x40001000)

#define FLASH_MAX_FREQ     (100000000) // 100MHz

/******************************************************************************/

typedef struct
{
	uint32_t DATA[256]; // 0x00-0x3FC, Data register
	uint32_t DIR;       // 0x400, Data direction register
	uint32_t IS;        // 0x404, Interrupt sense register
	uint32_t IBE;       // 0x408, Interrupt both-edges register
	uint32_t IEV;       // 0x40C, Interrupt event register
	uint32_t IE;        // 0x410, Interrupt mask register
	uint32_t RIS;       // 0x414, Raw interrupt status register
	uint32_t MIS;       // 0x418, Masked interrupt status register
	uint32_t IC;        // 0x41C, Interrupt clear register
	uint32_t AFSEL;     // 0x420, Mode control select register
} GPIO_TypeDef;

#define GPIO0_BASE 0x40014000
#define GPIO0 ((volatile GPIO_TypeDef *) GPIO0_BASE)
#define APB_MASK_GPIO0 (1 << 4)
#define GPIO0_IRQn 7

#define GPIO1_BASE 0x40015000
#define GPIO1 ((volatile GPIO_TypeDef *) GPIO1_BASE)
#define APB_MASK_GPIO1 (1 << 5)
#define GPIO1_IRQn 8

#define GPIO2_BASE 0x40016000
#define GPIO2 ((volatile GPIO_TypeDef *) GPIO2_BASE)
#define APB_MASK_GPIO2 (1 << 6)
#define GPIO2_IRQn 9

#define GPIO3_BASE 0x40017000
#define GPIO3 ((volatile GPIO_TypeDef *) GPIO3_BASE)
#define APB_MASK_GPIO3 (1 << 7)
#define GPIO3_IRQn 10

#define GPIO4_BASE 0x40018000
#define GPIO4 ((volatile GPIO_TypeDef *) GPIO4_BASE)
#define APB_MASK_GPIO4 (1 << 8)
#define GPIO4_IRQn 11

#define GPIO5_BASE 0x40019000
#define GPIO5 ((volatile GPIO_TypeDef *) GPIO5_BASE)
#define APB_MASK_GPIO5 (1 << 9)
#define GPIO5_IRQn 12

#define GPIO6_BASE 0x4001A000
#define GPIO6 ((volatile GPIO_TypeDef *) GPIO6_BASE)
#define APB_MASK_GPIO6 (1 << 10)
#define GPIO6_IRQn 13

#define GPIO7_BASE 0x4001B000
#define GPIO7 ((volatile GPIO_TypeDef *) GPIO7_BASE)
#define APB_MASK_GPIO7 (1 << 11)
#define GPIO7_IRQn 14

#define GPIO8_BASE 0x4001C000
#define GPIO8 ((volatile GPIO_TypeDef *) GPIO8_BASE)
#define APB_MASK_GPIO8 (1 << 12)
#define GPIO8_IRQn 15

#define GPIO9_BASE 0x4001D000
#define GPIO9 ((volatile GPIO_TypeDef *) GPIO9_BASE)
#define APB_MASK_GPIO9 (1 << 13)
#define GPIO9_IRQn 16


/******************************************************************************/


typedef struct
{
	uint32_t CR1;   // 0x00, Control register 1
	uint32_t CR2;   // 0x04, Control register 2
	uint32_t SMCR;  // 0x08, Slave mode control register
	uint32_t DIER;  // 0x0C, DMA/interrupt enable register
	uint32_t SR;    // 0x10, Status register
	uint32_t EGR;   // 0x14, Event generation register
	uint32_t CCMR0; // 0x18, Capture/compare mode register 0
	uint32_t CCMR1; // 0x1C, Capture/compare mode register 1
	uint32_t CCER;  // 0x20, Capture/compare enable register
	uint32_t CNT;   // 0x24, Counter
	uint32_t PSC;   // 0x28, Prescaler
	uint32_t ARR;   // 0x2C, Auto reload register
	uint32_t RCR;   // 0x30, Repetition counter register
	uint32_t CCR0;  // 0x34, Capture compare register 0
	uint32_t CCR1;  // 0x38, Capture compare register 1
	uint32_t CCR2;  // 0x3C, Capture compare register 2
	uint32_t CCR3;  // 0x40, Capture compare register 3
	uint32_t BDTR;  // 0x44, Break and dead time register
} GPTIMER_TypeDef;


#define GPTIMER0_BASE 0x40020000
#define GPTIMER0 ((volatile GPTIMER_TypeDef *) GPTIMER0_BASE)
#define APB_MASK_GPTIMER0 (1 << 16)
#define GPTIMER0_UPDATE_DMA_REQ 10
#define GPTIMER0_CC_DMA0_REQ 11
#define GPTIMER0_CC_DMA1_REQ 12
#define GPTIMER0_CC_DMA2_REQ 13
#define GPTIMER0_CC_DMA3_REQ 14
#define GPTIMER0_COM_DMA_REQ 15
#define GPTIMER0_TRIGGER_DMA_REQ 0
#define GPTIMER0_IRQn 19

#define GPTIMER1_BASE 0x40021000
#define GPTIMER1 ((volatile GPTIMER_TypeDef *) GPTIMER1_BASE)
#define APB_MASK_GPTIMER1 (1 << 17)
#define GPTIMER1_UPDATE_DMA_REQ 1
#define GPTIMER1_CC_DMA0_REQ 2
#define GPTIMER1_CC_DMA1_REQ 3
#define GPTIMER1_CC_DMA2_REQ 4
#define GPTIMER1_CC_DMA3_REQ 5
#define GPTIMER1_COM_DMA_REQ 6
#define GPTIMER1_TRIGGER_DMA_REQ 7
#define GPTIMER1_IRQn 20

#define GPTIMER2_BASE 0x40022000
#define GPTIMER2 ((volatile GPTIMER_TypeDef *) GPTIMER2_BASE)
#define APB_MASK_GPTIMER2 (1 << 18)
#define GPTIMER2_UPDATE_DMA_REQ 8
#define GPTIMER2_CC_DMA0_REQ 9
#define GPTIMER2_CC_DMA1_REQ 10
#define GPTIMER2_CC_DMA2_REQ 11
#define GPTIMER2_CC_DMA3_REQ 12
#define GPTIMER2_COM_DMA_REQ 13
#define GPTIMER2_TRIGGER_DMA_REQ 14
#define GPTIMER2_IRQn 21

#define GPTIMER3_BASE 0x40023000
#define GPTIMER3 ((volatile GPTIMER_TypeDef *) GPTIMER3_BASE)
#define APB_MASK_GPTIMER3 (1 << 19)
#define GPTIMER3_UPDATE_DMA_REQ 15
#define GPTIMER3_CC_DMA0_REQ 0
#define GPTIMER3_CC_DMA1_REQ 1
#define GPTIMER3_CC_DMA2_REQ 2
#define GPTIMER3_CC_DMA3_REQ 3
#define GPTIMER3_COM_DMA_REQ 4
#define GPTIMER3_TRIGGER_DMA_REQ 5
#define GPTIMER3_IRQn 22

#define GPTIMER4_BASE 0x40024000
#define GPTIMER4 ((volatile GPTIMER_TypeDef *) GPTIMER4_BASE)
#define APB_MASK_GPTIMER4 (1 << 20)
#define GPTIMER4_UPDATE_DMA_REQ 6
#define GPTIMER4_CC_DMA0_REQ 7
#define GPTIMER4_CC_DMA1_REQ 8
#define GPTIMER4_CC_DMA2_REQ 9
#define GPTIMER4_CC_DMA3_REQ 10
#define GPTIMER4_COM_DMA_REQ 11
#define GPTIMER4_TRIGGER_DMA_REQ 12
#define GPTIMER4_IRQn 23


/******************************************************************************/


typedef struct
{
	uint32_t PRERLO; // 0x00, Prescaler register lo-byte
	uint32_t PRERHI; // 0x04, Prescaler register hi-byte
	uint32_t CTR;    // 0x08, Control register
	union {
		uint32_t TXR;   // 0x0c, Transmit register
		uint32_t RXR;   // 0x0c, Receive register
	};
	union {
		uint32_t CR;    // 0x10, Command register
		uint32_t SR;    // 0x10, Status register
	};
} I2C_TypeDef;

#define I2C0_BASE 0x4002B000
#define I2C0 ((volatile I2C_TypeDef *) I2C0_BASE)
#define APB_MASK_I2C0 (1 << 27)
#define I2C0_IRQn 30

#define I2C1_BASE 0x4002C000
#define I2C1 ((volatile I2C_TypeDef *) I2C1_BASE)
#define APB_MASK_I2C1 (1 << 28)
#define I2C1_IRQn 31


/******************************************************************************/


typedef struct
{
	uint32_t PRIORITY[0x400]; // 0x0000-
	uint32_t PENDING[0x400];  // 0x1000-
	uint32_t ENABLE[0x7f800]; // 0x2000-
	uint32_t THRESHOLD;       // 0x200000
	uint32_t CLAIM_COMPLETE;  // 0x200004
} PLIC_TypeDef;

#define PLIC_BASE (0xc000000)
#define PLIC ((volatile PLIC_TypeDef *) PLIC_BASE)

typedef struct
{
	uint32_t MSIP;        // 0x0000, Machine software interrupt pending
	uint32_t RESERVED0[0x0fff];
	uint32_t MTIMECMP_LO; // 0x4000, Machine timer compare
	uint32_t MTIMECMP_HI;
	uint32_t RESERVED1[0x1ffc];
	uint32_t MTIME_LO;    // 0xbff8, Machine timer
	uint32_t MTIME_HI;
} CLINT_TypeDef;

#define CLINT ((volatile CLINT_TypeDef *) 0x2000000)


/******************************************************************************/

typedef struct
{
	uint32_t CTRL;      // 0x00, Control register
	uint32_t STAT;      // 0x04, Status register
	uint32_t MACMSB;    // 0x08, Mac address MSB
	uint32_t MACLSB;    // 0x0C, Mac address LSB
	uint32_t MDIO;      // 0x10, MDIO control and status register
	uint32_t TXBASE;    // 0x14, Transmit descriptor table base address
	uint32_t RXBASE;    // 0x18, Receive descriptor table base address
	uint32_t RESERVED0; // 0x1C, Reserved
	uint32_t HTMSB;     // 0x20, Hash table MSB
	uint32_t HTLSB;     // 0x24, Hash table LSB
} MAC_TypeDef;

#define MAC0_BASE 0x41040000
#define MAC ((volatile MAC_TypeDef *) MAC0_BASE)
#define AHB_MASK_MAC0 (1 << 3)
#define MAC0_IRQn 36


/******************************************************************************/


typedef struct
{
	uint16_t REG;
	uint16_t RESERVED;
} BKP_DR_Typedef;
#define BKP_DATA_REGISTERS 16

typedef struct
{
	uint16_t CRH;        // 0x00, RTC control register high
	uint16_t RESERVED0;
	uint16_t CRL;        // 0x04, RTC control register low
	uint16_t RESERVED1;
	uint16_t PRLH;       // 0x08, RTC prescaler load register high
	uint16_t RESERVED2;
	uint16_t PRLL;       // 0x0c, RTC prescaler load register low
	uint16_t RESERVED3;
	uint16_t DIVH;       // 0x10, RTC prescaler divider register high
	uint16_t RESERVED4;
	uint16_t DIVL;       // 0x14, RTC prescaler divider register low
	uint16_t RESERVED5;
	uint16_t CNTH;       // 0x18, RTC counter register high
	uint16_t RESERVED6;
	uint16_t CNTL;       // 0x1c, RTC counter register low
	uint16_t RESERVED7;
	uint16_t ALRH;       // 0x20, RTC alarm register high
	uint16_t RESERVED8;
	uint16_t ALRL;       // 0x24, RTC alarm register low
	uint16_t RESERVED9;
	uint16_t RCYC;       // 0x28, RTC read minimum cycle
	uint16_t RESERVED10;
	uint32_t RESERVED11; // 0x2C
	uint16_t BDCR;       // 0x30, Backup domain control register
	uint16_t BDRST;      // 0x32, Backup domain software reset
	uint16_t IWDG;       // 0x34, IWDG registers
	uint16_t RESERVED12; // 0x36
	uint32_t RESERVED13; // 0x38
	uint16_t RTCCR;      // 0x3C, RTC clock calibration register
	uint16_t RESERVED14;
	BKP_DR_Typedef BKP_DR[BKP_DATA_REGISTERS]; // 0x40, Backup domain data registers
} RTC_TypeDef;

#define RTC_BASE  (0x40000000)
#define RTC       ((volatile RTC_TypeDef *) RTC_BASE)


/******************************************************************************/


typedef struct __attribute__ ((aligned (4096)))
{
	uint32_t CTRL;          // 0x00, Control Register
	uint32_t RESERVED[3];   // 0x04-0x0C
	uint32_t PHASE_CTRL[8]; // 0x10-0x2C, Phase Control Register
	uint32_t PHASE_DATA[8]; // 0x30-0x4C, Phase Data Register
} SPI_TypeDef;

#define SPI0_BASE 0x40012000
#define SPI0 ((volatile SPI_TypeDef *) SPI0_BASE)
#define APB_MASK_SPI0 (1 << 2)
#define SPI0_TX_DMA_REQ 6
#define SPI0_RX_DMA_REQ 7
#define SPI0_IRQn 5

#define SPI1_BASE 0x40013000
#define SPI1 ((volatile SPI_TypeDef *) SPI1_BASE)
#define APB_MASK_SPI1 (1 << 3)
#define SPI1_TX_DMA_REQ 8
#define SPI1_RX_DMA_REQ 9
#define SPI1_IRQn 6

/******************************************************************************/


typedef struct
{
	uint32_t Timer1Load;   // 0x00, Load Register
	uint32_t Timer1Value;  // 0x04, Current Value Register
	uint32_t Timer1Ctrl;   // 0x08, Control Register
	uint32_t Timer1IntClr; // 0x0C, Interrupt Clear Register
	uint32_t Timer1RIS;    // 0x10, Raw Interrupt Status Register
	uint32_t Timer1MIS;    // 0x14, Masked Interrupt Status Register
	uint32_t Timer1BGL;    // 0x18, Background Load Register
	uint32_t RESERVED;     // 0x1C
	uint32_t Timer2Load;   // 0x20, Load Register
	uint32_t Timer2Value;  // 0x24, Current Value Register
	uint32_t Timer2Ctrl;   // 0x28, Control Register
	uint32_t Timer2IntClr; // 0x2C, Interrupt Clear Register
	uint32_t Timer2RIS;    // 0x30, Raw Interrupt Status Register
	uint32_t Timer2MIS;    // 0x34, Masked Interrupt Status Register
	uint32_t Timer2BGL;    // 0x38, Background Load Register
} TIMER_TypeDef;

#define TIMER0_BASE 0x4001E000
#define TIMER0 ((volatile TIMER_TypeDef *) TIMER0_BASE)
#define APB_MASK_TIMER0 (1 << 14)
#define TIMER0_IRQn 17

#define TIMER1_BASE 0x4001F000
#define TIMER1 ((volatile TIMER_TypeDef *) TIMER1_BASE)
#define APB_MASK_TIMER1 (1 << 15)
#define TIMER1_IRQn 18

/******************************************************************************/


typedef struct
{
	uint32_t DR;          // 0x00, Data register
	uint32_t RSR_ECR;     // 0x04, Receive status register/error clear register
	uint32_t RESERVED[4]; // 0x08-0x14
	uint32_t FR;          // 0x18, Flag register
	uint32_t RESERVED1[2];// 0x1C-0x20
	uint32_t IBRD;        // 0x24, Integer baud rate register
	uint32_t FBRD;        // 0x28, Fractional baud rate register
	uint32_t LCR_H;       // 0x2C, Line control register
	uint32_t CR;          // 0x30, Control register
	uint32_t IFLS;        // 0x34, Interrupt FIFO level select register
	uint32_t IMSC;        // 0x38, Interrupt mask set/clear register
	uint32_t RIS;         // 0x3C, Raw interrupt status register
	uint32_t MIS;         // 0x40, Masked interrupt status register
	uint32_t ICR;         // 0x44, Interrupt clear register
	uint32_t DMACR;       // 0x48, DMA control register
} UART_TypeDef;

#define UART0_BASE 0x40025000
#define UART0 ((volatile UART_TypeDef *) UART0_BASE)
#define APB_MASK_UART0 (1 << 21)
#define UART0_TX_DMA_REQ 13
#define UART0_RX_DMA_REQ 14
#define UART0_IRQn 24

#define UART1_BASE 0x40026000
#define UART1 ((volatile UART_TypeDef *) UART1_BASE)
#define APB_MASK_UART1 (1 << 22)
#define UART1_TX_DMA_REQ 15
#define UART1_RX_DMA_REQ 0
#define UART1_IRQn 25

#define UART2_BASE 0x40027000
#define UART2 ((volatile UART_TypeDef *) UART2_BASE)
#define APB_MASK_UART2 (1 << 23)
#define UART2_TX_DMA_REQ 1
#define UART2_RX_DMA_REQ 2
#define UART2_IRQn 26

#define UART3_BASE 0x40028000
#define UART3 ((volatile UART_TypeDef *) UART3_BASE)
#define APB_MASK_UART3 (1 << 24)
#define UART3_TX_DMA_REQ 3
#define UART3_RX_DMA_REQ 4
#define UART3_IRQn 27

#define UART4_BASE 0x40029000
#define UART4 ((volatile UART_TypeDef *) UART4_BASE)
#define APB_MASK_UART4 (1 << 25)
#define UART4_TX_DMA_REQ 5
#define UART4_RX_DMA_REQ 6
#define UART4_IRQn 28

/******************************************************************************/


typedef struct
{
	uint32_t WdogLoad;    // 0x00, Load Register
	uint32_t WdogValue;   // 0x04, Value Register
	uint32_t WdogControl; // 0x08, Control register
	uint32_t WdogIntClr;  // 0x0C, Interrupt Clear Register
	uint32_t WdogRIS;     // 0x10, Raw Interrupt Status Register
	uint32_t WdogMIS;     // 0x14, Masked Interrupt Status Register
	uint32_t RESERVED[762];
	uint32_t WdogLock;    // 0xC00, Lock Register
} WATCHDOG_TypeDef;
#define WDOG WATCHDOG0 // This is the only WATCHDOG supported for now

#define WATCHDOG0_BASE 0x40011000
#define WATCHDOG ((volatile WATCHDOG_TypeDef *) WATCHDOG0_BASE)
#define APB_MASK_WATCHDOG0 (1 << 1)
#define WATCHDOG0_IRQn 4


/******************************************************************************/


typedef struct
{
	uint32_t DR;        // 0x00, CRC Data register
	uint8_t  IDR;       // 0x04, CRC Independent data register
	uint8_t  RESERVED0; // 0x05, Reserved
	uint16_t RESERVED1; // 0x06, Reserved
	uint32_t CR;        // 0x08, CRC Control register
	uint32_t RESERVED2; // 0x0C, Reserved
	uint32_t INIT;      // 0x10, Initial CRC value register
	uint32_t POL;       // 0x14, CRC polynomial register
} CRC_TypeDef;

#define CRC0_BASE 0x41002000
#define CRC0 ((volatile CRC_TypeDef *) CRC0_BASE)
#define AHB_MASK_CRC0 (1 << 2)

/******************************************************************************/

/******************************************************************************/

/******************************************************************************/

/******************************************************************************/

/******************************************************************************/

/******************************************************************************/
















#endif
