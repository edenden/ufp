#ifndef _LIBUFP_RTX_H
#define _LIBUFP_RTX_H

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define prefetch(x)	__builtin_prefetch(x, 0)
#define prefetchw(x)	__builtin_prefetch(x, 1)

#if defined(__x86_64__) || defined(__i386__) || defined(__amd64__)
#define mb()  asm volatile("mfence" ::: "memory")
#define wmb() asm volatile("sfence" ::: "memory")
#define rmb() asm volatile("lfence" ::: "memory")
#else
#define mb()  asm volatile("" ::: "memory")
#define rmb() asm volatile("" ::: "memory")
#define wmb() asm volatile("" ::: "memory")
#endif

#endif /* _LIBUFP_RTX_H */
