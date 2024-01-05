
#include "main.h"
#include "xos.h"
#include "tntfs.h"
#include "usbh_core.h"
#include "usbh_msc.h"

/******************************************************************************/

void USBH_IRQHandler(void);

int usb_hc_low_level_init()
{
	SYS->AHB_CLKENABLE |= AHB_MASK_USB0;
	usb_osal_msleep(10);

	USB->USBMODE |= 0x03;  // Host mode
	USB->OTGSC |= 0x44; // Auto reset

	int_request(USB0_IRQn, USBH_IRQHandler, NULL);
	int_enable(USB0_IRQn);

	return 0;
}

int usb_hc_low_level2_init(void)
{
	USB->USBMODE |= 0x03;  // Host mode
	USB->OTGSC |= 0x44; // Auto reset
	return 0;
}


u8 usbh_get_port_speed(const u8 port)
{
	u8 speed = (USB->PORTSC>>26)&3;
	if (speed == 0x00) {
		return USB_SPEED_FULL;
	}
	if (speed == 0x01) {
		return USB_SPEED_LOW;
	}
	if (speed == 0x02) {
		return USB_SPEED_HIGH;
	}

	return 0;
}


int cherryusb_start(void)
{
	return usbh_initialize();
}


/******************************************************************************/


static int usb_msc_read_sector(void *itf, u8 *buf, u32 start, int size)
{
	return usbh_msc_scsi_read10((struct usbh_msc*)itf, start, buf, size);
}


static int usb_msc_write_sector(void *itf, u8 *buf, u32 start, int size)
{
	return usbh_msc_scsi_write10((struct usbh_msc*)itf, start, buf, size);
}


void usbh_msc_run(struct usbh_msc *msc_class)
{
	BLKDEV *bdev;
	int retv;

	bdev = xos_malloc(sizeof(BLKDEV));
	memset(bdev, 0, sizeof(BLKDEV));
	msc_class->user = bdev;

	bdev->itf = msc_class;
	bdev->sectors = msc_class->blocknum;
	bdev->read_sector  = usb_msc_read_sector;
	bdev->write_sector = usb_msc_write_sector;

	retv = f_initdev(bdev, "usb", 0);
	if(retv)
		return;

	retv = f_mount(bdev, 0);
	if(retv==0){
		f_list("usb0:/");
		printk("\n");
	}
}


void usbh_msc_stop(struct usbh_msc *msc_class)
{
	BLKDEV *bdev = (BLKDEV*)msc_class->user;

	if(bdev){
		f_removedev(bdev);
		xos_free(bdev);
	}
}


/******************************************************************************/


