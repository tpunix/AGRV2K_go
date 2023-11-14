

#include "main.h"


/******************************************************************************/


#define DIV_50M  0
#define DIV_25M  1
#define DIV_12M  3
#define DIV_10M  4
#define DIV_1M   49

#define DIV_HIGH DIV_25M
#define DIV_LOW  (250-1)


#define CMD_RESP_NONE     0x0000
#define CMD_RESP_SHORT    0x0040
#define CMD_RESP_SHORTNC  0x0080
#define CMD_RESP_LONG     0x00c0
#define CMD_TRANS_RD      0x0100
#define CMD_TRANS_WR      0x0200
#define CMD_DMA           0x4000
#define CMD_VALID         0x8000

#define RESP_MASK         0x00c0

#define RESP_NONE(cmd)  ((cmd&RESP_MASK)==CMD_RESP_NONE)
#define RESP_SHORT(cmd) ((cmd&RESP_MASK)==CMD_RESP_SHORT || (cmd&RESP_MASK)==CMD_RESP_SHORTNC)
#define RESP_LONG(cmd)  ((cmd&RESP_MASK)==CMD_RESP_LONG)
#define RESP_NOCRC(cmd) ((cmd&RESP_MASK)==CMD_RESP_SHORTNC)

#define HAS_DATA(cmd)   (cmd&(CMD_TRANS_RD|CMD_TRANS_WR))


#define CMD_00  ( 0 | CMD_RESP_NONE)
#define CMD_02  ( 2 | CMD_RESP_LONG)
#define CMD_03  ( 3 | CMD_RESP_SHORT)
#define CMD_06  ( 6 | CMD_RESP_SHORT)
#define CMD_07  ( 7 | CMD_RESP_SHORT)
#define CMD_08  ( 8 | CMD_RESP_SHORT)
#define CMD_09  ( 9 | CMD_RESP_LONG)
#define CMD_12  (12 | CMD_RESP_SHORT)
#define CMD_13  (13 | CMD_RESP_SHORT)
#define CMD_16  (16 | CMD_RESP_SHORT)
#define CMD_17  (17 | CMD_RESP_SHORT | CMD_TRANS_RD)
#define CMD_18  (18 | CMD_RESP_SHORT | CMD_TRANS_RD)
#define CMD_23  (23 | CMD_RESP_SHORT)
#define CMD_24  (24 | CMD_RESP_SHORT | CMD_TRANS_WR)
#define CMD_25  (25 | CMD_RESP_SHORT | CMD_TRANS_WR)
#define CMD_41  (41 | CMD_RESP_SHORTNC)
#define CMD_55  (55 | CMD_RESP_SHORT)


#define ERROR_MASK  0x0004

#define CF_CD      0x0001
#define CF_CMDONE  0x0002
#define CF_TIMEOUT 0x0004
#define CF_RESP    0x0008
#define CF_DTDONE  0x0010
#define CF_DRDY    0x0080

typedef struct
{
	u32 config;
	u32 inten;
	u32 flags;
	u32 data;
	u32 cmd;
	u32 arg;
	u32 resp1;
	u32 unuse;
}SDCTRL_REGS;

#define SDCTRL ((volatile SDCTRL_REGS*)0x70000100)


/******************************************************************************/


/******************************************************************************/

static u32 cmd_resp[5];
static int respp;
static int is_sdhc;
static int sd_rca;

static int bsize;
static int csize;
static int cblock;


/******************************************************************************/

static volatile int c_inten;

static int sd_irq(u32 *regs, void *arg)
{
	int flags;

	//printk("\nsd_irq! c_inten=%02x flags=%04x\n", c_inten, SDCTRL->flags);
	while(1){
		flags = SDCTRL->flags & c_inten;
		if(flags==0)
			break;

		if(flags&CF_RESP){
			cmd_resp[respp] = SDCTRL->arg;
			SDCTRL->flags = CF_RESP;
			respp += 1;
		}

		if(flags & (CF_CMDONE | CF_DTDONE | CF_TIMEOUT)){
			SDCTRL->inten = 0;
			c_inten = 0;
			break;
		}
	}

	//printk("EOI. E=%02x F=%04x\n", c_inten, SDCTRL->flags);
	return 0;
}


void sd_init(void)
{
	SDCTRL->config = (DIV_LOW<<4) | 0x01;
	SDCTRL->inten = 0;
	SDCTRL->flags = 0xffff;

	int_request(CLINT0_IRQn, sd_irq, NULL);
	int_enable(CLINT0_IRQn);
}

int sd_cmd(int cmd, int arg, void *dbuf, int bsize)
{
	int flags;

	respp = 0;
	cmd_resp[0] = 0;
	c_inten = CF_TIMEOUT;
	cmd |= CMD_VALID;

	SDCTRL->flags = 0xffff;

	if(HAS_DATA(cmd)){
		c_inten |= CF_DTDONE;
		cmd |= CMD_DMA;
		cmd |= ((bsize-1)<<16);
		if(cmd&CMD_TRANS_RD){
			DMAC->CH[0].SrcAddr = (u32)&SDCTRL->data;
			DMAC->CH[0].DstAddr = (u32)dbuf;
			DMAC->CH[0].LLI     = 0x00000000;
			DMAC->CH[0].Control = 0x0a480000 | (bsize*512/4);
			DMAC->CH[0].Config  = (2<<11) | (0<<6) | (1<<1) | 1;
		}
		if(cmd&CMD_TRANS_WR){
			DMAC->CH[0].SrcAddr = (u32)dbuf;
			DMAC->CH[0].DstAddr = (u32)&SDCTRL->data;
			DMAC->CH[0].LLI     = 0x00000000;
			DMAC->CH[0].Control = 0x06480000 | (bsize*512/4);
			DMAC->CH[0].Config  = (1<<11) | (1<<6) | (0<<1) | 1;
		}
	}else{
		c_inten |= CF_CMDONE;
		if(RESP_LONG(cmd)){
			c_inten |= CF_RESP;
		}
	}

	SDCTRL->inten = c_inten;
	SDCTRL->arg = arg;
	SDCTRL->cmd = cmd;

	int timeout = 0;
	if(cmd&CMD_TRANS_WR){
		reset_timer();
		int et = get_timer() + 1000000;
		while(c_inten){
			if(get_timer()>=et){
				timeout = 1;
				break;
			}
		}
	}else{
		while(c_inten){
			WFI();
		}
	}
	flags = SDCTRL->flags;
	if(timeout){
		printk("\nCMD timeout!\n");
	}else{
		SDCTRL->flags = 0xffff;
		SDCTRL->cmd = 0;
	}
	if(HAS_DATA(cmd)){
		// TODO: STOP DMA
	}
	printk("SDCMD(%02x): F=%02x", cmd&0x3f, flags);


	if(RESP_LONG(cmd)){
		printk(" C=%08x RESP=%08x %08x %08x %08x\n",
				cmd_resp[4], cmd_resp[3], cmd_resp[2], cmd_resp[1], cmd_resp[0]);
	}else if(RESP_SHORT(cmd)){
		cmd_resp[0] = SDCTRL->arg;
		cmd_resp[4] = SDCTRL->resp1;
		printk(" C=%08x RESP=%08x\n", cmd_resp[4], cmd_resp[0]);
	}else{
		printk("\n");
	}

	return (flags & ERROR_MASK);
}


static int sd_acmd(u32 cmd, u32 arg)
{
	int retv;

	retv = sd_cmd(CMD_55, sd_rca<<16, NULL, 0);
	if(retv)
		return retv;

	retv = sd_cmd(cmd, arg, NULL, 0);
	return retv;
}

int sd_test(void)
{
	int retv, hcs;
	char tmp[8];

	is_sdhc = 0;
	sd_rca = 0;

	sd_init();
	mdelay(1);

	sd_cmd(CMD_00, 0x00000000, NULL, 0);
	mdelay(10);
	sd_cmd(CMD_00, 0x00000000, NULL, 0);
	mdelay(10);

	retv = sd_cmd(CMD_08, 0x000001aa, NULL, 0);
	if(retv)
		hcs = 0;
	else
		hcs = 1<<30;

	while(1){
		retv = sd_acmd(CMD_41, 0x00f00000|hcs);
		if(retv)
			break;
		if(cmd_resp[0]&0x80000000)
			break;
		mdelay(10);
	}
	if(retv){
		printk("ACMD41 error! %08x\n", retv);
		return -1;
	}
	if(cmd_resp[0]&0x40000000)
		is_sdhc = 1;

	printk("    V2.0: %s\n", (hcs?"Yes":"No"));
	printk("    SDHC: %s\n", (is_sdhc?"Yes":"No"));

	// Send CID
	retv = sd_cmd(CMD_02, 0x00000000, NULL, 0);
	if(retv)
		return retv;
	u8 *sd_cid = (u8*)&cmd_resp[1];

	tmp[0] = sd_cid[0];
	tmp[1] = sd_cid[7];
	tmp[2] = sd_cid[6];
	tmp[3] = sd_cid[5];
	tmp[4] = sd_cid[4];
	tmp[5] = 0;
	printk("    Name: %s\n", tmp);

	// Send RCA
	retv = sd_cmd(CMD_03, 0x00000000, NULL, 0);
	if(retv)
		return retv;
	sd_rca = cmd_resp[0]>>16;

	// Send CSD
	retv = sd_cmd(CMD_09, (sd_rca<<16), NULL, 0);
	if(retv){
		sd_rca  =0;
		return retv;
	}
	u8 *sd_csd = (u8*)&cmd_resp[1];


	// Get Size form CSD
	bsize = 1<<(sd_csd[6]&0x0f);
	printk("    Blen: %d\n", bsize);

	if(is_sdhc){
		csize = (sd_csd[4]<<16) | (sd_csd[11]<<8) | sd_csd[10];
		csize &= 0x003fffff;
		csize = (csize+1)*512;
		if(bsize==512){
			cblock = csize*2;
		}else if(bsize==1024){
			cblock = csize;
		}else{
			cblock = csize/2;
		}
	}else{
		int m;
		m = (sd_csd[10]<<8) | sd_csd[9];
		m = (m>>7)&7;
		m = 1<<(m+2);
		csize = (sd_csd[5]<<16) | (sd_csd[4]<<8) | sd_csd[11];
		csize = (csize>>6)&0x00000fff;
		csize = (csize+1)*m;
		cblock = csize;
		if(bsize==512)  csize /= 2;
		if(bsize==2048) csize *= 2;
	}
	printk("    Blks: %d\n", cblock);
	printk("    Size: %d K\n", csize);


#if 1
	// Select CARD
	retv = sd_cmd(CMD_07, sd_rca<<16, NULL, 0);
	if(retv){
		sd_rca = 0;
		return retv;
	}
	//sd_clk_set(SDCLK_HIGH);
	SDCTRL->config = (DIV_HIGH<<4) | 0x01;

	// Set Block Len
	sd_cmd(CMD_16, 512, NULL, 0);

	// Switch to 4bit
	retv = sd_acmd(CMD_06, 2);
	if(retv)
		return retv;
	//sd_bus_width(4);

	// Card send its status
	retv = sd_cmd(CMD_13, sd_rca<<16, NULL, 0);
	if(retv)
		return retv;
#endif


	return 0;
}


/******************************************************************************/


int sd_read_blocks(u32 block, int count, u8 *buf)
{
	int retv;

	printk("sd_read_blocks:  blk=%08x cnt=%d\n", block, count);

	if(!is_sdhc)
		block *= 512;

	if(count==1){
		retv = sd_cmd(CMD_17, block, buf, 1);
	}else{
		retv = sd_cmd(CMD_18, block, buf, count);
		if(retv==0){
			retv = sd_cmd(CMD_12, 0, NULL, 0);
		}
	}

	return retv;
}


int sd_write_blocks(u32 block, int count, u8 *buf)
{
	int retv;

	if(!is_sdhc)
		block *= 512;

	if(count==1){
		retv = sd_cmd(CMD_24, block, buf, 1);
	}else{
		retv = sd_acmd(CMD_23, count);
		if(retv){
			return retv;
		}
		retv = sd_cmd(CMD_25, block, buf, count);
		if(retv==0){
			retv = sd_cmd(CMD_12, 0, NULL, 0);
		}
	}

	return retv;
}


/******************************************************************************/


u8 *dbuf = (u8*)0x20010000;

int sd_testr(int blkaddr, int blkcnt)
{
	int retv;

	retv = sd_read_blocks(blkaddr, blkcnt, dbuf);

	return retv;
}


int sd_testw(int blkaddr, int blkcnt)
{
	int retv;

	if(blkaddr==0){
		blkaddr = 1;
	}
	retv = sd_write_blocks(blkaddr, blkcnt, (u8*)0x80000000);

	return retv;
}


int sd_status(void)
{
	int retv;

	retv = sd_cmd(CMD_13, sd_rca<<16, NULL, 0);

	return retv;
}

/******************************************************************************/


