#!/bin/sh
# WSL1 needs the loader explicitly for this Ubuntu Node executable.
exec /lib64/ld-linux-x86-64.so.2 /usr/bin/node "$@"
