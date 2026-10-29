# Weenix (USC CSCI 402)

This is a local copy of the **Weenix** teaching OS used in USC **CSCI 402 / CS 402** (assignment drop `weenix-assignment-3.9.0`). Weenix is a small Unix-like kernel students implement in stages: processes and scheduling, VFS, then virtual memory (with S5FS provided as a library).

**This tree:** `/home/joseph/weenix-claude` on WSL Ubuntu (Windows: `\\wsl$\Ubuntu\home\joseph\weenix-claude`). It is the newest real source tree and the one published here.

**Team (from the assignment READMEs):** Yijun Li (`jli50217@usc.edu`), Yinghao Xu (`yinghaox@usc.edu`).

Kernel 1 (procs), Kernel 2 (VFS), and Kernel 3 (VM) student files are filled in. Compared to the Oct 2025 starter, the assignment-allowed `.c` files plus `Config.mk` contain the real work. S5FS and driver `.c` files still have `NOT_YET_IMPLEMENTED` stubs; those subsystems come from `kernel/libs5fs.a` and `kernel/libdrivers.a`.

## How to build and run

From `weenix-assignment-3.9.0/weenix` (see the stock `README` there):

```bash
# Ubuntu/Debian deps (from the assignment README)
sudo apt-get install git-core build-essential gcc gdb qemu genisoimage make python python-argparse cscope xterm bash grub xorriso

make
./weenix -n          # run
./weenix -n -d gdb   # run under gdb
```

`Config.mk` in this copy has **DRIVERS=1, VFS=1, S5FS=1, VM=1**, `CS402TESTS=10`, and `DBG=error,test`. After changing those flags, `make clean && make`.

Kernel 3 is set to exec `/sbin/init` from the init process. Kshell helpers in `kmain.c` still expose Kernel 1/2 tests (`faber`, `sunghan`, `sunghan_dl`, `vfstest`) plus Kernel 3 userland cases.

## Files worked on

Paths are relative to `weenix-assignment-3.9.0/weenix`. These are the student files that differ from the starter.

### Kernel 1 — processes, threads, sync

| File | What changed |
| --- | --- |
| `kernel/main/kmain.c` | Idle/init bootstrap, VFS `/dev` setup, kshell test commands, later `/sbin/init` |
| `kernel/proc/proc.c` | Process create/cleanup, wait/exit, kill |
| `kernel/proc/kthread.c` | Thread create/cancel/exit; Kernel 3 `kthread_clone` |
| `kernel/proc/sched.c` | Run queue, context switch, cancellable sleep |
| `kernel/proc/sched_helper.c` | Sleep / wakeup / broadcast on wait queues |
| `kernel/proc/kmutex.c` | Blocking mutexes |

### Kernel 2 — VFS

| File | What changed |
| --- | --- |
| `kernel/fs/namev.c` | Path lookup (`lookup`, `dir_namev`, `open_namev`) |
| `kernel/fs/open.c` | `do_open` |
| `kernel/fs/vfs_syscall.c` | File, directory, and cwd syscalls |
| `kernel/fs/vn_special.c` | Device-file read/write; later VM page ops for special files |

### Kernel 3 — virtual memory

| File | What changed |
| --- | --- |
| `kernel/mm/pframe.c` | `pframe_get` / pin / unpin and related page-frame helpers |
| `kernel/vm/vmmap.c` | Address-space map: insert, lookup, map, clone, destroy, I/O |
| `kernel/vm/anon.c` | Anonymous memory objects |
| `kernel/vm/shadow.c` | Shadow objects for copy-on-write |
| `kernel/vm/pagefault.c` | User page-fault handler |
| `kernel/vm/mmap.c` | `mmap` / `munmap` |
| `kernel/vm/brk.c` | Heap break |
| `kernel/proc/fork.c` | `fork` with COW shadows |
| `kernel/api/access.c` | User-pointer permission checks |
| `kernel/api/syscall.c` | `sys_read` / `sys_write` / `sys_getdents` copyin/copyout |
| `Config.mk` | Enabled VFS, S5FS, VM, drivers |

`kernel/fs/file.c` and `kernel/fs/vnode.c` match the starter (not student work). Extra-credit stubs that are **still unimplemented**: MTP (`kthread_detach` / `kthread_join` / reapd), GETCWD (`lookup_name` / `lookup_dirpath`), and mounting (`do_mount` / `do_umount`).

## Functions worked on

Only functions that differ from the starter and had `NOT_YET_IMPLEMENTED` removed (or are the documented Kernel 1/2/3 entry points in those same files). Names are from the code.

### Processes and scheduling

| Function | File | What it does |
| --- | --- | --- |
| `bootstrap` | `kernel/main/kmain.c` | Creates the idle process/thread and switches into it |
| `idleproc_run` | `kernel/main/kmain.c` | Sets `p_cwd`, creates `/dev/{null,zero,tty0}`, starts init, waits, then shuts down |
| `initproc_create` | `kernel/main/kmain.c` | Creates PID 1 (`init`) and its first thread |
| `initproc_run` | `kernel/main/kmain.c` | Kshell / tests, then execs `/sbin/init` |
| `proc_create` | `kernel/proc/proc.c` | Allocates a `proc_t`, PID, page tables, file table, child/thread lists |
| `proc_cleanup` | `kernel/proc/proc.c` | Reparents children, wakes waiters, tears down VM and files |
| `do_waitpid` | `kernel/proc/proc.c` | Waits for a child (or any child) and reaps it |
| `do_exit` | `kernel/proc/proc.c` | Current process exit path |
| `proc_kill` | `kernel/proc/proc.c` | Cancels threads in a process |
| `proc_kill_all` | `kernel/proc/proc.c` | Kills every process except current |
| `proc_thread_exited` | `kernel/proc/proc.c` | Last-thread-exit hook into process cleanup |
| `kthread_create` | `kernel/proc/kthread.c` | New kernel thread + context on a process |
| `kthread_cancel` | `kernel/proc/kthread.c` | Request cancellation / wake if cancellable |
| `kthread_exit` | `kernel/proc/kthread.c` | Thread teardown and switch away |
| `kthread_clone` | `kernel/proc/kthread.c` | Clone thread state for `fork` |
| `sched_make_runnable` | `kernel/proc/sched.c` | Puts a thread on the run queue |
| `sched_switch` | `kernel/proc/sched.c` | Picks the next thread and context-switches |
| `sched_cancel` | `kernel/proc/sched.c` | Cancel a sleeping thread |
| `sched_cancellable_sleep_on` | `kernel/proc/sched.c` | Interruptible sleep on a wait queue |
| `sched_wakeup_on` | `kernel/proc/sched_helper.c` | Wake one waiter |
| `sched_sleep_on` | `kernel/proc/sched_helper.c` | Uninterruptible sleep |
| `sched_broadcast_on` | `kernel/proc/sched_helper.c` | Wake all waiters |
| `kmutex_init` | `kernel/proc/kmutex.c` | Initialize a mutex |
| `kmutex_lock` | `kernel/proc/kmutex.c` | Blocking lock |
| `kmutex_lock_cancellable` | `kernel/proc/kmutex.c` | Lock that can be cancelled |
| `kmutex_unlock` | `kernel/proc/kmutex.c` | Unlock and wake a waiter |

### VFS

| Function | File | What it does |
| --- | --- | --- |
| `lookup` | `kernel/fs/namev.c` | Looks up a name in a directory vnode |
| `dir_namev` | `kernel/fs/namev.c` | Walks a path to the parent directory + basename |
| `open_namev` | `kernel/fs/namev.c` | Resolves a path to a vnode; honors `O_CREAT` |
| `do_open` | `kernel/fs/open.c` | Allocates an fd and `file_t`, calls `open_namev` |
| `do_read` | `kernel/fs/vfs_syscall.c` | Reads from an fd and advances `f_pos` |
| `do_write` | `kernel/fs/vfs_syscall.c` | Writes (including `O_APPEND`) |
| `do_close` | `kernel/fs/vfs_syscall.c` | `fput` and clear `p_files[fd]` |
| `do_dup` / `do_dup2` | `kernel/fs/vfs_syscall.c` | Duplicate an fd onto a new / specified slot |
| `do_lseek` | `kernel/fs/vfs_syscall.c` | Seek by `SEEK_SET` / `CUR` / `END` |
| `do_stat` | `kernel/fs/vfs_syscall.c` | Stat a path |
| `do_getdent` | `kernel/fs/vfs_syscall.c` | Read one directory entry |
| `do_mkdir` / `do_rmdir` | `kernel/fs/vfs_syscall.c` | Create / remove a directory |
| `do_unlink` / `do_link` / `do_rename` | `kernel/fs/vfs_syscall.c` | Directory entry ops (`rename` is link + unlink) |
| `do_mknod` | `kernel/fs/vfs_syscall.c` | Create a device node |
| `do_chdir` | `kernel/fs/vfs_syscall.c` | Change `curproc->p_cwd` |
| `special_file_read` | `kernel/fs/vn_special.c` | Device read via `vn_cdev` |
| `special_file_write` | `kernel/fs/vn_special.c` | Device write via `vn_cdev` |
| `special_file_mmap` | `kernel/fs/vn_special.c` | mmap path for special files |
| `special_file_fillpage` | `kernel/fs/vn_special.c` | Fill a mapped special-file page |
| `special_file_dirtypage` | `kernel/fs/vn_special.c` | Mark that page dirty |
| `special_file_cleanpage` | `kernel/fs/vn_special.c` | Write it back |

### Virtual memory

| Function | File | What it does |
| --- | --- | --- |
| `pframe_get` | `kernel/mm/pframe.c` | Get/create a page frame for an `mmobj` pagenum |
| `pframe_pin` / `pframe_unpin` | `kernel/mm/pframe.c` | Pin a frame so the pageout daemon will not free it |
| `vmmap_create` | `kernel/vm/vmmap.c` | New address-space map |
| `vmmap_destroy` | `kernel/vm/vmmap.c` | Tear down all `vmarea`s and objects |
| `vmmap_insert` | `kernel/vm/vmmap.c` | Insert a `vmarea` in order |
| `vmmap_lookup` | `kernel/vm/vmmap.c` | Find the `vmarea` for a virtual page |
| `vmmap_map` | `kernel/vm/vmmap.c` | Map an `mmobj` (or anonymous) into a range |
| `vmmap_find_range` | `kernel/vm/vmmap.c` | Find a hole for mmap |
| `vmmap_is_range_empty` | `kernel/vm/vmmap.c` | Check that a VPN range is free |
| `vmmap_remove` | `kernel/vm/vmmap.c` | Unmap a range |
| `vmmap_clone` | `kernel/vm/vmmap.c` | Clone the map for `fork` (shadows come from `do_fork`) |
| `vmmap_read` / `vmmap_write` | `kernel/vm/vmmap.c` | Copy data through the map |
| `anon_init` / `anon_create` | `kernel/vm/anon.c` | Anonymous object setup |
| `anon_ref` / `anon_put` | `kernel/vm/anon.c` | Refcount; destroy when it hits zero |
| `anon_lookuppage` / `anon_fillpage` | `kernel/vm/anon.c` | Lookup / zero-fill anonymous pages |
| `anon_dirtypage` / `anon_cleanpage` | `kernel/vm/anon.c` | Dirty/clean hooks |
| `shadow_init` / `shadow_create` | `kernel/vm/shadow.c` | COW shadow object setup |
| `shadow_ref` / `shadow_put` | `kernel/vm/shadow.c` | Refcount and collapse when unused |
| `shadow_lookuppage` | `kernel/vm/shadow.c` | Walk shadow → parent → vnode |
| `shadow_fillpage` | `kernel/vm/shadow.c` | Copy a page from the parent (COW) |
| `shadow_dirtypage` / `shadow_cleanpage` | `kernel/vm/shadow.c` | COW write / clean |
| `handle_pagefault` | `kernel/vm/pagefault.c` | Resolve a user fault via the `vmarea` + `mmobj` |
| `do_mmap` / `do_munmap` | `kernel/vm/mmap.c` | mmap/munmap syscalls |
| `do_brk` | `kernel/vm/brk.c` | Grow/shrink the heap mapping |
| `do_fork` | `kernel/proc/fork.c` | New process, cloned map with shadow objects, cloned thread |
| `addr_perm` / `range_perm` | `kernel/api/access.c` | Check user VA permissions before copyin/copyout |
| `sys_read` / `sys_write` / `sys_getdents` | `kernel/api/syscall.c` | User/kernel copies around the VFS calls |
