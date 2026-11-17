FROM alpine:3.22 AS build
RUN apk add --no-cache gcc musl-dev binutils
WORKDIR /src
COPY sys.h getrandom_shim.c install.c ./
ARG CFLAGS="-nostdlib -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -Os -Wall -Wextra -Werror"
ARG LDSHRINK="-s -Wl,--build-id=none -Wl,--gc-sections -Wl,--hash-style=gnu"
RUN mkdir -p /out/shim \
 && gcc -shared -fPIC $CFLAGS $LDSHRINK -o /out/getrandom_shim.so getrandom_shim.c \
 && gcc -static -no-pie -fno-pie $CFLAGS $LDSHRINK -o /out/install install.c \
 && strip -R .comment /out/getrandom_shim.so /out/install \
 && chmod 644 /out/getrandom_shim.so \
 && ! readelf -d /out/getrandom_shim.so | grep -q NEEDED \
 && ls -l /out

FROM scratch
COPY --from=build /out/ /
VOLUME /shim
# Copies /getrandom_shim.so into the volume mounted on /shim and exits.
ENTRYPOINT ["/install"]
