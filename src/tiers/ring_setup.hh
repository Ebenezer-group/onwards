
#include<atomic>
#include<cmwBuffer.hh>

inline auto mmapWrapper (size_t length){
  if(auto addr=::mmap(0,length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);addr!=MAP_FAILED)return addr;
  ::cmw::raise("mmap",errno);
}

template <typename T>
void URING_WRITE_ONCE (T &var,T val){
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
void uring_smp_store_release (T *p,T v){
  ::std::atomic_store_explicit(reinterpret_cast<::std::atomic<T> *>(p), v,
                               ::std::memory_order_release);
}

template <typename T>
T uring_smp_load_acquire (const T *p){
  return ::std::atomic_load_explicit(
        reinterpret_cast<::std::atomic<T> const*>(p),
                ::std::memory_order_acquire);
}

inline void uring_smp_mb (){
  ::std::atomic_thread_fence(::std::memory_order_seq_cst);
}

void uring_initialize_sqe (::io_uring_sqe* sqe)
{
  sqe->flags=0;
  sqe->ioprio=0;
  sqe->rw_flags=0;
  sqe->buf_index=0;
  sqe->personality=0;
  sqe->file_index=0;
  sqe->addr3=0;
  sqe->__pad2[0]=0;
}

::io_uring_sqe* uring_get_sqe (::io_uring* ring)
{
  ::io_uring_sq* sq=&ring->sq;
  unsigned head=*ring->sq.khead, 
	   tail=sq->sqe_tail;

  if(tail-head>=sq->ring_entries)return 0;

  auto sqe=&sq->sqes[(tail & sq->ring_mask)<<io_uring_sqe_shift(ring)];
  sq->sqe_tail=tail+1;
  ::uring_initialize_sqe(sqe);
  return sqe;
}

unsigned uring_cq_ready (::io_uring const* ring)
{
  return uring_smp_load_acquire(ring->cq.ktail) - *ring->cq.khead;
}

unsigned uring_peek_batch_cqe (::io_uring* ring,
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

void uring_setup_ring_pointers (::io_uring_params* p,
                                ::io_uring_sq* sq,::io_uring_cq* cq)
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

  sq->ring_mask=*sq->kring_mask;
  sq->ring_entries=*sq->kring_entries;
  cq->ring_mask=*cq->kring_mask;
  cq->ring_entries=*cq->kring_entries;
}

int uring_alloc_huge (unsigned entries, ::io_uring_params* p,
                      ::io_uring_sq* sq, ::io_uring_cq* cq,
                      void* buf, size_t buf_size)
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

