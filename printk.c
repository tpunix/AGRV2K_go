/*
 *
 * printk.c
 *
 * printk for bootbox
 *
 * support format:
 *  %c
 *  %s
 *  %[[0]n]x
 *  %[[0]n]X
 *  %[[0]n]d
 *
 */

#include <stdarg.h>

#include "main.h"

static int itostr(char *buf, unsigned long long data, int base, int upper, int frac_width)
{
	int res, len, i;
	char *str;

	if(base!=15){
		frac_width = 0;
	}else{
		base = 10;
	}

	str = buf;
	i = 0;
	do{
		res = data%base;
		data = data/base;
		if(res<10){
			res += '0';
		}else{
			if(upper){
				res += 'A'-10;
			}else{
				res += 'a'-10;
			}
		}
		*str++ = res;
		i += 1;
		if(i==frac_width)
			*str++ = '.';
	}while(data);
	if(*(str-1)=='.')
		*str++ = '0';
	len = str-buf;

	/* reverse digital order */
	for(i=0; i<len/2; i++){
		res = buf[i];
		buf[i] = buf[len-1-i]; 
		buf[len-1-i] = res; 
	}

	return len;
}

/*
 * vsnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 */

#define OUT_C(c) \
	if(str<end){ \
		*str++ = (c); \
	}

int vsnprintk(char *buf, int size, char *fmt, va_list args)
{
	char ch, *s, *str, *end, *sstr;
	char digital_buf[32];
	int zero_pad, left_adj, add_sign, field_width, frac_width, sign;
	int i, base, upper, len;


	if(!buf || !fmt ||!size){
		return 0;
	}

	str = buf;
	end = buf+size;

	while(str<end && *fmt){
		if(*fmt!='%'){
			if(*fmt=='\n')
				OUT_C('\r');
			OUT_C(*fmt++);
			continue;
		}

		/* skip '%' */
		sstr = fmt;
		fmt++;

		/* %% */
		if(*fmt=='%'){
			OUT_C(*fmt++);
			continue;
		}

		/* get flag */
		zero_pad = ' ';
		left_adj = 0;
		add_sign = 0;
		while((ch=*fmt)!=0){

			if(*fmt=='0'){
				zero_pad = '0';
			}else if(*fmt=='-'){
				left_adj = 1;
			}else if(*fmt=='#'){
			}else if(*fmt==' '){
				if(add_sign!='+')
					add_sign = ' ';
			}else if(*fmt=='+'){
				add_sign = '+';
			}else{
				break;
			}
			fmt++;
		}

		/* get field width: m.n */
		field_width = 0;
		/* get m */
		while(*fmt>'0' && *fmt<='9'){
			field_width = field_width*10+(*fmt-'0');
			fmt++;
		}
		frac_width = 6;
		if(*fmt=='.'){
			fmt++;
			/* get n */
			while(*fmt>'0' && *fmt<='9'){
				frac_width = (*fmt-'0'); // only support one digital
				fmt++;
			}
		}

		/* get format char */
		upper = 0;
		base = 0;
		sign = 0;
		len = 0;
		s = digital_buf;
		while((ch=*fmt)!=0){
			fmt++;
			switch(ch){
			/* hexadecimal */
			case 'p':
			case 'X':
				upper = 1;
			case 'x':
				base = 16;
				break;

			/* decimal */
			case 'd':
			case 'i':
				sign = 1;
			case 'u':
				base = 10;
				break;

			/* octal */
			case 'o':
				base = 8;
				break;

			/* character */
			case 'c':
				digital_buf[0] = (unsigned char) va_arg(args, int);
				len = 1;
				break;

			/* string */
			case 's':
				s = va_arg(args, char *);
				if(!s) s = "<NUL>";
				len = strlen(s);
				break;

			/* float format, skip it */
			case 'E': case 'F': case 'G': case 'A':
				upper = 1;
			case 'e': case 'f': case 'g': case 'a':
				base = 15;
				break;

			/* length modifier */
			case 'l': case 'L': case 'h': case 'j': case 'z': case 't':
				/* skip it */
				continue;

			/* bad format */
			default:
				s = sstr;
				len = fmt-sstr;
				break;
			}
			break;
		}

		if(base){
			unsigned long long d64;
			if(base==15){
				typedef union { double d; int i[2]; }DFP;
				DFP df;
				df.d = va_arg(args, double);
				if(df.i[1]<0){
					add_sign = '-';
					df.d = -df.d;
				}
				for(i=0; i<frac_width; i++){
					df.d *= 10;
				}
				d64 = df.d+0.5;
			}else{
				i = va_arg(args, int);
				if(base==10 && sign && i<0){
					add_sign = '-';
					i = -i;
				}
				d64 = (unsigned int)i;
			}
			len = itostr(digital_buf, d64, base, upper, frac_width);
		}else{
			zero_pad = ' ';
			add_sign = 0;
		}

		if(s){
			if(len>=field_width){
				field_width = len;
				if(add_sign)
					field_width++;
			}
			for(i=0; i<field_width; i++){
				if(left_adj){
					if(i<len){
						OUT_C(*s++);
					}else{
						OUT_C(' ');
					}
				}else{
					if(add_sign && (zero_pad=='0' || i==(field_width-len-1))){
						OUT_C(add_sign);
						add_sign = 0;
						continue;
					}
					if(i<(field_width-len)){
						OUT_C(zero_pad);
					}else{
						OUT_C(*s++);
					}
				}
			}
		}
	}

	OUT_C(0);
	return str-buf;
}


int printk(char *fmt, ...)
{
	va_list args;
	int printed_len;
	char printk_buf[256];

	/* Emit the output into the temporary buffer */
	va_start(args, fmt);
	printed_len = vsnprintk(printk_buf, sizeof(printk_buf), fmt, args);
	va_end(args);

	puts(printk_buf);

	return printed_len;
}


int sprintk(char *sbuf, const char *fmt, ...)
{
	va_list args;
	int printed_len;

	/* Emit the output into the temporary buffer */
	va_start(args, fmt);
	printed_len = vsnprintk(sbuf, 256, (char*)fmt, args);
	va_end(args);

	return printed_len;
}


void hex_dump(char *str, void *addr, int size)
{
	int i;
	u8 *p = (u8*)addr;

	if(str)
		printk("%s:\n", str);

	i=0;
	printk("0x%04X ", i);

	for(; i<size; ++i) {
		if (i != 0 && i % 16 == 0) {
			printk("\n0x%04X ", i);
		}

		if (i != 0 && i % 8 == 0 && i % 16 != 0) {
			printk("- ");
		}

		printk("%02X ", p[i]);
	}

	printk("\n\n");
}

