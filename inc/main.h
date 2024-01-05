
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


int int_request(int id, void *func, void *arg);
int int_priorit(int id, int priorit);
int int_enable(int id);
int int_disable(int id);



int rtc_getcnt(void);


void uart0_init(int baudrate);
int  getc(int tmout);
void putc(int ch);
void puts(char *str);


void gpio_af(int group, int bit, int en);
void gpio_dir(int group, int bit, int dir);
void gpio_set(int group, int bit, int val);
int  gpio_get(int group, int bit);


void device_init(void);
void trap_init(void);

/******************************************************************************/

int sd_identify(void);
int sd_read_blocks(u32 block, int count, u8 *buf);
int sd_write_blocks(u32 block, int count, u8 *buf);



int printk(char *fmt, ...);
int sprintk(char *sbuf, const char *fmt, ...);
int snprintf(char *sbuf, int len, const char *fmt, ...);
void hex_dump(char *str, void *addr, int size);


unsigned int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, unsigned int n);
int strcasecmp(const char *s1, const char *s2);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, unsigned int n);
char *strchr(const char *s1, int ch);
u32 strtoul(char *str, char **endptr, int requestedbase, int *ret);

void *memset(void *s, int v, unsigned int n);
void *memcpy(void *to, const void *from, unsigned int n);
int memcmp(const void *dst, const void *src, unsigned int n);


void simple_shell(void);


/******************************************************************************/






#endif

