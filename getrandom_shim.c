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

#include "sys.h"

extern int *__errno_location(void);


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
