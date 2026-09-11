#!/usr/bin/env bash
# GDB stall-bytes watch: flag CS [0x42205148], gate [0x4011F250+44],
# drainctx+5. Guest heap is deterministic across boots (drainctx 0x701BFE20);
# if DRAINCTX reports a different ctx, update the watch addresses below.
# Usage: LD_LIBRARY_PATH="$PWD/rexglue-sdk/out/linux-amd64:$LD_LIBRARY_PATH" \
#   SDL_VIDEODRIVER=offscreen timeout --signal=KILL 110 \
#   nix develop path:. -c gdb -q -batch -x scripts/gdb-watch-stall.gdb \
#   ./build/native/fable_ii > logs/gdb-stall.log 2>&1
set pagination off
set confirm off
set debuginfod enabled off
handle SIGSEGV nostop noprint pass
break main
run --game_data_root=assets-extracted/00007000 --update_data_root=runtime/update-empty
printf "watching flag + gate + spin bytes\n"
watch *(char*)0x142205148
commands
bt 10
continue
end
watch *(char*)0x14011F27C
commands
bt 10
continue
end
watch *(char*)0x1701BFE25
commands
bt 8
continue
end
continue
