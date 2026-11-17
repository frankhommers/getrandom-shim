/* Raw Linux syscalls without libc (x86_64 and aarch64). */
#ifndef SYS_H
#define SYS_H

#include <stddef.h>
#include <sys/syscall.h>

#define E_INTR   4
#define E_IO     5
#define E_NOSYS  38

#define AT_FDCWD_ (-100)
#define O_RDONLY_ 0
#define O_CLOEXEC_ 02000000 /* same value on x86_64 and aarch64 */
#define O_WRONLY_ 01
#define O_CREAT_ 0100
#define O_TRUNC_ 01000

#if defined(__x86_64__)
static long sys4(long n, long a, long b, long c, long d)
{
    long ret;
    register long r10 __asm__("r10") = d;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10)
                     : "rcx", "r11", "memory");
    return ret;
}
#elif defined(__aarch64__)
static long sys4(long n, long a, long b, long c, long d)
{
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc 0"
                     : "+r"(x0)
                     : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
                     : "memory");
    return x0;
}
#else
#error "unsupported architecture (x86_64 and aarch64 only)"
#endif

#endif
