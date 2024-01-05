//
// fs_util.c
//
// simple filessytem library
// writen by tpu, 2019-7-1
//

#include "main.h"
#include "tntfs.h"


///////////////////////////////////////////////////////////////////////////////


u8 lfn_utf8[256];
int lfn_utf8_len;


int unicode_to_utf8(u16 *u, int ulen, u8 *utf8)
{
	u16 uc;
	u8 *d = utf8;
	int len = 0;

	while(ulen){
		uc = *u++;
		ulen--;
		if(uc==0)
			break;

		if(uc<=0x007f){
			*d++ = uc;
			len += 1;
		}else if(uc<0x07ff){
			*d++ = 0xc0 | (uc>>6);
			*d++ = 0x80 | (uc&0x3f);
			len += 2;
		}else{
			*d++ = 0xe0 | (uc>>12);
			*d++ = 0x80 | ((uc>>6)&0x3f);
			*d++ = 0x80 | (uc&0x3f);
			len += 3;
		}
	}

	utf8[len] = 0;
	return len;
}


u16 le16(void *p)
{
	u8 *q = (u8*)p;
	return (q[1]<<8) | q[0];
}


u32 le32(void *p)
{
	u8 *q = (u8*)p;
	return (q[3]<<24) | (q[2]<<16) | (q[1]<<8) | q[0];
}


/******************************************************************************/

static BLKDEV *devlist[8];
static BLKDEV *current_dev;

u8 sector_buf[512];


int f_initdev(BLKDEV *bdev, char *devname, int index)
{
	int i, p, retv;
	u32 start, size;


	for(i=0; i<8; i++){
		if(devlist[i]==NULL){
			devlist[i] = bdev;
			break;
		}
	}
	if(i==8){
		return -1;
	}

	sprintk(bdev->name, "%s%d", devname, index);
	logmsg("\nBlock device %s: %d sectors.\n", bdev->name, bdev->sectors);

	// 先用parts[0]指向整个设备.
	bdev->parts[0].lba_start = 0;
	bdev->parts[0].lba_size = bdev->sectors;
	bdev->parts[1].lba_size = 0;
	bdev->parts[2].lba_size = 0;
	bdev->parts[3].lba_size = 0;

	retv = bdev->read_sector(bdev->itf, sector_buf, 0, 1);
	if(retv)
		return retv;
	if(sector_buf[510]!=0x55 || sector_buf[511]!=0xAA){
		return 0;
	}

	// 扫描4个主分区表项. 当前不支持扩展分区.
	p = 0;
	for(i=0; i<4; i++){
		u8 *pt = sector_buf+446+i*16;

		// 分区表项可以是全0
		if(le32(pt+0)==0 && le32(pt+4)==0 && le32(pt+8)==0 && le32(pt+12)==0)
			continue;

		start = le32(pt+8);
		size  = le32(pt+12);
		if((pt[0]&0x7f) || (size==0) || (start > bdev->sectors) || (start+size) > bdev->sectors){
			// 非法的分区表项. 可以认为没有分区表.
			return 0;
		}

		bdev->parts[p].lba_start = start;
		bdev->parts[p].lba_size = size;
		p += 1;
		logmsg("PART_%d: LBA=%08x SIZE=%08x\n", i, start, size);
	}

	return 0;
}


int f_removedev(BLKDEV *bdev)
{
	int i;

	for(i=0; i<4; i++){
		if(bdev->parts[i].fs){
			f_umount(bdev, i);
		}
	}

	for(i=0; i<8; i++){
		if(devlist[i]==bdev){
			devlist[i] = NULL;
			break;
		}
	}

	return 0;
}


BLKDEV *f_getdev(char *path, int *part, char **rest_path)
{
	char devname[8], *p;
	BLKDEV *bdev;
	int i;

	memcpy(devname, path, 8);
	p = strchr(devname, ':');
	if(p==NULL)
		return current_dev;

	*p = 0;
	p += 1;

	if(*p>='0' && *p<='9'){
		if(part){
			*part = *p - '0';
		}
		p += 1;
	}else{
		if(part){
			*part = 0;
		}
	}


	if(rest_path){
		*rest_path = p;
	}

	for(i=0; i<8; i++){
		bdev = devlist[i];
		if(bdev==NULL)
			continue;

		if(strcmp(devname, bdev->name)==0){
			return bdev;
		}
	}

	return NULL;
}


FSDESC *f_getfs(char *path, char **rest_path)
{
	BLKDEV *bdev;
	int part = 0;

	bdev = f_getdev(path, &part, rest_path);
	if(bdev==NULL)
		return NULL;

	if(part>3)
		return NULL;

	return bdev->parts[part].fs;
}


/******************************************************************************/


static FSD f_find(FSDESC *cfs, char *file)
{
	char tbuf[128], *p, *n;
	FSD dir;

	if(file==NULL)
		return NULL;

	strcpy(tbuf, file);
	p = tbuf;
	if(*p=='/')
		p++;

	dir = NULL;
	while(1){
		n = strchr(p, '/');
		if(n){
			*n = 0;
			n += 1;
		}

		dir = cfs->find(cfs->super, dir, p, n, 0);
		if(dir==NULL)
			return NULL;

		if(n==NULL || *n==0)
			break;
		p = n;
	}

	return dir;
}

int f_list(char *file)
{
	char *p;
	FSD fsd;
	FSDESC *cfs;

	logmsg("List %s\n", file);

	cfs = f_getfs(file, &file);
	if(cfs==NULL)
		return -ENODEV;

	p = file;
	if(p && *p=='/')
		p++;

	if(p==NULL || *p==0){
		fsd= 0;  // list root
	}else{
		fsd = f_find(cfs, file);
		if(fsd==NULL)
			return 0;
	}

	fsd = cfs->find(cfs->super, fsd, NULL, NULL, 1);
	logmsg("        Total %4d Files\n", (int)fsd);

	return (int)fsd;
}

int f_stat(char *file, STAT_T *st)
{
	FSD fsd;
	FSDESC *cfs;

	cfs = f_getfs(file, &file);
	if(cfs==NULL)
		return -ENODEV;

	fsd = f_find(cfs, file);
	if(fsd)
		return cfs->stat(cfs->super, fsd, st);

	return -ENOENT;
}


FD f_open(char *file)
{
	FSD fsd;
	FSDESC *cfs;

	cfs = f_getfs(file, &file);
	if(cfs==NULL)
		return (void*)-ENODEV;

	fsd = f_find(cfs, file);
	if(fsd==NULL)
		return (void*)-ENOENT;

	cfs->fsd = fsd;
	cfs->open(cfs->super, fsd);

	return &cfs->fsd;
}


static FSDESC *fd2fs(FD fd)
{
	return (FSDESC*)( (u32)fd - (u32)&((FSDESC*)0)->fsd );
}


int f_close(FD fd)
{
	FSDESC *cfs = fd2fs(fd);
	return cfs->close(cfs->super, (FSD)*fd);
}


SIZE_T f_lseek(FD fd, SIZE_T offset, int whence)
{
	FSDESC *cfs = fd2fs(fd);
	return cfs->lseek(cfs->super, (FSD)*fd, offset, whence);
}


int f_read(FD fd, void *buf, int size)
{
	FSDESC *cfs = fd2fs(fd);
	return cfs->read(cfs->super, (FSD)*fd, buf, size);
}


int f_write(FD fd, void *buf, int size)
{
	FSDESC *cfs = fd2fs(fd);
	return cfs->write(cfs->super, (FSD)*fd, buf, size);
}


/******************************************************************************/

extern FSDESC fs_fat;
extern FSDESC fs_ntfs;

FSDESC *fs_list[] = {
	&fs_fat,
	&fs_ntfs,
	NULL,
};


int f_mount(BLKDEV *bdev, int part)
{
	FSDESC *cfs, *newfs;
	void *fs_super;
	int i;

	if(part>3)
		return -EINVAL;

	for(i=0;;i++){
		cfs = fs_list[i];
		if(cfs==NULL)
			break;

		fs_super = cfs->mount(bdev, part);
		if(fs_super)
			break;
	}
	if(fs_super==NULL)
		return -ENOSYS;

	newfs = (FSDESC*)FS_MALLOC(sizeof(FSDESC));
	memcpy(newfs, cfs, sizeof(FSDESC));
	newfs->super = fs_super;
	bdev->parts[part].fs = newfs;

	return 0;
}


int f_umount(BLKDEV *bdev, int part)
{
	FSDESC *cfs;

	if(part>3)
		return -EINVAL;

	cfs = bdev->parts[part].fs;
	cfs->umount(cfs->super);
	FS_FREE(cfs);

	bdev->parts[part].fs = NULL;

	return 0;
}


/******************************************************************************/


