

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


/******************************************************************************/


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


void gpio_af(int group, int bit, int en)
{
	volatile GPIO_TypeDef *gpio = (GPIO_TypeDef*)(0x40014000+group*0x1000);
	int mask = 1<<bit;

	if(en){
		gpio->AFSEL |=  mask;
	}else{
		gpio->AFSEL &= ~mask;
	}
}


void gpio_dir(int group, int bit, int dir)
{
	volatile GPIO_TypeDef *gpio = (GPIO_TypeDef*)(0x40014000+group*0x1000);
	int mask = 1<<bit;

	if(dir){
		gpio->DIR |=  mask;
	}else{
		gpio->DIR &= ~mask;
	}
}


void gpio_set(int group, int bit, int val)
{
	volatile GPIO_TypeDef *gpio = (GPIO_TypeDef*)(0x40014000+group*0x1000);
	int mask = 1<<bit;

	gpio->DATA[mask] = (val)? mask : 0;
}


int gpio_get(int group, int bit)
{
	volatile GPIO_TypeDef *gpio = (GPIO_TypeDef*)(0x40014000+group*0x1000);
	int mask = 1<<bit;

	return (gpio->DATA[mask] & mask) ? 1: 0;
}

/******************************************************************************/


void device_init(void)
{
	SYS->AHB_CLKENABLE = 0x05;       // CRC DMA
	SYS->APB_CLKENABLE = 0x00217ffc; // UART0 GTIM0 TIM0 GPIO[9:0] SPI1 SPI0

	GPIO6->AFSEL |= (1<<1); // UART0_RXD
	GPIO7->AFSEL |= (1<<6); // UART0_TXD

	GPIO4->AFSEL |= 0x80;   // SPI1: SCK
	GPIO5->AFSEL |= 0x01;   // SPI1: CSN
	GPIO0->AFSEL |= 0x30;   // SPI1: SI SO

	GPIO2->DIR |= 0x01;

	uart0_init(115200);

	DMAC->IntTCClear = 0xff;
	DMAC->Config = 0x00000001;
}


/******************************************************************************/

static void *int_vectors[64];
static void *int_args[64];

int int_request(int id, void *func, void *arg)
{
	int_vectors[id] = func;
	int_args[id] = arg;
	return 0;
}


int int_priorit(int id, int priorit)
{
	if(id<48)
		PLIC->PRIORITY[id] = priorit;
	return 0;
}


int int_enable(int id)
{
	if(id<48){
		PLIC->ENABLE[id/32] |=  (1<<(id%32));
	}else{
		if(id==48){
			set_csr(mie, (1<<IRQ_M_SOFT));
		}else if(id==49){
			set_csr(mie, (1<<IRQ_M_TIMER));
		}else{
			set_csr(mie, (1<<(id-50+16)));
		}
	}
	return 0;
}


int int_disable(int id)
{
	if(id<48){
		PLIC->ENABLE[id/32] &= ~(1<<(id%32));
	}else{
		if(id==48){
			clear_csr(mie, (1<<IRQ_M_SOFT));
		}else if(id==49){
			clear_csr(mie, (1<<IRQ_M_TIMER));
		}else{
			clear_csr(mie, (1<<(id-50+16)));
		}
	}
	return 0;
}


static void int_handle(u32 *regs, int cause)
{
	int id=0;
	int (*func)(void *regs, void *arg);

	if(cause==11){
		// from PLIC
		while(1){
			id = PLIC->CLAIM;
			if(id==0)
				break;
			func = int_vectors[id];
			if(func){
				func(regs, int_args[id]);
			}else{
				printk("\nEmpty Interrupt %d!\n", id);
			}
			PLIC->CLAIM = id;
		}
	}else{
		// from CLINT
		if(cause==3){
			id = 48;
		}else if(cause==7){
			id = 49;
		}else if(cause>15){
			id = cause-16+50;
		}

		func = int_vectors[id];
		if(func){
			func(regs, int_args[id]);
		}else{
			printk("\nEmpty Interrupt %d!\n", id);
		}
	}
}


/******************************************************************************/

static char *excp_msg[16] = {
	"Inst Align",
	"Inst Fault",
	"Inst Illegal",
	"Breakpoint",
	"Load Align",
	"Load Fault",
	"Store Align",
	"Store Fault",
	"ECall_U",
	"ECall_S",
	"Reserved",
	"ECall_M",
	"IPage Fault",
	"LPage Fault",
	"Reserved",
	"SPage Fault",
};

static char *reg_names[32] = {
	"x0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
	"s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
	"a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
	"s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

void trap_handle(u32 *regs, int cause)
{
	int i;

	if(cause<0){
		int_handle(regs, cause&0xff);
		return;
	}else{
		printk("\n[%s!]: EPC=%08x TVAL=%08x CAUSE=%d\n", excp_msg[cause], regs[31], read_csr(mtval), cause);
	}

	printk(" x0=00000000 ");
	for(i=1; i<32; i++){
		if((i%4)==0)
			printk("\n");
		printk("%3s=%08x ", reg_names[i], regs[i-1]);
	}
	printk("\n");


	while(1);
}


void _trap_entry(void);

void trap_init(void)
{
	int i;

	clear_csr(mstatus, MSTATUS_MIE);
	write_csr(mtvec, (u32)_trap_entry);

	// 貌似第一次enable一个外部中断，总会先触发一次。这里先统一触发清除一次。
	PLIC->ENABLE[0] = 0xffffffff;
	PLIC->ENABLE[1] = 0xffffffff;
	do{
		i = PLIC->CLAIM;
		PLIC->CLAIM = i;
	}while(i);

	PLIC->ENABLE[0] = 0;
	PLIC->ENABLE[1] = 0;
	PLIC->THRESHOLD = 0;

	for(i=0; i<64; i++){
		int_vectors[i] = NULL;
		int_args[i] = NULL;
		PLIC->PRIORITY[i] = 8;
	}

	set_csr(mie, (1<<IRQ_M_EXT));
	set_csr(mstatus, MSTATUS_MIE);
}


/******************************************************************************/


int dmac_tc_irq(void *regs, void *arg)
{
	printk("\nDMAC TC!\n");
	DMAC->IntTCClear = 1;
	return 0;
}

void dma_test(void)
{

	int_request(DMAC0_INTTC_IRQn, dmac_tc_irq, NULL);
	int_enable(DMAC0_INTTC_IRQn);

	DMAC->CH[0].SrcAddr = 0x80000000;
	DMAC->CH[0].DstAddr = 0x68000000;
	DMAC->CH[0].LLI     = 0x00000000;
	DMAC->CH[0].Control = 0x8e480000 | (2048);
	DMAC->CH[0].Config  = 0x00008001;

}



/******************************************************************************/


int main(void)
{
	system_init();
	device_init();
	trap_init();

	puts("\n\nAGRV2K start!\n");
	printk(" mstatus: %08x\n", read_csr(mstatus));
	printk("   mtvec: %08x\n", read_csr(mtvec));
	printk("    mie: %08x\n", read_csr(mie));
	printk("    mip: %08x\n", read_csr(mip));

	simple_shell();

	while(1){
		gpio_set(2, 0, 0);
		mdelay(500);
		gpio_set(2, 0, 1);
		mdelay(500);
		//printk("FR: %08x\n", UART0->FR);
	}

	return 0;
}



