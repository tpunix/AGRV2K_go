
#ifndef _XOS_H_
#define _XOS_H_

#include <stdint.h>
#include <stdarg.h>
#include "main.h"

/******************************************************************************/

#define MAX_PRIORITY  8

#define DEFAULT_STACK_SIZE  4096
#define DEFAULT_TASK_PRIO   4

#define HZ 100


typedef void (*ENTRY_FN)(void*);
typedef struct tcb_t *task_t;
typedef struct tcb_t TCB;


typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long  u64;

#ifndef NULL
#define NULL	(void*)0
#endif

/******************************************************************************/

typedef struct list_node_t {
	struct list_node_t *next;
	struct list_node_t *prev;
}LIST_NODE;

typedef struct list_t {
	struct list_node_t *head;
	int lock;
}LIST;


void list_init(LIST *list);
void list_add(LIST *list, LIST_NODE *node);
void list_remove(LIST *list, LIST_NODE *node);
void list_insert_order(LIST *list, LIST_NODE *node, int (*cmp_func)(void *, void*));

#define list_entry(ptr, type, member)  (type*)( ((uint8_t*)(ptr)) - __builtin_offsetof(type, member) )

#define atomic_add(a, b)  __sync_add_and_fetch((a), (b))
#define atomic_sub(a, b)  __sync_sub_and_fetch((a), (b))

int spin_lock_irq(void);
void spin_unlock_irq(int key);
void spin_lock(int *val);
void spin_unlock(int *val);


/******************************************************************************/


#if defined(ARCH_ARMV7)
#elif defined(ARCH_ARMV7M)
#elif defined(ARCH_RISCV)
typedef struct {
	u32 x1;   // ra
	u32 x2;   // sp
	u32 x3;   // gp
	u32 x4;   // tp
	u32 x5;   // t0
	u32 x6;   // t1
	u32 x7;   // t2
	u32 x8;   // s0/fp
	u32 x9;   // s1
	u32 x10;  // a0
	u32 x11;  // a1
	u32 x12;  // a2
	u32 x13;  // a3
	u32 x14;  // a4
	u32 x15;  // a5
	u32 x16;  // a6
	u32 x17;  // a7
	u32 x18;  // s2
	u32 x19;  // s3
	u32 x20;  // s4
	u32 x21;  // s5
	u32 x22;  // s6
	u32 x23;  // s7
	u32 x24;  // s8
	u32 x25;  // s9
	u32 x26;  // s10
	u32 x27;  // s11
	u32 x28;  // t3
	u32 x29;  // t4
	u32 x30;  // t5
	u32 x31;  // t6
	u32 pc;
}RISCV_REGS;


typedef struct tcb_arch_t {
	RISCV_REGS regs;
}TCB_ARCH;
#endif


/******************************************************************************/







/******************************************************************************/


// 任务状态
#define T_IDLE    0
#define T_READY   1
#define T_RUN     2
#define T_WAIT    3
#define T_DEAD    4


typedef struct tcb_t {
	TCB_ARCH arch;
	void (*entry)(void*);
	void *arg;
	u8 *stack; 
	int stack_size;

	u8 prio;
	u8 state;
	u8 mask;
	u8 tid;

	int timeout;
	int retv;

	LIST_NODE node;
	char name[16];
}TCB;


typedef struct sem_t {
	int lock;
	int value;
	TCB *tcb;
}SEM;


extern LIST g_list_ready;
extern LIST g_list_sleep;
extern LIST g_list_dead;


extern uint32_t g_ticks;


/******************************************************************************/

void _task_entry(void *arg);
void _idle_task(void *arg);
void _tick_handle(void);
void __die(void);

task_t xos_task_create(char *task_name, ENTRY_FN entry, void *arg, int priority, int stack_size);
void xos_task_exit(void);
void xos_task_delay(int ticks);
void xos_task_yield(void);
TCB *xos_task_self(void);
void xos_init(void *first_entry);


void xos_sem_init(SEM *sem, int val);
void xos_sem_give(SEM *sem);
int  xos_sem_take(SEM *sem, int timeout);

void xos_heap_init(u32 start, u32 size);
void *xos_malloc(int size);
void xos_free(void *ptr);

void dump_task(void);

/******************************************************************************/

int  arch_in_isr(void);
int  arch_irq_lock(void);
void arch_irq_unlock(int key);
void arch_irq_enable(void);
int  arch_task_init(TCB *tcb);
int  arch_sys_init(void);
void arch_timer_init(void);
void arch_idle(void);
void arch_putc(int);
void arch_puts(char *);
int  arch_getc(void);

void arch_swap_context(TCB *);
void arch_set_context(TCB *);

/******************************************************************************/

#endif

