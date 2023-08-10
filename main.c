

#include "main.h"


/******************************************************************************/


u32 pclk_freq;
u32 sclk_freq;


void system_init(void)
{
	u32 sclk_div = (SYSCLK_FREQ-1)/FLASH_MAX_FREQ;
	u32 clkctl = SYS->CLK_CNTL;

	// set sclk divider
	clkctl &= ~0xff00;
	clkctl |= (sclk_div<<8) | (sclk_div<<12);

	// enable HSE and PLL
	clkctl &= ~(1<<3);
	clkctl |= (1<<2) | (1<<5);
	SYS->CLK_CNTL = clkctl;

	// wait HSE and PLL
	while(!(SYS->CLK_CNTL & ((1<<4)|(1<<6))));

	// Switch SYSCLK to PLL
	clkctl &= ~3;
	clkctl |= 2;
	SYS->CLK_CNTL = clkctl;

	pclk_freq = SYSCLK_FREQ/(SYS->PBUS_DIVIDER+1);
	sclk_freq = SYSCLK_FREQ/(sclk_div+1);

	RTC->BDCR = 0x8101;
}


/******************************************************************************/


u64 get_mcycle(void)
{
	u32 h0, h1, low;

	do{
		h0  = read_csr(mcycleh);
		low = read_csr(mcycle);
		h1  = read_csr(mcycleh);
	}while(h0!=h1);

	return ((u64)h0<<32) | low;
}


void timer_init(void)
{
	reset_timer();
}


void reset_timer(void)
{
	SYS->MTIME_PSC = 0x40000000;
	CLINT->MTIME_LO = 0;
	CLINT->MTIME_HI = 0;
	SYS->MTIME_PSC = (SYSCLK_FREQ/1000000)-1;
}


u32 get_timer(void)
{
	return CLINT->MTIME_LO;
}


void udelay(int us)
{
	reset_timer();
	u32 end = get_timer() + us;
	while(get_timer()<end);
}


void mdelay(int ms)
{
	udelay(1000*ms);
}


/******************************************************************************/


int rtc_getcnt(void)
{
	int cntl = RTC->CNTL;
	int cnth = RTC->CNTH;
	return (cnth<<16)|cntl;
}


/******************************************************************************/


void uart0_init(int baudrate)
{
	int div64;

	div64 = (pclk_freq*4 + baudrate/2)/baudrate;

	UART0->CR = 0;
	UART0->IBRD = (div64>>6);
	UART0->FBRD = (div64&0x3f);
	UART0->LCR_H = 0x0070;
	UART0->ICR = 0x07ff;
	UART0->IMSC = 0;
	UART0->DMACR = 0;
	UART0->CR = 0x0301;
}


int getc(int tmout)
{
	reset_timer();
	u32 end = get_timer() + tmout*(TIMER_HZ/1000);
	while((UART0->FR&0x0010)){
		if(get_timer()>=end){
			return -1;
		}
	};
	return (UART0->DR)&0xff;
}


void putc(int ch)
{
	while((UART0->FR&0x0020));
	UART0->DR = ch;
}

void puts(char *str)
{
	int ch;
	while((ch=*str++)){
		if(ch=='\n')
			putc('\r');
		putc(ch);
	}
}


/******************************************************************************/


void device_init(void)
{
	SYS->AHB_CLKENABLE = 0x05;       // CRC DMA
	SYS->APB_CLKENABLE = 0x00217ffc; // UART0 GTIM0 TIM0 GPIO[9:0] SPI1 SPI0

	GPIO6->AFSEL |= (1<<1); // UART0_RXD
	GPIO7->AFSEL |= (1<<6); // UART0_TXD
	GPIO4->AFSEL |= (3<<5); // SPI0: SCK CSN
	GPIO0->AFSEL |= 0x0f;   // SPI0: SI SO WP HOLD

	GPIO2->DIR |= 0x02;

	uart0_init(115200);
}


/******************************************************************************/


int main(void)
{
	system_init();
	device_init();

	puts("\n\nAGRV2K start!\n");

	simple_shell();

	while(1){
		printk("FR: %08x\n", UART0->FR);
	}

	return 0;
}



