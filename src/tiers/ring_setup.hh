#include<atomic>

inline int uring_alloc_huge (unsigned sq_entries,::io_uring_params& p,
                             ::io_uring_sq* sq,::io_uring_cq* cq,
                             void* buf,size_t bufSize)
{
  constexpr int KERN_MAX_ENTRIES=32768;
  constexpr int KERN_MAX_CQ_ENTRIES=2*KERN_MAX_ENTRIES;

  if(!sq_entries)return -EINVAL;
  if(sq_entries>KERN_MAX_ENTRIES){
    if(!(p.flags&IORING_SETUP_CLAMP))
      return -EINVAL;
    sq_entries=KERN_MAX_ENTRIES;
  }

  unsigned cq_entries;
  if(p.flags&IORING_SETUP_CQSIZE){
    if(!p.cq_entries)return -EINVAL;
    cq_entries=p.cq_entries;
    if(cq_entries>KERN_MAX_CQ_ENTRIES){
      if(!(p.flags & IORING_SETUP_CLAMP))
        return -EINVAL;
      cq_entries=KERN_MAX_CQ_ENTRIES;
    }
    if(cq_entries<sq_entries)return -EINVAL;
  }else{
    cq_entries=2*sq_entries;
  }

  size_t sqes_mem=sq_entries*sizeof(io_uring_sqe);
  unsigned long pageSize=getpagesize();
  sqes_mem=(sqes_mem+pageSize-1) & ~(pageSize-1);

  size_t cqes_mem=cq_entries*sizeof(io_uring_cqe);
  if(p.flags&IORING_SETUP_CQE32)cqes_mem*=2;
  constexpr int KRING_SIZE=64;
  size_t const ring_mem=KRING_SIZE+sqes_mem+cqes_mem;
  unsigned long mem_used=ring_mem;
  mem_used=(mem_used+pageSize-1) & ~(pageSize-1);

  /*
   * A maxed-out number of CQ entries with IORING_SETUP_CQE32 fills a 2MB
   * huge page by itself, so the SQ entries won't fit in the same huge
   * page. For SQEs, that shouldn't be possible given KERN_MAX_ENTRIES,
   * but check that too to future-proof (e.g. against different huge page
   * sizes). Bail out early so we don't overrun.
   */
  constexpr size_t huge_pageSize=2*1024*1024;
  if(!buf && (sqes_mem>huge_pageSize||ring_mem>huge_pageSize))
    return -ENOMEM;

  if(mem_used>bufSize)return -ENOMEM;

  sq->sqes=(io_uring_sqe*)buf;
  sq->ring_ptr=(unsigned char*)sq->sqes+sqes_mem;

  cq->ring_ptr=sq->ring_ptr;
  p.sq_off.user_addr = (unsigned long) sq->sqes;
  p.cq_off.user_addr = (unsigned long) sq->ring_ptr;
  return (int) mem_used;
}

inline void uring_setup_ring (::io_uring_params& p,::io_uring& rng)
{
  ::io_uring_sq& sq=rng.sq;
  ::io_uring_cq& cq=rng.cq;
  sq.khead = (unsigned*)((unsigned char*)sq.ring_ptr + p.sq_off.head);
  sq.ktail = (unsigned*)((unsigned char*)sq.ring_ptr + p.sq_off.tail);
  sq.kring_mask = (unsigned*)((unsigned char*)sq.ring_ptr + p.sq_off.ring_mask);
  sq.kring_entries = (unsigned*)((unsigned char*)sq.ring_ptr + p.sq_off.ring_entries);
  sq.kflags = (unsigned*)((unsigned char*)sq.ring_ptr + p.sq_off.flags);
  sq.kdropped = (unsigned*)((unsigned char*)sq.ring_ptr + p.sq_off.dropped);

  cq.khead = (unsigned*)((unsigned char*)cq.ring_ptr + p.cq_off.head);
  cq.ktail = (unsigned*)((unsigned char*)cq.ring_ptr + p.cq_off.tail);
  cq.kring_mask = (unsigned*)((unsigned char*)cq.ring_ptr + p.cq_off.ring_mask);
  cq.kring_entries = (unsigned*)((unsigned char*)cq.ring_ptr + p.cq_off.ring_entries);
  cq.koverflow = (unsigned*)((unsigned char*)cq.ring_ptr + p.cq_off.overflow);
  cq.cqes = (::io_uring_cqe*)((unsigned char*)cq.ring_ptr + p.cq_off.cqes);
  if(p.cq_off.flags)
    cq.kflags = (unsigned*)((unsigned char*)cq.ring_ptr + p.cq_off.flags);

  sq.ring_mask=*sq.kring_mask;
  sq.ring_entries=*sq.kring_entries;
  cq.ring_mask=*cq.kring_mask;
  cq.ring_entries=*cq.kring_entries;
  rng.features=p.features;
  rng.flags=p.flags;
}

template <typename T>
void URING_WRITE_ONCE (T& var,T val){
        std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(&var),
                                   val, ::std::memory_order_relaxed);
}

template <typename T>
T URING_READ_ONCE (T const& var){
	return ::std::atomic_load_explicit(
           reinterpret_cast<::std::atomic<T> const*>(&var),
           ::std::memory_order_relaxed);
}

template <typename T>
void uring_smp_store_release (T* p,T v){
  ::std::atomic_store_explicit(reinterpret_cast<::std::atomic<T> *>(p), v,
                               ::std::memory_order_release);
}

template <typename T>
T uring_smp_load_acquire (const T* p){
  return ::std::atomic_load_explicit(
        reinterpret_cast<::std::atomic<T> const*>(p),
                ::std::memory_order_acquire);
}

inline void uring_smp_mb (){
  ::std::atomic_thread_fence(::std::memory_order_seq_cst);
}

inline ::io_uring_sqe* uring_get_sqe (::io_uring* ring)
{
  ::io_uring_sq* sq=&ring->sq;
  unsigned head=*ring->sq.khead,
	   tail=sq->sqe_tail;

  if(tail-head>=sq->ring_entries)return 0;

  ++sq->sqe_tail;
  auto& sqe=sq->sqes[(tail & sq->ring_mask)<<io_uring_sqe_shift(ring)];
  sqe={};
  return &sqe;
}

inline unsigned uring_cq_ready (::io_uring const* ring)
{
  return uring_smp_load_acquire(ring->cq.ktail) - *ring->cq.khead;
}

inline unsigned uring_peek_batch_cqe (::io_uring* ring,
                                      ::io_uring_cqe** cqes,unsigned count)
{
  int shift=0;
  if(ring->flags&IORING_SETUP_CQE32)
    shift=1;

  unsigned ready=uring_cq_ready(ring);
  if(ready<count)count=ready;

  unsigned head=*ring->cq.khead;
  unsigned mask=ring->cq.ring_mask;
  for(unsigned last=head+count;head!=last;++head)
    *(cqes++)=&ring->cq.cqes[(head & mask)<<shift];
  return count;
}

enum{
  INT_FLAG_REG_RING       = IORING_ENTER_REGISTERED_RING,
  INT_FLAG_NO_IOWAIT      = IORING_ENTER_NO_IOWAIT,
  INT_FLAG_REG_REG_RING   = 1,
  INT_FLAG_APP_MEM        = 2,
  INT_FLAG_CQ_ENTER       = 4,
};

