
#include "main.h"
#include "xos.h"
#include "riscv.h"

LIST g_list_ready;
LIST g_list_sleep;
LIST g_list_dead;

TCB *task_current;

uint32_t g_ticks;
int nr_cpus;

/******************************************************************************/

#if defined(ARCH_RISCV)

extern int in_isr;

inline int arch_in_isr(void)
{
	return in_isr;
}


inline int arch_irq_lock(void)
{
	int mstat = clear_csr(mstatus, MSTATUS_MIE);
	return mstat&MSTATUS_MIE;
}


inline void arch_irq_unlock(int key)
{
	set_csr(mstatus, key&MSTATUS_MIE);
}


inline void arch_irq_enable(void)
{
	set_csr(mstatus, MSTATUS_MIE);
}


inline void arch_idle(void)
{
	WFI();
}

extern int __global_pointer$;

int arch_task_init(TCB *tcb)
{
	TCB_ARCH *arch = &tcb->arch;

	arch->regs.x10 = (u32)tcb;
	arch->regs.x2 = (u32)(tcb->stack + tcb->stack_size);
	arch->regs.x3 = (u32)&__global_pointer$;
	arch->regs.pc = (u32)_task_entry;

	return 0;
}


static u64 mtime_inc;
static u64 mtime_cmp;

static void mtime_irq(u32 *regs, void *arg)
{
	mtime_cmp += mtime_inc;
	CLINT->MTIMECMP_LO = mtime_cmp;
	CLINT->MTIMECMP_HI = mtime_cmp>>32;

	_tick_handle();
}


void arch_timer_init(void)
{
	mtime_inc = 10*1000;
	mtime_cmp = mtime_inc;

	SYS->MTIME_PSC = 0x40000000;
	CLINT->MTIME_LO = 0;
	CLINT->MTIME_HI = 0;
	CLINT->MTIMECMP_LO = mtime_cmp;
	CLINT->MTIMECMP_HI = mtime_cmp>>32;

	int_request(MTIMER_IRQn, mtime_irq, NULL);
	int_enable(MTIMER_IRQn);

	SYS->MTIME_PSC = 0x80000000 | (SYSCLK_FREQ/1000000 - 1); // 1us
}

#endif


/******************************************************************************/


inline void list_init(LIST *list)
{
	list->head = NULL;
	list->lock = 0;
}


inline void list_add(LIST *list, LIST_NODE *node)
{
	LIST_NODE *next = list->head;
	node->next = next;
	node->prev = NULL;
	list->head = node;
	if(next){
		next->prev = node;
	}
}


inline void list_remove(LIST *list, LIST_NODE *node)
{
	LIST_NODE *next = node->next;
	LIST_NODE *prev = node->prev;

	if(next){
		next->prev = prev;
		node->next = NULL;
	}

	if(prev){
		prev->next = next;
		node->prev = NULL;
	}else{
		list->head = next;
	}
}


// 按顺序插入。如果cmp返回1，则把node插入当前节点之前。
void list_insert_order(LIST *list, LIST_NODE *node, int (*cmp_func)(void *, void*))
{
	LIST_NODE *p = list->head;
	LIST_NODE *prev = NULL;
	while(1){
		if((p==NULL) || cmp_func(node, p)){
			node->next = p;
			node->prev = prev;
			if(p)
				p->prev = node;
			if(prev)
				prev->next = node;
			else
				list->head = node;
			break;
		}
		prev = p;
		p = p->next;
	}
}


int spin_lock_irq(void)
{
	return arch_irq_lock();
}


void spin_unlock_irq(int key)
{
	arch_irq_unlock(key);
}


/******************************************************************************/

TCB *xos_task_self(void)
{
	return task_current;
}


static int cmp_prio(void *a, void *b)
{
	TCB *ta = list_entry(a, TCB, node);
	TCB *tb = list_entry(b, TCB, node);

	return (ta->prio < tb->prio)? 1: 0;
}


void _task_ready(TCB *tcb)
{
	int key = spin_lock_irq();
	list_insert_order(&g_list_ready, &tcb->node, cmp_prio);
	tcb->state = T_READY;
	spin_unlock_irq(key);
}


void _task_free(TCB *tcb)
{
	xos_free(tcb);
}


/******************************************************************************/


void dump_task(void)
{
	LIST *list;
	LIST_NODE *p;

	printk("\n________________\n");

	printk("    ready(%d):", g_list_ready.lock);
	list = &g_list_ready;
	p = list->head;
	while(p){
		TCB *t = list_entry(p, TCB, node);
		printk(" ->(%s.%x)", t->name, t->state);
		p = p->next;
	}
	printk("\n");

	printk("    sleep(%d):", g_list_sleep.lock);
	list = &g_list_sleep;
	p = list->head;
	while(p){
		TCB *t = list_entry(p, TCB, node);
		printk(" ->(%s.%x)", t->name, t->state);
		p = p->next;
	}
	printk("\n");

	TCB *t = task_current;
	printk("   current: (%s.%x)\n", t->name, t->state);
	printk("\n");

	printk("________________\n");
}


//
//  : 用户空间，新task创建后
//  : 执行了semGive
//  : 在中断中，有睡眠的task变为ready了。
//
void _task_switch(int in_isr, int self)
{
	LIST *list = &g_list_ready;
	int key = spin_lock_irq();

	// 最高优先级任务
	TCB *high = list_entry(list->head, TCB, node);
	//printk("task_switch(%d): cur=%s high=%s\n", in_isr, task_current->name, high->name);
	if(high && self==0 && task_current->prio < high->prio)
		goto _exit;

	if(task_current->state==T_RUN){
		task_current->state = T_READY;
	}
	if(task_current->state==T_READY){
		list_insert_order(list, &task_current->node, cmp_prio);
	}

	// 将high从ready中移出
	list_remove(list, &high->node);
	high->state = T_RUN;

	if(in_isr==0){
		arch_swap_context(high);
	}else{
		task_current = high;
	}

_exit:
	spin_unlock_irq(key);
}


void __die(void)
{
	g_list_ready.lock = 1;

	dump_task();
	simple_shell();
	while(1){
		arch_idle();
	}
}


void _tick_handle(void)
{
	g_ticks += 1;

	LIST *list = &g_list_sleep;
	int key = spin_lock_irq();

	int need_sched = 0;
	while(1){
		LIST_NODE *p = list->head;
		if(p==NULL)
			break;

		TCB *t = list_entry(p, TCB, node);
		if(g_ticks < t->timeout)
			break;

		//printk("\n_tick_handle: g_ticks=%d tmout=%d tcb=%p(%s)\n", g_ticks, t->timeout, t, t->name);
		list_remove(list, p);
		t->timeout = 0;

		_task_ready(t);
		need_sched = 1;
	}

	spin_unlock_irq(key);

	if(need_sched){
		_task_switch(1, 0);
	}
}


/******************************************************************************/

int g_tid = 0;

task_t xos_task_create(char *task_name, ENTRY_FN entry, void *arg, int priority, int stack_size)
{
	TCB *tcb;

	if(stack_size==0)
		stack_size = DEFAULT_STACK_SIZE;

	tcb = (TCB*)xos_malloc(sizeof(TCB)+stack_size);
	memset(tcb, 0, sizeof(TCB)+stack_size);

	strcpy(tcb->name, task_name);
	tcb->stack = (u8*)(tcb+1);
	tcb->stack_size = stack_size;
	tcb->entry = entry;
	tcb->arg = arg;
	tcb->tid = g_tid;
	atomic_add(&g_tid, 1);
	tcb->prio = priority;
	tcb->mask = 0xff;
	arch_task_init(tcb);

	printk("xos_task_create: tid=%d  TCB=%p(%s)\n", tcb->tid, tcb, tcb->name);

	if(task_current==NULL){
		tcb->state = T_RUN;
		task_current = tcb;
	}else{
		_task_ready(tcb);
		_task_switch(0, 0);
	}

	return tcb;
}


void xos_task_exit(void)
{
	int key = spin_lock_irq();

	task_current->state = T_DEAD;
	list_add(&g_list_dead, &task_current->node);

	spin_unlock_irq(key);
	_task_switch(0, 1);
}


static int cmp_timeout(void *a, void *b)
{
	TCB *ta = list_entry(a, TCB, node);
	TCB *tb = list_entry(b, TCB, node);

	return (ta->timeout < tb->timeout)? 1: 0;
}


void xos_task_delay(int ticks)
{
	int key = spin_lock_irq();

	task_current->state = T_WAIT;
	task_current->timeout = g_ticks + ticks;
	list_insert_order(&g_list_sleep, &task_current->node, cmp_timeout);

	spin_unlock_irq(key);
	_task_switch(0, 1);
}


void xos_task_yield(void)
{
	int key = spin_lock_irq();

	task_current->state = T_READY;

	spin_unlock_irq(key);
	_task_switch(0, 1);
}


/******************************************************************************/

void xos_sem_init(SEM *sem, int val)
{
	sem->lock = 0;
	sem->value = val;
	sem->tcb = NULL;
}


// 释放信号量。可在中断中调用。
void xos_sem_give(SEM *sem)
{
	int key = spin_lock_irq();

	TCB *tcb = sem->tcb;
	sem->tcb = NULL;
	if(tcb){
		tcb->retv = 0;
		if(tcb->timeout > 0){
			list_remove(&g_list_sleep, &tcb->node);
			tcb->timeout = 0;
		}
	}else{
		atomic_add(&sem->value, 1);
	}

	spin_unlock_irq(key);

	if(tcb && tcb->state==T_WAIT){
		_task_ready(tcb);
		_task_switch(arch_in_isr(), 0);
	}
}


// 获取信号量。不可在中断中调用。
int xos_sem_take(SEM *sem, int timeout)
{

	if(sem->value){
		atomic_sub(&sem->value, 1);
		return 0;
	}
	if(timeout==0){
		return -1;
	}

	int key = spin_lock_irq();

	task_current->state = T_WAIT;

	// 再次检查，防止第一次检查到spin_lock之间value改变。
	if(sem->value){
		atomic_sub(&sem->value, 1);
		task_current->state = T_RUN;
		spin_unlock_irq(key);
		return 0;
	}

	sem->tcb = task_current;
	task_current->timeout = 0;
	if(timeout>0){
		task_current->timeout = g_ticks + timeout;
		task_current->retv = -1;
		list_insert_order(&g_list_sleep, &task_current->node, cmp_timeout);
	}

	spin_unlock_irq(key);
	_task_switch(0, 1);

	return task_current->retv;
}


/******************************************************************************/

void _log_task(void *arg);
void _pk_putc(int ch);

static void *_user_entry;

void _idle_task(void *arg)
{
	TCB *tcb = xos_task_self();

	printk("\n%s running! tcb=%p\n", tcb->name, tcb);

	arch_timer_init();

	xos_task_create("XosUser", _user_entry, NULL, DEFAULT_TASK_PRIO, 0);

	while(1){
		LIST *list = &g_list_dead;

		int key = spin_lock_irq();
		while(1){
			LIST_NODE *p = list->head;
			if(p==NULL)
				break;
			list_remove(list, p);
			TCB *t = list_entry(p, TCB, node);
			_task_free(t);
		}

		spin_unlock_irq(key);

		arch_idle();
	}

}


void _task_entry(void *arg)
{
	TCB *tcb = (TCB*)arg;
	tcb->entry(tcb->arg);
	xos_task_exit();
}


extern int _heap_start;
extern int _heap_size;

void xos_init(void *user_entry)
{
	_user_entry = user_entry;
	g_ticks = 0;

	list_init(&g_list_ready);
	list_init(&g_list_sleep);
	list_init(&g_list_dead);

	xos_heap_init((u32)&_heap_start, (u32)&_heap_size);
	trap_init();

	TCB *tcb = xos_task_create("IDLE", _idle_task, NULL, MAX_PRIORITY, 0);
	arch_set_context(tcb);
}

