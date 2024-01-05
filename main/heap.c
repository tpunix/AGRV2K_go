
#include "xos.h"

/******************************************************************************/

typedef struct _mblk_t {
	struct _mblk_t *next;
	struct _mblk_t *prev;
	struct _mblk_t *nf;
	struct _mblk_t *pf;
}MBLK;


static u32 heap_start;
static u32 heap_size;

static MBLK *head;

static int heap_lock;

/******************************************************************************/

void xos_heap_init(u32 start, u32 size)
{
	MBLK *mb, *tail;

	printk("XOS heap: start=%08x size=%08x\n", start, size);

	heap_lock = 0;
	heap_start = start;
	heap_size  = size;

	head = (MBLK*)start;
	mb   = (MBLK*)(start + sizeof(MBLK));
	tail = (MBLK*)(start + size - sizeof(MBLK));

	head->next = mb;
	head->prev = NULL;
	head->nf = mb;
	head->pf = NULL;

	mb->next = tail;
	mb->prev = head;
	mb->nf = NULL;
	mb->pf = head;

	tail->next = NULL;
	tail->prev = mb;
}


static int mblk_size(MBLK *m)
{
	u32 next = (u32)m->next;

	int size = (next&~1) - (u32)m;
	return size-8;
}


void xos_dump(void)
{
	MBLK *m = head;

	printk("\n");
	while(m){
		u32 next = (u32)m->next;
		printk("%08x:  nb_%08x pb_%08x", m, next, m->prev);
		if((next&1)==0){
			printk("  nf_%08x pf_%08x sz_%08x\n", m->nf, m->pf, mblk_size(m));
		}else{
			printk("\n");
		}

		m = (MBLK*)(next&~1);
	}
	printk("\n");
}


void *xos_malloc(int size)
{
	MBLK *m = head->nf;
	MBLK *fit_mblk = NULL;
	int fit_size = 0x7fffffff;
	int bsize;

	//printk("xos_malloc: %d bytes\n", size);
	size = (size+7)&~7;

	int key = spin_lock_irq();

	while(m){
		bsize = mblk_size(m);
		if(bsize > size){
			// 找最合适的块
			if(bsize < fit_size){
				fit_size = bsize;
				fit_mblk = m;
			}
#if 0
			// 找第一个块
			break;
#endif
		}

		m = m->nf;
	}
	if(fit_mblk==NULL){
		spin_unlock_irq(key);
		//printk("\n");
		return NULL;
	}
	m = fit_mblk;
	bsize = fit_size;

	if(bsize-size>=16){
		// m可分裂为两块
		MBLK *n = (MBLK*)((u8*)m+8+size);

		n->next = m->next;
		n->prev = m;
		if(n->next)
			n->next->prev = n;
		m->next = n;

		// n代替m，加入free列表中
		n->nf = m->nf;
		n->pf = m->pf;
		if(n->pf)
			n->pf->nf = n;
		if(n->nf)
			n->nf->pf = n;
	}else{
		// m剩余空间不够分裂。将m从free列表移除
		if(m->pf)
			m->pf->nf = m->nf;
		if(m->nf)
			m->nf->pf = m->pf;
	}

	// next最低为标记为使用中
	m->next = (MBLK*)((u32)m->next | 1);

	spin_unlock_irq(key);

	//printk(" %08x\n", (u32)&(m->nf));
	return (void*)&(m->nf);
}


static int is_free(MBLK *p)
{
	u32 next = (u32)p->next;
	return (next && (next&1)==0)? 1: 0;
}


void xos_free(void *ptr)
{
	int m_free = 0;
	MBLK *m = (MBLK*)((u8*)ptr-8);

	int key = spin_lock_irq();

	m->next = (MBLK*)((u32)m->next & ~1);

	MBLK *n = m->next;
	if(n && is_free(n)){
		// 与下一块合并(n并入m)
		m->next = n->next;
		if(n->next)
			n->next->prev = m;

		m->nf = n->nf;
		m->pf = n->pf;
		if(m->pf)
			m->pf->nf = m;
		if(m->nf)
			m->nf->pf = m;

		m_free = 1;
	}

	MBLK *p = m->prev;
	if(is_free(p)){
		if(p==head){
			// m是第一块
			m->nf = p->nf;
			m->pf = p;
			p->nf = m;
			if(m->nf)
				m->nf->pf = m;
		}else{
			// 与上一块合并(m并入p)
			p->next = m->next;
			if(p->next)
				p->next->prev = p;
			if(m_free){
				if(m->nf == p){
					p->pf = m->pf;
					if(p->pf)
						p->pf->nf = p;
				}else{
					p->nf = m->nf;
					if(p->nf)
						p->nf->pf = p;
				}
			}
		}
	}else if(m_free==0){
		// 上一块使用中，直接替换head
		m->nf = head->nf;
		m->pf = head;
		m->nf->pf = m;
		head->nf = m;
	}

	spin_unlock_irq(key);

}


/******************************************************************************/


