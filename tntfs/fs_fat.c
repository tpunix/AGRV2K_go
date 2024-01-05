//
// fs_fat.c
//
// simple FAT filessytem library
// writen by tpu, 2019-7-1
//

#include "main.h"
#include "tntfs.h"


///////////////////////////////////////////////////////////////////////////////


#define FS_FAT12 1
#define FS_FAT16 2
#define FS_FAT32 3


#define DO_SEEK  0
#define DO_READ  1
#define DO_WRITE 2


typedef struct _fatdir_t {
	char name[8];
	char ext[3];
	u8 attr;
	u8 eattr;
	u8 ctime_ms;
	u16 ctime;
	u16 cdate;
	u16 adate;
	u16 cluster_high;
	u16 mtime;
	u16 mdate;
	u16 cluster;
	u32 size;
}FATDIR;


typedef struct _fatfs_t {
	BLKDEV *bdev;
	int part;

	int type;
	int cluster_size;
	int root_size;

	u64 lba_base;
	u32 lba_dbr;
	u32 lba_fat1;
	u32 lba_fat2;
	u32 lba_root;
	u32 lba_data;

	FATDIR cur_dir;     // 当前操作的文件
	SIZE_T f_offset;    // 文件位置
	u32 f_cluster;      // 文件位置对应的簇号

	u8 *fbuf;           // FAT缓存
	int fat_sector;

	u8 *dbuf;           // 簇数据缓存
	int data_cluster;
	int d_dirty;

}FATFS;




///////////////////////////////////////////////////////////////////////////////

static u8 lfn_unicode[256];

static void add_lfn(u8 *buf)
{
	int index;

	index = (buf[0]&0x1f) - 1;
	index *= 26;
	memcpy(lfn_unicode+index+ 0, buf+ 1, 10);
	memcpy(lfn_unicode+index+10, buf+14, 12);
	memcpy(lfn_unicode+index+22, buf+28,  4);
	if(buf[0]&0x40){
		*(u16*)(lfn_unicode+index+26) = 0;
	}

	if(index==0){
		lfn_utf8_len = unicode_to_utf8((u16*)lfn_unicode, 128, lfn_utf8);
	}
}


static void make_dosname(char *buf, char *name)
{
	char ch;
	int i, d, len, mlen, elen;

	memset(buf, 0x20, 11);
	len = strlen(name);

	d = 0;
	mlen = 0;
	elen = 0;
	for(i=0; i<len; i++){
		ch = name[i];
		if(ch=='.'){
			d = 8;
			continue;
		}

		if(d<8){
			mlen += 1;
		}else{
			elen += 1;
		}
		if((d>=8 && mlen==0) || mlen>8 || elen>3){
			memset(buf, 0x20, 11);
			return;
		}

		if(ch>='a' && ch<='z')
			ch = ch-'a'+'A';
		buf[d] = ch;
		d += 1;
	}

}


///////////////////////////////////////////////////////////////////////////////

static void dump_dir_info(FATDIR *dir)
{
	int y, m, d, h, mi;
	char base[12], ext[4];
	int i;

	y = (dir->mdate>>9)+1980;
	m = (dir->mdate>>5)&0x0f;
	d = (dir->mdate&0x1f);
	h = (dir->mtime>>11);
	mi= (dir->mtime>>5)&0x3f;
	printk("  %04d-%02d-%02d %02d:%02d  ", y, m, d, h, mi);

	memcpy(base, dir->name, 8);
	memcpy(ext , dir->ext , 3);

	base[8] = 0;
	for(i=7; i>=0; i--){
		if(base[i]==0x20)
			base[i] = 0;
		else
			break;
	}

	ext[3] = 0;
	if(ext[2]==0x20) ext[2] = 0;
	if(ext[1]==0x20) ext[1] = 0;
	if(ext[0]==0x20) ext[0] = 0;

	if(dir->attr&0x10){
		printk("           [");
	}else{
		printk("%9d  ", dir->size);
	}

	if(dir->ctime_ms){
		printk("%s", lfn_utf8);
	}else{
		printk("%s", base);
		if(ext[0])
			printk(".%s", ext);
	}

	if(dir->attr&0x10)
		printk("]");
	printk("\n");
}

static int read_fat(FATFS *fatfs, int sector, int length)
{
	int retv;

	if(fatfs->fat_sector != sector){
		u32 lba_addr = fatfs->lba_base+fatfs->lba_fat1+sector;
		retv = fatfs->bdev->read_sector(fatfs->bdev->itf, fatfs->fbuf, lba_addr, length);
		if(retv)
			return retv;
		fatfs->fat_sector = sector;
	}

	return 0;
}


// 返回下一个cluster
static u32 next_cluster(FATFS *fatfs, u32 cluster)
{
	int retv;
	int sector, offset;
	int next_cluster;

	if(fatfs->type==FS_FAT12){
		sector = (cluster*3)>>10;
		offset = (cluster*3)&0x3ff;
		retv = read_fat(fatfs, sector, 2);
		if(retv<0)
			return 0;

		next_cluster = *(u16*)(fatfs->fbuf+(offset/2));
		if(offset&1){
			next_cluster >>= 4;
		}else{
			next_cluster &= 0x0fff;
		}
		if(next_cluster>=0x0ff8){
			next_cluster = 0;
		}

	}else if(fatfs->type==FS_FAT16){
		sector = cluster>>8;
		offset = cluster&0xff;

		retv = read_fat(fatfs, sector, 1);
		if(retv<0)
			return 0;

		next_cluster = *(u16*)(fatfs->fbuf+offset*2);
		if(next_cluster>=0xfff8){
			next_cluster = 0;
		}
	}else{
		sector = cluster>>7;
		offset = cluster&0x7f;
		retv = read_fat(fatfs, sector, 1);
		if(retv<0)
			return 0;

		next_cluster = *(u32*)(fatfs->fbuf+offset*4);
		if(next_cluster>=0x0ffffff8){
			next_cluster = 0;
		}
	}

	return next_cluster;
}


static u32 read_cluster(FATFS *fatfs, u32 cluster)
{
	if(fatfs->data_cluster != cluster){
		u32 lba_addr = fatfs->lba_base + fatfs->lba_data + fatfs->cluster_size * (cluster-2);
		int retv;

		retv = fatfs->bdev->read_sector(fatfs->bdev->itf, fatfs->dbuf, lba_addr, fatfs->cluster_size);
		if(retv)
			return -EIO;

		fatfs->data_cluster = cluster;
		fatfs->d_dirty = 0;
		
	}

	return 0;
}


static u32 write_cluster(FATFS *fatfs, u32 cluster)
{
	u32 lba_addr = fatfs->lba_base + fatfs->lba_data + fatfs->cluster_size * (cluster-2);
	int retv;

	retv = fatfs->bdev->write_sector(fatfs->bdev->itf, fatfs->dbuf, lba_addr, fatfs->cluster_size);
	if(retv)
		return -EIO;

	if(fatfs->data_cluster == cluster){
		fatfs->d_dirty = 0;
	}

	return 0;
}


static void *fat_find(void *fs_super, void *in_dir, char *name, char *next, int do_list)
{
	FATFS *fatfs = (FATFS*)fs_super;
	FATDIR *dir = (FATDIR*)in_dir;
	u32 dir_cluster;
	int i, j, retv, buf_size;
	int have_lfn, list_cnt=0;
	char name83[12];

	if(do_list){
		if(dir && (dir->attr&0x10)==0){
			// 是普通文件
			dump_dir_info(dir);
			return (void*)1;
		}
	}

	if(fatfs->type==FS_FAT32){
		buf_size = fatfs->cluster_size*512;
	}else{
		buf_size = 512;
	}

	if(dir==NULL){
		// find in root
		dir_cluster = (fatfs->type==FS_FAT32) ? fatfs->lba_root : 0;
	}else{
		dir_cluster = dir->cluster;
		if(fatfs->type!=FS_FAT12)
			dir_cluster |= (dir->cluster_high<<16);
	}

	if(name)
		make_dosname(name83, name);

	i = 0;
	have_lfn = 0;
	while(1){
		if(dir_cluster==0){
			// FAT12/16 root record
			retv = fatfs->bdev->read_sector(fatfs->bdev->itf, fatfs->dbuf, fatfs->lba_base+fatfs->lba_root+i, 1);
		}else{
			retv = read_cluster(fatfs, dir_cluster);
		}
		if(retv<0)
			return NULL;

		for(j=0; j<buf_size; j+=32){
			dir = (FATDIR*)(fatfs->dbuf+j);
			if(dir->name[0]==0 || (u8)dir->name[0]==0xe5)
				continue;
			if(dir->attr==0x08 || dir->attr==0x28)
				continue;

			if(dir->attr==0x0f){
				if(dir->name[0]&0x40){
					have_lfn = 1;
				}
				if(have_lfn){
					add_lfn(fatfs->dbuf+j);
				}
				continue;
			}

			if(have_lfn)
				dir->ctime_ms = 0xff;
			else
				dir->ctime_ms = 0x00;

			if(do_list){
				list_cnt += 1;
				dump_dir_info(dir);
			}else{
				if(have_lfn && strcasecmp(name, (char*)lfn_utf8)==0){
					goto _found;
				}else if(strncmp(name83, dir->name, 11)==0) {
					goto _found;
				}
			}
			have_lfn = 0;
		}

		if(dir_cluster==0){
			i += 1;
			if(i==fatfs->root_size)
				break;
		}else{
			dir_cluster = next_cluster(fatfs, dir_cluster);
			if(dir_cluster==0)
				break;
		}
	}

	if(do_list)
		return (void*)list_cnt;

	return NULL;

_found:

	if( (next && *next) && (dir->attr&0x10)==0){
		// 还有下一级目录, 但本级找到了一个普通文件
		return NULL;
	}

	memcpy(&fatfs->cur_dir, dir, sizeof(FATDIR));
	return &fatfs->cur_dir;
}


///////////////////////////////////////////////////////////////////////////////


static int fat_open(void *fs_super, void *fd)
{
	FATFS *fatfs = (FATFS*)fs_super;

	fatfs->f_offset = 0;
	fatfs->f_cluster = 0;

	return 0;
}


static int fat_close(void *fs_super, void *fd)
{
	return 0;
}


static SIZE_T fat_lseek(void *fs_super, void *fd, SIZE_T offset, int whence)
{
	FATFS *fatfs = (FATFS*)fs_super;
	FATDIR *dir = (FATDIR*)fd;

	if(whence==SEEK_SET){
	}else if(whence==SEEK_CUR){
		offset += fatfs->f_offset;
	}else if(whence==SEEK_END){
		offset += dir->size;
	}else{
		return -EINVAL;
	}

	if(offset<0 || offset>=dir->size){
		return -EINVAL;
	}

	fatfs->f_offset = offset;
	fatfs->f_cluster = 0;

	return offset;
}


static int fat_read(void *fs_super, void *fd, void *in_buf, int size)
{
	FATFS *fatfs = (FATFS*)fs_super;
	FATDIR *dir = (FATDIR*)fd;
	u8 *buf = in_buf;
	int vcn, vcn_offset, vcn_size;

	if(fatfs->f_offset+size > dir->size){
		size = dir->size - fatfs->f_offset;
	}

	vcn_size = fatfs->cluster_size*512;
	vcn = fatfs->f_offset / vcn_size;        // 虚拟簇号
	vcn_offset = fatfs->f_offset % vcn_size; // 簇内位置

	if(fatfs->f_cluster==0){
		// 定位到f_offset对应的簇
		int i;
		fatfs->f_cluster = dir->cluster;
		if(fatfs->type!=FS_FAT12)
			fatfs->f_cluster |= (dir->cluster_high<<16);

		for(i=0; i<vcn; i++){
			fatfs->f_cluster = next_cluster(fatfs, fatfs->f_cluster);
		}
	}

	int remain = size;
	while(remain && fatfs->f_cluster){
		int read_size = vcn_size - vcn_offset;
		if(read_size<remain)
			read_size = remain;

		read_cluster(fatfs, fatfs->f_cluster);
		memcpy(buf, fatfs->dbuf + vcn_offset, read_size);

		buf += read_size;
		remain -= read_size;
		vcn_offset += read_size;
		fatfs->f_offset += read_size;
		if(vcn_offset >= vcn_size){
			vcn_offset = 0;
			fatfs->f_cluster = next_cluster(fatfs, fatfs->f_cluster);
		}
	}

	size -= remain;
	return size;
}


static int fat_write(void *fs_super, void *fd, void *in_buf, int size)
{
	// TODO 只支持覆盖写入
	return -ENOSYS;
}


static int fat_stat(void *fs_super, void *fd, STAT_T *st)
{
	FATDIR *dir = (FATDIR*)fd;

	st->size = dir->size;

	st->flags = dir->attr&0x27;
	if(dir->attr&0x10)
		st->flags |= S_DIR;

	st->ctime.year = (dir->cdate>>9)+1980;
	st->ctime.mon  = (dir->cdate>>5)&0x0f;
	st->ctime.d = (dir->cdate&0x1f);
	st->ctime.h = (dir->ctime>>11);
	st->ctime.m = (dir->ctime>>5)&0x3f;
	st->ctime.s = 0;

	st->mtime.year = (dir->mdate>>9)+1980;
	st->mtime.mon  = (dir->mdate>>5)&0x0f;
	st->mtime.d = (dir->mdate&0x1f);
	st->mtime.h = (dir->mtime>>11);
	st->mtime.m = (dir->mtime>>5)&0x3f;
	st->mtime.s = 0;

	return 0;
}


///////////////////////////////////////////////////////////////////////////////

#if 1
static void dump_bpb(u8 *buf, int fat_type)
{
	printk("\nBPB info:\n");
	buf[11] = 0;
	printk("  OEM name: %s\n", buf+3);
	printk("  sector size: %d\n", le16(buf+0x0b));
	printk("  cluster size: %d\n", *(u8*)(buf+0x0d));
	printk("  reserved sectors: %d\n", le16(buf+0x0e));
	printk("  FAT numbers: %d\n", *(u8*)(buf+0x10));
	printk("  root entries: %d\n", le16(buf+0x11));
	printk("  total sectors: %d\n", le16(buf+0x13));
	printk("  Media: %02x\n", *(u8*)(buf+0x15));
	printk("  sectors per FAT: %d\n", *(u16*)(buf+0x16));
	printk("  sectors per Track: %d\n", *(u16*)(buf+0x18));
	printk("  heads per Cylinder: %d\n", *(u16*)(buf+0x1a));
	printk("  hidden sectors: %d\n", *(u32*)(buf+0x1c));
	printk("  total sectors: %d\n", *(u16*)(buf+0x20));

	if(fat_type==FS_FAT32){
		printk("FAT32 BPB2 info:\n");
		printk("  sectors per FAT32: %d\n", *(u32*)(buf+0x24));
		printk("  extended flags: %04x\n", *(u16*)(buf+0x28));
		printk("  FS version: %04x\n", *(u16*)(buf+0x2a));
		printk("  root cluster: %08x\n", *(u16*)(buf+0x2c));
		printk("  FSinfo sectors: %04x\n", *(u16*)(buf+0x30));
		printk("  backup boot sectors: %04x\n", *(u16*)(buf+0x32));
		printk("  drive number: %02x\n", *(u8*)(buf+0x40));
		printk("  extended boot sig: %02x\n", *(u8*)(buf+0x42));
		printk("  Volume serial: %08x\n", le32(buf+0x43));
		buf[0x52] = 0;
		printk("  Volume label: %s\n", buf+0x47);
	}else{
		printk("FAT12/16 BPB2 info:\n");
		printk("  drive number: %02x\n", *(u8*)(buf+0x24));
		printk("  extended boot sig: %02x\n", *(u8*)(buf+0x26));
		printk("  Volume serial: %08x\n", le32(buf+0x27));
		buf[0x36] = 0;
		printk("  Volume label: %s\n", buf+0x2B);
	}
	logmsg("\n");
}
#endif


static void *fat_mount(BLKDEV *bdev, int part)
{
	FATFS *fatfs;
	u8 *buf;
	int retv, fat_type;
	u32 dbr_lba;

	dbr_lba = bdev->parts[part].lba_start;

	buf = FS_MALLOC(1024);
	retv = bdev->read_sector(bdev->itf, buf, dbr_lba, 1);
	if(retv<0){
		FS_FREE(buf);
		return NULL;
	}

	if(strncmp((char*)buf+0x36, "FAT12", 5)==0){
		fat_type = FS_FAT12;
	}else if(strncmp((char*)buf+0x36, "FAT16", 5)==0){
		fat_type = FS_FAT16;
	}else if(strncmp((char*)buf+0x52, "FAT32", 5)==0){
		fat_type = FS_FAT32;
	}else{
		//printk("Unknow FAT type!\n");
		FS_FREE(buf);
		return NULL;
	}

	dump_bpb(buf, fat_type);

	fatfs = (FATFS*)FS_MALLOC(sizeof(FATFS));
	memset(fatfs, 0, sizeof(FATFS));

	fatfs->bdev = bdev;
	fatfs->part = part;
	fatfs->type = fat_type;
	fatfs->lba_base = 0;
	fatfs->lba_dbr = dbr_lba;

	fatfs->lba_fat1 = fatfs->lba_dbr + le16(buf+0x0e);
	if(fat_type==FS_FAT32){
		fatfs->lba_fat2 = fatfs->lba_fat1 + *(u32*)(buf+0x24);
		fatfs->lba_data = fatfs->lba_fat2 + *(u32*)(buf+0x24);
		fatfs->lba_root = *(u32*)(buf+0x2c);
	}else{
		fatfs->lba_fat2 = fatfs->lba_fat1 + *(u16*)(buf+0x16);
		fatfs->lba_root = fatfs->lba_fat2 + *(u16*)(buf+0x16);
		fatfs->lba_data = fatfs->lba_root + ((le16(buf+0x11)+15)>>4);
	}

	fatfs->cluster_size = *(u8*)(buf+0x0d);
	fatfs->root_size = (le16(buf+0x11)+15)>>4;

	fatfs->fbuf = buf;
	fatfs->dbuf = (u8*)FS_MALLOC(fatfs->cluster_size*512);

	fatfs->fat_sector = -1;
	fatfs->data_cluster = -1;

	return fatfs;
}


static int fat_umount(void *fs_super)
{
	FATFS *fatfs = (FATFS*)fs_super;

	FS_FREE(fatfs->dbuf);
	FS_FREE(fatfs->fbuf);
	FS_FREE(fatfs);

	return 0;
}


///////////////////////////////////////////////////////////////////////////////

FSDESC fs_fat = {
	.name  = "FATFS",
	.mount = fat_mount,
	.umount= fat_umount,
	.find  = fat_find,
	.stat  = fat_stat,
	.open  = fat_open,
	.close = fat_close,
	.lseek = fat_lseek,
	.read  = fat_read,
	.write = fat_write, //
};

