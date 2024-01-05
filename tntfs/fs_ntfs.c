//
// fs_ntfs.c
//
// simple NTFS filessytem library
// writen by tpu, 2019-7-1
//

#include "main.h"
#include "tntfs.h"

///////////////////////////////////////////////////////////////////////////////

/*
 *  LBA: 扇区编号。从磁盘的0扇区开始。
 *  LCN: 逻辑簇号。从分区开始。LBA = LCN * 簇大小
 *  VCN: 虚拟簇号。从文件开始。物理上可能是不连续的。
 *
 *  MFT: 主文件表。MFT由一项项的record组成，每一个(或多个)record记录一个文件。
 *       MFT本身也是文件。record0记录MFT。文件很多时，MFT也可能是不连续的。
 *
 *  record: 大小为1K。也可以为其他大小，但很少见。由一系列attr组成。常用attr:
 *    attr_20: ATTRIBUTE_LIST
 *      当一个文件由多个record描述时，attr_20保存attr列表及在哪个record中的位置等信息。
 *    attr_30: NAME
 *      描述文件名信息。一般有两个，第一个是长文件名，第二个是dos 8.3文件名。
 *    attr_80: DATA
 *      描述文件数据。当文件比较小时，数据直接放在attr里面。当数据比较大时，用runlist描述。
 *    attr_90: INDEX_ROOT
 *      目录B+树的根节点。小目录只需要根节点就可以描述了。
 *    attr_a0: INDEX_ALLOCATION
 *      目录B+树的叶节点描述。用runlist表示各个节点的位置。
 *
 */

/*
 * 有些文件比较特殊，用了多个file record，并且有多个80属性分布在后面的record中。
 *
 * record n+0: attr_10 attr_20 attr_30 attr_30
 *        n+1: attr_80 (vcn: 0000-mmmm  filesize: nnnn)
 *        n+2: attr_80 (vcn: mmmm-nnnn  filesize: 0000)
 *
 * 暂时不支持读取这类文件。
 *
 * 如何支持？解析20属性，把其中列出的属性提取，并统一放置在一处。需要重新设计当前的架构。
 * 据说还有非常驻的20属性，这就更复杂了。
 *
 */


/* 运行时runlist结构。覆盖存储在attr开头位置。start指向runlist数据。*/
typedef struct {
	u16 start;
	u16 next;
	u16 end;      // end对应attr的长度
	u16 zero;

	u32 LCN_start;
	u32 LCN;
	u32 LCN_size;
}RUNLIST;


typedef struct {
	u32 fd;
	u16 fd_high;
	u16 seq;
	u16 len;
	u16 content_len;
	u8  flag;
	u8  unuse[3];
}INDEX;


typedef struct {
	u64 parent_seq;
	u64 create_time;
	u64 modify_time;
	u64 mft_time;
	u64 access_time;
	u64 space_size;
	u64 file_size;
	u32 flags;
	u32 reparse;
	u8  name_len;
	u8  name_type;
	u8  name[2];
}NAME;

typedef struct {
	BLKDEV *bdev;
	int part;

	u64 lba_base;
	u32 lba_mft;
	int cluster_size;
	u8  *mft_80;            // 保存MFT自己的runlist. MFT可能是不连续的.

	u8 *mbuf;               // 当前的record信息
	int mbuf_index;
	u8 *attr_30;            // 文件名
	u8 *attr_80;            // 文件数据或runlist
	u8 *attr_90;            // 索引树的root节点
	u8 *attr_a0;            // 索引树的子节点的runlist

	NAME *name;
	SIZE_T f_offset;
	SIZE_T f_valid;

	u8 *dbuf;               // 当前簇的数据
	u32 data_lcn;
}NTFS;


/******************************************************************************/


/* Number of 100 nanosecond units from 1/1/1601 to 1/1/1970 */
#define EPOCH_BIAS  116444736000000000ull

static char mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static u32 div64(u64 a, u32 b)
{
	u64 t = (u64)b<<32;
	u32 r = 0;

	for(int i=0; i<32; i++){
		if(a>=t){
			a -= t;
			r |= 1;
		}
		t >>= 1;
		r <<= 1;
	}

	return r;
}

static void parse_ftime(u64 ftime, TIME *t)
{
	u32 d, h, m, s;
	u32 year, mon;

	if(ftime==0){
		memset(t, 0, sizeof(TIME));
		return;
	}

	ftime -= EPOCH_BIAS;
	s = div64(ftime, (10*1000*1000));

	d = s/(24*60*60); s -= d*24*60*60;
	h = s/(60*60); s -= h*60*60;
	m = s/60; s -= m*60;

	year = d/(365*4+1); d -= year*(365*4+1);
	mon = d/365; d -= mon*365;

	if(mon==3){
		if(d==0){
			mon = 2;
			d = 365;
		}else{
			d -= 1;
		}
	}
	year = 1970+year*4+mon;

	mdays[1] = (mon==2)? 29:28;
	mon = 0;
	while(d>mdays[mon]){
		d -= mdays[mon];
		mon += 1;
	}
	mon += 1;
	d += 1;

	t->year = year;
	t->mon  = mon;
	t->d = d;
	t->h = h;
	t->m = m;
	t->s = s;

}

///////////////////////////////////////////////////////////////////////////////


static void rl_reset(RUNLIST *rl)
{
	rl->next = 0;
}

static int rl_decode_next(RUNLIST *rl)
{
	u8 *buf;
	int i, cl, sl;
	int new_start, sbyte;

	if(rl->next==0){
		rl->next = rl->start;
		rl->LCN_start = 0;
	}

	buf = (u8*)rl;
	buf += rl->next;
	if(rl->next>=rl->end || buf[0]==0)
		return -1;

	sl = buf[0]&0x0f;
	cl = buf[0]>>4;
	rl->next += sl+cl+1;

	buf += 1;
	rl->LCN_size = 0;
	for(i=0; i<sl; i++){
		rl->LCN_size |= buf[i]<<(i*8);
	}

	buf += i;
	new_start = 0;
	for(i=0; i<cl; i++){
		sbyte = (char)buf[i];
		new_start &= (1<<i*8)-1;
		new_start |= sbyte<<(i*8);
	}
	rl->LCN_start += new_start;

	rl->LCN = 0;
	return 0;
}

// 返回下一个lcn
static u32 rl_next_lcn(RUNLIST *rl)
{
	if(rl->next==0 || rl->LCN==(rl->LCN_size - 1)){
		if(rl_decode_next(rl))
			return 0;
	}else{
		rl->LCN += 1;
	}

	return rl->LCN_start + rl->LCN;
}

// 从头开始查找vcn，并返回lcn
static u32 rl_seek_vcn(RUNLIST *rl, int vcn)
{
	int vcn_start;

	rl_reset(rl);

	vcn_start = 0;
	while(1){
		if(rl_decode_next(rl))
			return 0;

		if(vcn<(vcn_start+rl->LCN_size)){
			return rl->LCN_start + ( vcn - vcn_start);
		}

		vcn_start += rl->LCN_size;
	}

}


///////////////////////////////////////////////////////////////////////////////


/* record与index扇区最后的两个字节用于校验。真实的数据在头部保存。需要读取后恢复。*/
static void sector_fixup(u8 *buf)
{
	int p = *(u16*)(buf+4);
	int s = *(u16*)(buf+6);
	int i;

	if(*(u16*)(buf+p) == *(u16*)(buf+0x01fe)){
		for(i=0; i<(s-1); i++){
			*(u16*)(buf+i*0x0200+0x01fe) = *(u16*)(buf+p+2+i*2);
		}
	}
}

static void parse_record(NTFS *ntfs)
{
	int p;
	u8 *buf;
	RUNLIST *rl;
	NAME *name = NULL;

	sector_fixup(ntfs->mbuf);

	ntfs->attr_30 = NULL;
	ntfs->attr_80 = NULL;
	ntfs->attr_90 = NULL;
	ntfs->attr_a0 = NULL;

	p = 0x38;
	while(p<1024){
		buf = ntfs->mbuf+p;
		if(*(u32*)(buf)==0xffffffff)
			break;

		u32 id = *(u32*)(buf);
		if(id==0x30){
			u8 *dp = buf + *(u16*)(buf+0x14);

			if(dp[0x41]!=2 || ntfs->attr_30==NULL){
				ntfs->attr_30 = buf;
				name = (NAME*)(buf + *(u16*)(buf+0x14));
				ntfs->name = name;
			}
		}else if(id==0x80){
			ntfs->attr_80 = buf;
			rl = (RUNLIST*)buf;
			if(buf[8]){
				// 大文件，数据由runlist描述。
				rl->start = *(u16*)(buf+0x20);
				rl->next  = 0;

				name->space_size = *(u64*)(buf+0x28);
				name->file_size  = *(u64*)(buf+0x30);
			}else{
				// 小文件，数据在attr里面。
				name->space_size = *(u32*)(buf+0x04) - *(u16*)(buf+0x14);
				name->file_size  = *(u32*)(buf+0x10);
			}
		}else if(id==0x90){
			ntfs->attr_90 = buf;
		}else if(id==0xa0){
			ntfs->attr_a0 = buf;
			rl = (RUNLIST*)buf;
			rl->start = *(u16*)(buf+0x20);
			rl->next  = 0;
		}

		p += *(u32*)(buf+4);
	}
}

static int read_record(NTFS *ntfs, int index)
{
	RUNLIST *rl = (RUNLIST*)ntfs->mft_80;
	int retv;
	u32 lba, vcn, lcn, offset;

	if(ntfs->mbuf_index == index)
		return 0;

	vcn = (index*2) / ntfs->cluster_size;
	offset = (index*2) % ntfs->cluster_size;

	lcn = rl_seek_vcn(rl, vcn);
	if(lcn==0)
		return -1;
	lba = ntfs->lba_base + lcn*ntfs->cluster_size + offset;

	retv = ntfs->bdev->read_sector(ntfs->bdev->itf, ntfs->mbuf, lba, 2);
	if(retv)
		return retv;
	ntfs->mbuf_index = index;

	if(strncmp((char*)(ntfs->mbuf), "FILE", 4))
		return -1;

	parse_record(ntfs);
	return 0;
}

static int read_cluster(NTFS *ntfs, u32 lcn)
{
	u32 lba;
	int retv;

	if(ntfs->data_lcn == lcn)
		return 0;

	lba = ntfs->lba_base + lcn*ntfs->cluster_size;
	//logmsg("LCN %08x @ %08x\n", lcn, lba);

	retv = ntfs->bdev->read_sector(ntfs->bdev->itf, ntfs->dbuf, lba, ntfs->cluster_size);
	if(retv)
		return retv;

	return 0;
}


///////////////////////////////////////////////////////////////////////////////


static void dump_dir_info(NAME *n)
{
	TIME t;

	parse_ftime(n->modify_time, &t);

	logmsg("  %04d-%02d-%02d %02d:%02d  ", t.year, t.mon, t.d, t.h, t.m);
	//logmsg(" %08x ", n->flags);

	if(n->flags&0x10000000){
		logmsg("           [");
	}else{
		logmsg("%9d  ", (u32)n->file_size);
	}

	unicode_to_utf8((u16*)n->name, n->name_len, lfn_utf8);
	logmsg("%s", lfn_utf8);

	if(n->flags&0x10000000) logmsg("]");

	logmsg("\n");
}


/* e->fd是48位的。实际上只用了低32位。即使把32位全用完了，那整个MFT都有4T了。不太可能。 */
static int process_index(u8 *buf, int len, char *name, int do_list)
{
	INDEX *e;
	NAME *n;
	int p = 0;
	int retv = 0;

	while(p<len){
		e = (INDEX*)(buf+p);
		if(e->flag&2)
			break;

		n = (NAME*)(buf+p+0x10);
		if(e->fd>=16 && n->name_type!=2){
			if(do_list){
				dump_dir_info(n);
				retv += 1;
			}else{
				unicode_to_utf8((u16*)n->name, n->name_len, lfn_utf8);
				if(strcasecmp(name, (char*)lfn_utf8)==0){
					retv = e->fd;
					break;
				}
			}
		}

		p += e->len;
	}

	return retv;
}

// 在指定目录(in_dir)查找一个文件(name)。这里是按顺序搜索的，没有按B+树来搜索。
static void *ntfs_find(void *fs_super, void *in_dir, char *name, char *next, int do_list)
{
	NTFS *ntfs = (NTFS*)fs_super;
	RUNLIST *rl;
	int FD = (u32)in_dir;
	u8 *buf;
	int retv, p, len, list_cnt=0;
	u32 lcn;

	if(FD==0)
		FD = 5; // find in root

	retv = read_record(ntfs, FD);
	if(retv<0)
		return NULL;

	if(do_list){
		if(ntfs->mbuf[0x16]!=3){
			// 是普通文件
			dump_dir_info(ntfs->name);
			return (void*)1;
		}
	}

	// parse attr_90
	buf = ntfs->attr_90;
	len = *(u32*)(buf+4);
	p = 0x40;

	FD = process_index(buf+p, len-p, name, do_list);
	if(do_list){
		list_cnt += FD;
	}else if(FD){
		goto _found;
	}


	// parse attr_a0
	if(ntfs->attr_a0==NULL)
		goto _skip_a0;

	rl = (RUNLIST*)ntfs->attr_a0;
	rl_reset(rl);

	while(1){
		lcn = rl_next_lcn(rl);
		if(lcn==0)
			break;

		read_cluster(ntfs, lcn);
		buf = ntfs->dbuf;
		if(*(u32*)(buf)!=0x58444e49)
			continue;
		sector_fixup(buf);

		p = *(u32*)(buf+0x18);
		p += 0x18;

		FD = process_index(buf+p, 4096-p, name, do_list);
		if(do_list){
			list_cnt += FD;
		}else if(FD){
			goto _found;
		}
	}

_skip_a0:

	if(do_list)
		return (void*)list_cnt;

	return NULL;

_found:

	if( (next && *next) && (ntfs->mbuf[0x16]!=3)){
		// 还有下一级目录, 但本级找到了一个普通文件
		return NULL;
	}

	return (void*)FD;
}


///////////////////////////////////////////////////////////////////////////////

static int ntfs_open(void *fs_super, void *fd)
{
	NTFS *ntfs = (NTFS*)fs_super;
	int FD = (u32)fd;
	int retv;

	retv = read_record(ntfs, FD);
	if(retv<0)
		return -ENOENT;

	ntfs->f_offset = 0;
	ntfs->f_valid = *(SIZE_T*)(ntfs->attr_80+0x38);

	return 0;
}

static int ntfs_close(void *fs_super, void *fd)
{
	return 0;
}


static SIZE_T ntfs_lseek(void *fs_super, void *fd, SIZE_T offset, int whence)
{
	NTFS *ntfs = (NTFS*)fs_super;
	SIZE_T file_size = ntfs->name->file_size;

	if(whence==SEEK_SET){
	}else if(whence==SEEK_CUR){
		offset += ntfs->f_offset;
	}else if(whence==SEEK_END){
		offset += file_size;
	}else{
		return -EINVAL;
	}

	if(offset<0 || offset>=file_size){
		return -EINVAL;
	}

	ntfs->f_offset = offset;

	return offset;
}


static int ntfs_read(void *fs_super, void *fd, void *in_buf, int size)
{
	NTFS *ntfs = (NTFS*)fs_super;
	u8 *buf = in_buf;

	if(ntfs->attr_80==NULL)
		return -EBADF;


	if(ntfs->f_offset+size > ntfs->name->file_size){
		size = ntfs->name->file_size - ntfs->f_offset;
	}

	if(ntfs->attr_80[8]==0){
		// 小文件
		int p = *(u16*)(ntfs->attr_80+0x14);
		p += ntfs->f_offset;
		memcpy(buf, ntfs->attr_80+p, size);
		ntfs->f_offset += size;
	}else{
		// 大文件
		u32 lcn, vcn, vcn_offset, vcn_size;
		RUNLIST *rl = (RUNLIST*)ntfs->attr_80;

		vcn_size = ntfs->cluster_size*512;
		vcn = ntfs->f_offset / vcn_size;
		vcn_offset = ntfs->f_offset % vcn_size;

		lcn = rl_seek_vcn(rl, vcn);
		if(lcn==0){
			return -EBADF;
		}

		int remain = size;
		while(remain){
			int read_size = vcn_size - vcn_offset;
			if(read_size<remain)
				read_size = remain;

			if(ntfs->f_offset >= ntfs->f_valid){
				memset(buf, 0, read_size);
			}else{
				read_cluster(ntfs, lcn);
				memcpy(buf, ntfs->dbuf + vcn_offset, read_size);
			}

			buf += read_size;
			remain -= read_size;
			vcn_offset += read_size;
			ntfs->f_offset += read_size;
			if(vcn_offset >= vcn_size){
				vcn_offset = 0;
				lcn = rl_next_lcn(rl);
			}
		}
	}

	return size;
}


static int ntfs_write(void *fs_super, void *fd, void *buf, int size)
{
	// TODO 只支持覆盖写入
	return -ENOSYS;
}


static int ntfs_stat(void *fs_super, void *fd, STAT_T *st)
{
	NTFS *ntfs = (NTFS*)fs_super;
	int FD = (u32)fd;
	int retv;

	retv = read_record(ntfs, FD);
	if(retv<0)
		return -ENOENT;

	st->size = ntfs->name->file_size;
	parse_ftime(ntfs->name->create_time, &st->ctime);
	parse_ftime(ntfs->name->modify_time, &st->mtime);
	st->flags  = ntfs->name->flags;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////


#if 1
static void dump_bpb(u8 *buf)
{
	logmsg("\nBPB info:\n");
	buf[11] = 0;
	logmsg("  OEM name: %s\n", buf+3);
	logmsg("  sector size: %d\n", le16(buf+0x0b));
	logmsg("  cluster size: %d\n", *(u8*)(buf+0x0d));
	logmsg("  Media: %02x\n", *(u8*)(buf+0x15));
	logmsg("  sectors per Track: %d\n", *(u16*)(buf+0x18));
	logmsg("  heads per Cylinder: %d\n", *(u16*)(buf+0x1a));
	logmsg("  hidden sectors: %d\n", *(u32*)(buf+0x1c));

	logmsg("NTFS BPB2 info:\n");
	logmsg("  total_sectors: %08x_%08x\n", *(u32*)(buf+0x2c), *(u32*)(buf+0x28));
	logmsg("  MFT  start: %08x_%08x\n", *(u32*)(buf+0x34), *(u32*)(buf+0x30));
	logmsg("  MFT2 start: %08x_%08x\n", *(u32*)(buf+0x3c), *(u32*)(buf+0x38));
	logmsg("  MFT size: %08x\n", *(u32*)(buf+0x40));
	logmsg("  Index size: %08x\n", *(u32*)(buf+0x44));
	logmsg("  Logic serial: %08x_%08x\n", *(u32*)(buf+0x4c), *(u32*)(buf+0x48));
	logmsg("\n");
}
#endif


static void *ntfs_mount(BLKDEV *bdev, int part)
{
	u32 dbr_lba;
	NTFS *ntfs;
	int retv;
	u8 *buf;

	dbr_lba = bdev->parts[part].lba_start;

	buf = FS_MALLOC(1024);
	retv = bdev->read_sector(bdev->itf, buf, dbr_lba, 1);
	if(retv<0 || strncmp((char*)(buf+3), "NTFS", 4)){
		FS_FREE(buf);
		return NULL;
	}

	dump_bpb(buf);

	ntfs = (NTFS*)FS_MALLOC(sizeof(NTFS));
	memset(ntfs, 0, sizeof(NTFS));

	ntfs->bdev = bdev;
	ntfs->part = part;

	ntfs->lba_base = dbr_lba;
	ntfs->cluster_size = *(u8*)(buf+0x0d);
	ntfs->lba_mft  = *(u32*)(buf+0x30);
	ntfs->lba_mft = ntfs->lba_mft*ntfs->cluster_size + dbr_lba;

	ntfs->mbuf = buf;
	ntfs->dbuf = (u8*)FS_MALLOC(ntfs->cluster_size*512);
	ntfs->data_lcn = 0xffffffff;

	// parse $MFT
	bdev->read_sector(bdev->itf, ntfs->mbuf, ntfs->lba_mft, 2);
	ntfs->mbuf_index = 0;
	parse_record(ntfs);

	RUNLIST *rl = (RUNLIST*)ntfs->attr_80;
	int len = rl->end - rl->start + sizeof(RUNLIST);
	ntfs->mft_80 = FS_MALLOC(len);
	memset(ntfs->mft_80, 0, len);
	memcpy(ntfs->mft_80 + sizeof(RUNLIST), ntfs->attr_80 + rl->start, len-sizeof(RUNLIST));

	rl = (RUNLIST*)ntfs->mft_80;
	rl->start = sizeof(RUNLIST);
	rl->end = len;

	return ntfs;
}

static int ntfs_umount(void *fs_super)
{
	NTFS *ntfs = (NTFS*)fs_super;

	FS_FREE(ntfs->dbuf);
	FS_FREE(ntfs->mbuf);
	FS_FREE(ntfs->mft_80);
	FS_FREE(ntfs);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////


FSDESC fs_ntfs = {
	.name  = "NTFS",
	.mount = ntfs_mount,
	.umount= ntfs_umount,
	.find  = ntfs_find,
	.stat  = ntfs_stat,
	.open  = ntfs_open,
	.close = ntfs_close,
	.lseek = ntfs_lseek,
	.read  = ntfs_read,
	.write = ntfs_write, //
};


///////////////////////////////////////////////////////////////////////////////

