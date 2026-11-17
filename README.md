# getrandom-shim

`LD_PRELOAD` shim for Linux kernels **older than 3.17**, which don't have the
`getrandom(2)` syscall. Synology DSM is the main example: it keeps the kernel the
NAS originally shipped with.

Recent `git` builds, for example in the `gitea/gitea` and `fnsys/dockhand`
images, fail on such kernels with:

```
error: unable to get random bytes for temporary file: Function not implemented
fatal: Unable to create temporary file '.../tXXXXXX': Function not implemented
```

The shim overrides `getrandom()` and `getentropy()`. It tries the real syscall
first and falls back to `/dev/urandom` only when the kernel returns `ENOSYS`, so
on a modern kernel it changes nothing. It is built with `-nostdlib`, has no libc
dependency, and works in both musl (Alpine) and glibc (Debian, Wolfi) images.

Based on Jan Seeger's posts
[Modern Git on old Synology kernels](https://jan.alphadev.net/blog/2026/git-on-synology/)
and [part 2](https://jan.alphadev.net/blog/2026/git-on-synology-2/). This version
fixes the aarch64 build (`openat` instead of `open`), handles short reads and
`EINTR`, sets `errno`, and adds `getentropy()`.

## Usage

The image `ghcr.io/frankhommers/getrandom-shim` (amd64 and arm64) copies
`getrandom_shim.so` into a volume and exits. It is `FROM scratch` and contains
only the shim and a tiny static installer, also libc-free: about 5 KB on amd64
and 87 KB on arm64, most of which is 64K page alignment.

Docker Engine 28+ could mount the image directly (`type: image`) and skip the
init container, but Synology's Container Manager ships Docker 24, so this uses
the copy-to-volume approach. Mount that volume into the
affected service and preload the shim:

```yaml
services:
  getrandom-shim:
    image: ghcr.io/frankhommers/getrandom-shim:latest
    volumes:
      - getrandom-shim:/shim

  gitea:
    image: gitea/gitea:latest
    environment:
      - LD_PRELOAD=/shim/getrandom_shim.so
    volumes:
      - getrandom-shim:/shim:ro
    depends_on:
      getrandom-shim:
        condition: service_completed_successfully

volumes:
  getrandom-shim:
```

If the image already sets `LD_PRELOAD`, add the shim to that value rather than
replacing it, separated by `:`, e.g.
`LD_PRELOAD=/shim/getrandom_shim.so:/usr/lib/libjemalloc.so.2`.

Statically linked binaries ignore `LD_PRELOAD`. Gitea's Go binary is one of
them, but that's fine: the Go runtime falls back to `/dev/urandom` by itself.
The shim is for the dynamically linked `git` processes it starts.

## Testing

`test/run.sh` simulates an old kernel with a seccomp profile that makes
`getrandom` return `ENOSYS`. It checks that git fails without the shim and
works with it:

```sh
docker build -t getrandom-shim:test .
test/run.sh getrandom-shim:test                     # gitea + dockhand
test/run.sh getrandom-shim:test some/image:tag      # any image with sh + git
```

CI runs this on amd64 and arm64 on every push and weekly against the latest
images, then publishes the multi-arch image.
