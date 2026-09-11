#!/usr/bin/env bash
# GDB thread-spawn trace: logs every ExCreateThread (guest start address)
# and NtResumeThread (handle) with short backtraces. Proves whether a
# verdict-spawned worker (e.g. handle 0xF80000FC, start 82CA3430) is
# created, resumed, and where the resumer calls from.
# Usage: same wrapper as gdb-watch-stall.gdb, output to logs/gdb-spawn.log.
set pagination off
set confirm off
set debuginfod enabled off
handle SIGSEGV nostop noprint pass
break main
run --game_data_root=assets-extracted/00007000 --update_data_root=runtime/update-empty
printf "tracing creates + resumes\n"
break rex::kernel::xboxkrnl::ExCreateThread_entry
commands
silent
printf "CREATE start=%08X\n", $r8 & 0xFFFFFFFF
bt 4
continue
end
break rex::kernel::xboxkrnl::NtResumeThread_entry
commands
silent
printf "RESUME handle=%08X\n", $rdi & 0xFFFFFFFF
bt 6
continue
end
continue
