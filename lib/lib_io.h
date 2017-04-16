#ifndef _LIBUFP_IO_H
#define _LIBUFP_IO_H

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define prefetch(x)	__builtin_prefetch(x, 0)
#define prefetchw(x)	__builtin_prefetch(x, 1)

#define barrier()	asm volatile("" ::: "memory")

#if defined(__x86_64__) || defined(__i386__) || defined(__amd64__)
#define mb()		asm volatile("mfence" ::: "memory")
#define rmb()		asm volatile("lfence" ::: "memory")
#define wmb()		asm volatile("sfence" ::: "memory")
#define dma_rmb()	barrier()
#define dma_wmb()	barrier()
#else
#define mb()		barrier()
#define rmb()		mb()
#define wmb()		mb()
#define dma_rmb()	rmb()
#define dma_wmb()	wmb()
#endif

#endif /* _LIBUFP_IO_H */
