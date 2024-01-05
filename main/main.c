

#include "main.h"
#include "xos.h"


/******************************************************************************/

SEM tsem;

void task8(void *arg)
{
	printk("task8 running!\n");

	while(1){
		int retv = xos_sem_take(&tsem, 150);
		printk("task_8: get sem! retv=%d\n", retv);
	}
}


void task1(void *arg)
{
	TCB *tcb = xos_task_self();
	int id = (long)arg;
	tcb->mask = 1<<(id%4);
	printk("task_%d running! mask=%02x\n", id, tcb->mask);

	int cnt = 0;
	while(1){
		printk("task_%d: cnt=%d\n", id, cnt);
		cnt += 1;
		xos_task_delay(200);
	}
}


void led_task(void *arg)
{
	while(1){
		gpio_set(2, 0, 0);
		xos_task_delay(50);
		gpio_set(2, 0, 1);
		xos_task_delay(50);
	}
}


void main_task(void *arg)
{
	printk("main task running!\n");

	xos_task_create("led", led_task, (void*)(long)8, DEFAULT_TASK_PRIO, 0);

#if 0
	int i;

	for(i=0; i<8; i++){
		char name[16];
		sprintk(name, "task_%d", i);
		xos_task_create(name, task1, (void*)(long)i, DEFAULT_TASK_PRIO, 0);
	}

	xos_task_create("task_8", task8, (void*)(long)8, DEFAULT_TASK_PRIO, 0);


	xos_sem_init(&tsem, 0);

	int cnt = 0;
	while(1){
		printk("\nmain: cnt=%d\n", cnt);
		cnt += 1;
		xos_task_delay(100);
		if((cnt%8)==0){
			xos_sem_give(&tsem);
		}
	}
#endif

	simple_shell();
}

/******************************************************************************/


int main(void)
{
	system_init();
	device_init();


	puts("\n\nAGRV2K start!\n");
	printk(" mstatus: %08x\n", read_csr(mstatus));
	printk("   mtvec: %08x\n", read_csr(mtvec));
	printk("    mie: %08x\n", read_csr(mie));
	printk("    mip: %08x\n", read_csr(mip));

	xos_init(main_task);

	return 0;
}


/******************************************************************************/

