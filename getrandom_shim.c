/*
 * getrandom_shim - LD_PRELOAD shim for kernels without the getrandom(2)
 * syscall (Linux < 3.17, e.g. Synology DSM).
 *
 * Overrides getrandom() and getentropy(). The real syscall is tried first;
 * only when the kernel answers ENOSYS does the shim fall back to reading
 * /dev/urandom. On modern kernels it is a transparent pass-through.
 *
 * Built with -nostdlib so it has no libc dependency and works in both musl
 * and glibc based images. errno is set through __errno_location(), which
 * both musl and glibc export and which is resolved at runtime from the libc
 * already loaded by the host process.
 */

#include <stddef.h>
#include <sys/syscall.h>

#define E_INTR   4
#define E_IO     5
#define E_NOSYS  38

#define AT_FDCWD_ (-100)
#define O_RDONLY_ 0
#define O_CLOEXEC_ 02000000 /* same value on x86_64 and aarch64 */

extern int *__errno_location(void);

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

/* 0 = unknown, 1 = kernel has getrandom, -1 = kernel lacks it */
static int have_getrandom;

static long urandom_read(unsigned char *buf, size_t len)
{
    long fd;
    size_t done = 0;

    do {
        fd = sys4(SYS_openat, AT_FDCWD_, (long)"/dev/urandom",
                  O_RDONLY_ | O_CLOEXEC_, 0);
    } while (fd == -E_INTR);
    if (fd < 0)
        return fd;

    while (done < len) {
        long r = sys4(SYS_read, fd, (long)(buf + done), (long)(len - done), 0);
        if (r == -E_INTR)
            continue;
        if (r <= 0) {
            sys4(SYS_close, fd, 0, 0, 0);
            return r < 0 ? r : -E_IO;
        }
        done += (size_t)r;
    }
    sys4(SYS_close, fd, 0, 0, 0);
    return (long)done;
}

static long fill(void *buf, size_t len, unsigned int flags)
{
    if (have_getrandom >= 0) {
        long r = sys4(SYS_getrandom, (long)buf, (long)len, (long)flags, 0);
        if (r != -E_NOSYS) {
            have_getrandom = 1;
            return r;
        }
        have_getrandom = -1;
    }
    /* /dev/urandom never blocks, so GRND_NONBLOCK needs no handling. */
    return urandom_read((unsigned char *)buf, len);
}

__attribute__((visibility("default")))
long getrandom(void *buf, size_t len, unsigned int flags)
{
    long r = fill(buf, len, flags);
    if (r < 0) {
        *__errno_location() = (int)-r;
        return -1;
    }
    return r;
}

__attribute__((visibility("default")))
int getentropy(void *buf, size_t len)
{
    unsigned char *p = (unsigned char *)buf;

    if (len > 256) {
        *__errno_location() = E_IO;
        return -1;
    }
    while (len) {
        long r = fill(p, len, 0);
        if (r == -E_INTR)
            continue;
        if (r < 0) {
            *__errno_location() = (int)-r;
            return -1;
        }
        p += r;
        len -= (size_t)r;
    }
    return 0;
}
