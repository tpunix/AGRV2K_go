/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"
#include "xos.h"

/******************************************************************************/

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
	task_t task_handle = xos_task_create((char*)name, entry, args, prio+DEFAULT_TASK_PRIO-4, stack_size);
	return (usb_osal_thread_t)task_handle;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    xos_task_exit();
}

/******************************************************************************/

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
	SEM *sem = NULL;

	sem = xos_malloc(sizeof(SEM));
	xos_sem_init(sem, initial_count);

	return (usb_osal_sem_t)sem;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
	xos_free(sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
	if (timeout == USB_OSAL_WAITING_FOREVER) {
		return xos_sem_take((SEM*)sem, -1);
	} else {
		return xos_sem_take((SEM*)sem, timeout);
	}
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
	xos_sem_give((SEM*)sem);
	return 0;
}

/******************************************************************************/

int usb_osal_enter_critical_section(void)
{
    return spin_lock_irq();
}

void usb_osal_leave_critical_section(int flag)
{
    spin_unlock_irq(flag);
}

void usb_osal_msleep(uint32_t delay)
{
	delay /= 10;
	delay += 1;
	xos_task_delay(delay);
}

/******************************************************************************/

