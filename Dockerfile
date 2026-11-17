FROM alpine:3.22 AS build
RUN apk add --no-cache gcc musl-dev
WORKDIR /src
COPY getrandom_shim.c .
RUN gcc -shared -fPIC -nostdlib -fno-stack-protector -O2 -Wall -Wextra -Werror \
        -o getrandom_shim.so getrandom_shim.c \
 && chmod 644 getrandom_shim.so \
 && ! readelf -d getrandom_shim.so | grep -q NEEDED

FROM busybox:stable-musl
COPY --from=build /src/getrandom_shim.so /getrandom_shim.so
VOLUME /shim
# Copies the shim into the shared volume and exits.
CMD ["sh", "-c", "cp /getrandom_shim.so /shim/getrandom_shim.so && chmod 644 /shim/getrandom_shim.so && echo 'getrandom_shim.so installed in /shim'"]
