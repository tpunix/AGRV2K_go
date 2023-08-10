
#ifndef _MAIN_H_
#define _MAIN_H_

#include "agrv.h"
#include "riscv.h"

typedef unsigned  char u8;
typedef unsigned short u16;
typedef unsigned   int u32;
typedef unsigned long long u64;

#define NULL ((void*)0)

#define REG(x) (*(volatile unsigned int*)(x))

/******************************************************************************/



#define SYSCLK_FREQ 200000000

/******************************************************************************/


void system_init(void);

#define TIMER_HZ 1000000

u64 get_mcycle(void);
void timer_init(void);
void reset_timer(void);
u32 get_timer(void);
void udelay(int us);
void mdelay(int ms);

int rtc_getcnt(void);


void uart0_init(int baudrate);
int  getc(int tmout);
void putc(int ch);
void puts(char *str);

/******************************************************************************/



int printk(char *fmt, ...);
int sprintk(char *sbuf, const char *fmt, ...);
void hex_dump(char *str, void *addr, int size);


int strlen(char *s);
int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, int n);
char *strcpy(char *dst, char *src);
char *strncpy(char *dst, char *src, int n);
char *strchr(char *s1, int ch);
u32 strtoul(char *str, char **endptr, int requestedbase, int *ret);

void *memset(void *s, int v, int n);
void *memcpy(void *to, void *from, int n);
int memcmp(void *dst, void *src, int n);


void simple_shell(void);


/******************************************************************************/






#endif

