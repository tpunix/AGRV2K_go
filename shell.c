
#include "main.h"

/******************************************************************************/

static char hbuf[128];
static int hblen = 0;
#define PROMPT "rv"

int gets(char *buf, int len)
{
	int n, ch, esc;

	printk(PROMPT "> ");
	n = 0;
	esc = 0;
	while(1){
		while((ch=getc(1000))<0);
		if(ch==0x0d || ch==0x0a){
			break;
		}

		if(esc==0 && ch==0x1b){
			esc = 1;
			continue;
		}
		if(esc==1){
			if(ch==0x5b){
				esc = 2;
			}else{
				printk("<ESC1 %02x>", ch);
				esc = 0;
			}
			continue;
		}
		if(esc==2){
			esc = 0;
			if(ch==0x41 || ch==0x43){
				// UP:0x41  RIGHT:0x43
				if(hblen){
					int sn = n;
					n = hblen;
					memcpy(buf, hbuf, hblen);
					printk("\r" PROMPT "> %s", hbuf);
					while(hblen<sn){
						printk(" ");
						hblen += 1;
					}
					if(ch==0x43){
						break;
					}
				}
			}else{
				printk("<ESC2 %02x>", ch);
			}
		}else if(ch==0x08){
			if(n){
				n -= 1;
				printk("\b \b");
			}
		}else if(ch>=0x20){
			printk("%c", ch);
			buf[n] = ch;
			n++;
			if(n==len)
				break;
		}else{
			printk("{%02x}", ch);
		}
	}

	if(n){
		memcpy(hbuf, buf, n);
		hbuf[n] = 0;
		hblen = n;
	}

	printk("\r\n");
	buf[n] = 0;
	return n;
}


static int arg_base = 16;

int str2hex(char *str, int *hex)
{
	int n, p, ch, base;

	base = arg_base;
	n = 0;
	p = 0;
	hex[0] = 0;
	while(1){
		ch = *str++;
		if(ch==' '){
			if(p){
				n++;
				hex[n] = 0;
				p = 0;
			}
			continue;
		}else if((ch=='0' && *str=='x')){
			str += 1;
			base = 16;
		}else if((ch>='0' && ch<='9')){
			ch -= '0';
		}else if((base==16 && ch>='a' && ch<='f')){
			ch = ch-'a'+10;
		}else if((base==16 && ch>='A' && ch<='F')){
			ch = ch-'A'+10;
		}else{
			if(p)
				n++;
			return n;
		}
		hex[n] = hex[n]*base + ch;
		p++;
	}
}


/**********************************************************/

static int dump_width = 4;
static int dump_addr = 0;

void dump(int argc, int *args, int width)
{
	int p, addr, len;
	u32 d;

	addr = (argc>=1)? args[0] : dump_addr;
	len  = (argc>=2)? args[1] : 256;
	if(width==0)
		width = dump_width;

	p = 0;
	while(p<len){
		if((p%16)==0){
			printk("\n%08x:  ", addr);
		}else if((p%8)==0){
			printk("- ");
		}

		if(width==1){
			d = *(u8*)(addr);
			printk("%02x ", d);
		}else if(width==2){
			d = *(u16*)(addr);
			printk("%04x ", d);
		}else{
			d = *(u32*)(addr);
			printk("%08x ", d);
		}
		p += width;
		addr += width;
	}
	printk("\n");

	dump_width = width;
	dump_addr = addr;
}



/******************************************************************************/

void memcmp32(void *dst_ptr, void *src_ptr, int len)
{
	u32 *dst = (u32*)dst_ptr;
	u32 *src = (u32*)src_ptr;
	int i;

	for(i=0; i<len; i+=4){
		u32 dd = *dst;
		u32 sd = *src;
		if(dd!=sd){
			printk("memcmp mismatch at %08x:\n", i);
			printk("    dst_%08x=%08x\n", dst, dd);
			printk("    src_%08x=%08x\n", src, sd);
			break;
		}
		dst++;
		src++;
	}
	if(i>=len){
		printk("memcmp OK!\n");
	}
}


#define CMD_START if(0){}
#define CMD(x) else if(strcmp(cmd, #x)==0)

void simple_shell(void)
{
	char cmd[128];
	int argc, arg[4];

	dump_addr = 0x08000000;

	while(1){
		gets(cmd, 128);
		char *sp = strchr(cmd, ' ');
		if(sp){
			*sp = 0;
			sp += 1;
			argc = str2hex(sp, arg);
		}else{
			argc = 0;
		}

		CMD_START
		CMD(base16) { arg_base = 16; }
		CMD(base10) { arg_base = 10; }
		CMD(rb){ printk("%08x: %02x\n", arg[0], *(u8*)arg[0]);  }
		CMD(rw){ printk("%08x: %04x\n", arg[0], *(u16*)arg[0]); }
		CMD(rd){ printk("%08x: %08x\n", arg[0], *(u32*)arg[0]);	}
		CMD(wb){ printk("%08x= %02x\n", arg[0], (u8)arg[1]);  *(u8*)arg[0]  = arg[1]; }
		CMD(ww){ printk("%08x= %04x\n", arg[0], (u16)arg[1]); *(u16*)arg[0] = arg[1]; }
		CMD(wd){ printk("%08x= %08x\n", arg[0], (u32)arg[1]); *(u32*)arg[0] = arg[1]; }
		CMD(db){ dump(argc, arg, 1); }
		CMD(dw){ dump(argc, arg, 2); }
		CMD(dd){ dump(argc, arg, 4); }
		CMD(d) { dump(argc, arg, 0); }
		CMD(go){ void (*go)(void) = (void(*)(void))arg[0]; go(); }
		
		CMD(memset){
			if(argc>=3){
				memset((void*)arg[0], arg[1], arg[2]);
			}else{
				printk("Wrong args!\n");
			}
		}
		CMD(memcpy){
			if(argc>=3){
				memcpy((void*)arg[0], (void*)arg[1], arg[2]);
			}else{
				printk("Wrong args!\n");
			}
		}
		CMD(memcmp){
			if(argc>=3){
				memcmp32((void*)arg[0], (void*)arg[1], arg[2]);
			}else{
				printk("Wrong args!\n");
			}
		}

		CMD(cpmem){
			memcpy((u8*)0x68000000, (u8*)0x80000000, 0x100000);
		}
		CMD(t){
			memcmp32((u8*)0x68000000, (u8*)0x80000000, 0x100000);
		}

		CMD(df){
			u32 t0 = REG(0x70000000);
			u32 t1 = REG(0x70000004);
			u32 t2 = REG(0x70000008);
			u32 t3 = REG(0x7000000c);
			int i;
			printk("\n%08x %08x %08x %08x\n", t0, t1, t2, t3);
			printk("--------------------------------\n");
			for(i=0; i<32; i++){ if(t0&(1<<(31-i))) putc('1'); else putc('0'); } printk("\n");
			for(i=0; i<32; i++){ if(t1&(1<<(31-i))) putc('1'); else putc('0'); } printk("\n");
			for(i=0; i<32; i++){ if(t2&(1<<(31-i))) putc('1'); else putc('0'); } printk("\n");
			for(i=0; i<32; i++){ if(t3&(1<<(31-i))) putc('1'); else putc('0'); } printk("\n");
			printk("--------------------------------\n");
			for(i=0; i<32; i++){
				int b0 = (t0&(1<<(31-i)))? 1: 0;
				int b1 = (t1&(1<<(31-i)))? 2: 0;
				int b2 = (t2&(1<<(31-i)))? 4: 0;
				int b3 = (t3&(1<<(31-i)))? 8: 0;
				int nb = b3 | b2 | b1 | b0;
				printk("%x", nb);
			}
			printk("\n\n");
		}

		CMD(mt){
			int cnt=0;
			while(cnt<51){
				printk("cnt: %d\n", cnt);
				cnt += 1;
				udelay(1000000);
			}
		}

#ifdef RUN_COREMARK
		CMD(cmk){
			void cmain(void);
			cmain();
		}
#endif

		CMD(q){
			break;
		}
	}
}

