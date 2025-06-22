
void io_uring_setup_ring_pointers(io_uring_params *p,
                                  io_uring_sq *sq,
                                  io_uring_cq *cq)
{
  sq->khead = (unsigned*)((unsigned char*)sq->ring_ptr + p->sq_off.head);
  sq->ktail = (unsigned*)((unsigned char*)sq->ring_ptr + p->sq_off.tail);
  sq->kring_mask = (unsigned*)((unsigned char*)sq->ring_ptr + p->sq_off.ring_mask);
  sq->kring_entries = (unsigned*)((unsigned char*)sq->ring_ptr + p->sq_off.ring_entries);
  sq->kflags = (unsigned*)((unsigned char*)sq->ring_ptr + p->sq_off.flags);
  sq->kdropped = (unsigned*)((unsigned char*)sq->ring_ptr + p->sq_off.dropped);

  cq->khead = (unsigned*)((unsigned char*)cq->ring_ptr + p->cq_off.head);
  cq->ktail = (unsigned*)((unsigned char*)cq->ring_ptr + p->cq_off.tail);
  cq->kring_mask = (unsigned*)((unsigned char*)cq->ring_ptr + p->cq_off.ring_mask);
  cq->kring_entries = (unsigned*)((unsigned char*)cq->ring_ptr + p->cq_off.ring_entries);
  cq->koverflow = (unsigned*)((unsigned char*)cq->ring_ptr + p->cq_off.overflow);
  cq->cqes = (::io_uring_cqe*)((unsigned char*)cq->ring_ptr + p->cq_off.cqes);
  if (p->cq_off.flags)
    cq->kflags = (unsigned*)((unsigned char*)cq->ring_ptr + p->cq_off.flags);

  sq->ring_mask = *sq->kring_mask;
  sq->ring_entries = *sq->kring_entries;
  cq->ring_mask = *cq->kring_mask;
  cq->ring_entries = *cq->kring_entries;
}

int io_uring_alloc_huge (unsigned entries, io_uring_params *p,
                         io_uring_sq *sq, io_uring_cq *cq,
                         void *buf, size_t buf_size)
{
  constexpr int KERN_MAX_ENTRIES=32768;
  constexpr int KERN_MAX_CQ_ENTRIES=2*KERN_MAX_ENTRIES;

  unsigned long page_size=getpagesize();

  if (!entries)return -EINVAL;
  if (entries>KERN_MAX_ENTRIES){
    if (!(p->flags&IORING_SETUP_CLAMP))
      return -EINVAL;
    entries = KERN_MAX_ENTRIES;
  }

  unsigned cq_entries;
  if(p->flags&IORING_SETUP_CQSIZE){
    if(!p->cq_entries)return -EINVAL;
    cq_entries=p->cq_entries;
    if(cq_entries>KERN_MAX_CQ_ENTRIES){
      if(!(p->flags & IORING_SETUP_CLAMP))
        return -EINVAL;
      cq_entries=KERN_MAX_CQ_ENTRIES;
    }
    if(cq_entries<entries)return -EINVAL;
  }else{
    cq_entries=2*entries;
  }

  unsigned sq_entries=entries;

  size_t sqes_mem=sq_entries*sizeof(io_uring_sqe);
  sqes_mem = (sqes_mem+page_size-1) & ~(page_size-1);

  size_t cqes_mem=cq_entries*sizeof(io_uring_cqe);
  if(p->flags&IORING_SETUP_CQE32) cqes_mem*=2;
  constexpr int KRING_SIZE=64;
  size_t const ring_mem=KRING_SIZE+sqes_mem+cqes_mem;
  unsigned long mem_used=ring_mem;
  mem_used = (mem_used+page_size-1) & ~(page_size-1);

  /*
   * A maxed-out number of CQ entries with IORING_SETUP_CQE32 fills a 2MB
   * huge page by itself, so the SQ entries won't fit in the same huge
   * page. For SQEs, that shouldn't be possible given KERN_MAX_ENTRIES,
   * but check that too to future-proof (e.g. against different huge page
   * sizes). Bail out early so we don't overrun.
   */
  constexpr size_t huge_page_size = 2*1024*1024;
  if(!buf && (sqes_mem>huge_page_size||ring_mem>huge_page_size))
    return -ENOMEM;

  if(mem_used>buf_size)return -ENOMEM;

  sq->sqes=(io_uring_sqe*)buf;
  sq->ring_ptr=(unsigned char*)sq->sqes+sqes_mem ;

  cq->ring_ptr=(void *)sq->ring_ptr;
  p->sq_off.user_addr = (unsigned long) sq->sqes;
  p->cq_off.user_addr = (unsigned long) sq->ring_ptr;
  return (int) mem_used;
}

enum {
        INT_FLAG_REG_RING       = IORING_ENTER_REGISTERED_RING,
        INT_FLAG_NO_IOWAIT      = IORING_ENTER_NO_IOWAIT,
        INT_FLAG_REG_REG_RING   = 1,
        INT_FLAG_APP_MEM        = 2,
        INT_FLAG_CQ_ENTER       = 4,
};

