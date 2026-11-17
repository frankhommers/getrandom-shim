#!/usr/bin/env bash
# Simulates a kernel without getrandom(2) (seccomp returns ENOSYS) and checks,
# for real-world images, that git fails without the shim and works with it.
# Usage: test/run.sh <shim-image> [image ...]
set -euo pipefail
SHIM=${1:?usage: test/run.sh <shim-image> [image ...]}; shift
IMAGES=("$@")
[ $# -eq 0 ] && IMAGES=(gitea/gitea:latest fnsys/dockhand:latest)
SECCOMP="seccomp=$(cd "$(dirname "$0")" && pwd)/seccomp-no-getrandom.json"
VOL=getrandom-shim-test-$$
trap 'docker volume rm -f "$VOL" >/dev/null' EXIT

docker run --rm -v "$VOL":/shim "$SHIM"

GIT_TEST='cd /tmp && git init -q r && cd r && echo hi > f && git add f && git -c user.name=t -c user.email=t@t commit -qm t'
fail=0
for img in "${IMAGES[@]}"; do
  echo "== $img"
  if docker run --rm --security-opt "$SECCOMP" --entrypoint sh "$img" -c "$GIT_TEST" >/dev/null 2>&1; then
    echo "   without shim: works (this git does not need getrandom)"
  else
    echo "   without shim: fails (reproduces the old-kernel bug)"
  fi
  if docker run --rm --security-opt "$SECCOMP" -e LD_PRELOAD=/shim/getrandom_shim.so \
       -v "$VOL":/shim:ro --entrypoint sh "$img" -c "$GIT_TEST"; then
    echo "   with shim:    OK"
  else
    echo "   with shim:    FAILED"; fail=1
  fi
done
exit $fail
