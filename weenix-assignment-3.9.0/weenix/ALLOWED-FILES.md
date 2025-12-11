# WEENIX KERNEL 3 (VM) - ALLOWED FILES FOR MODIFICATION

## CRITICAL RULE
**ONLY files listed in `make vm-submit` can be modified. ALL other files are PRISTINE and CANNOT be changed.**

From kernel spec line 327:
> "Please note that you are not permitted to change any other existing files. If you submit another existing file, it will be deleted before grading."

## ALLOWED FILES (vm-submit)

The ONLY files that can be modified for Kernel 3 are:

```
Config.mk
vm-README.txt
kernel/main/kmain.c
kernel/proc/kmutex.c
kernel/proc/kthread.c
kernel/proc/proc.c
kernel/proc/sched.c
kernel/proc/sched_helper.c
kernel/fs/namev.c
kernel/fs/open.c
kernel/fs/vfs_syscall.c
kernel/fs/vn_special.c
kernel/api/access.c
kernel/api/syscall.c
kernel/mm/pframe.c
kernel/proc/fork.c
kernel/vm/anon.c
kernel/vm/brk.c
kernel/vm/mmap.c
kernel/vm/pagefault.c
kernel/vm/shadow.c
kernel/vm/vmmap.c
```

**Total: 22 files + Config.mk + README**

## PRISTINE FILES (CANNOT MODIFY)

### Critical Pristine Files Related to Current Bug

**File System:**
- `kernel/fs/vnode.c` ❌ PRISTINE
- `kernel/fs/vfs.c` ❌ PRISTINE
- `kernel/fs/vn_mmobj_ops.c` ❌ PRISTINE
- `kernel/fs/s5fs/s5fs.c` ❌ PRISTINE (precompiled in libs5fs.a)
- `kernel/fs/s5fs/*.c` ❌ ALL PRISTINE

**Drivers:**
- `kernel/drivers/*` ❌ ALL PRISTINE
- `kernel/drivers/blockdev.c` ❌ PRISTINE

**Memory Management:**
- `kernel/mm/pagetable.c` ❌ PRISTINE
- `kernel/mm/page.c` ❌ PRISTINE
- `kernel/mm/slab.c` ❌ PRISTINE
- `kernel/mm/tlb.c` ❌ PRISTINE

**ALL HEADER FILES:**
- `kernel/include/**/*.h` ❌ **ALL PRISTINE - NO HEADERS CAN BE MODIFIED**

This includes:
- `kernel/include/vm/anon.h` ❌ CANNOT MODIFY
- `kernel/include/vm/shadow.h` ❌ CANNOT MODIFY
- `kernel/include/vm/vmmap.h` ❌ CANNOT MODIFY
- `kernel/include/mm/pframe.h` ❌ CANNOT MODIFY
- `kernel/include/fs/vnode.h` ❌ CANNOT MODIFY

**Precompiled Libraries:**
- `kernel/libs5fs.a` ❌ PRISTINE (binary)
- `kernel/libdrivers.a` ❌ PRISTINE (binary)

## FAQ Clarification (Line 314-321)

Q: What files am I allowed to modify and to what extent?

A: **If a file is supposed to be included in the submission, you can modify anything in that file.**

To see what files are included in kernel 3 submission:
```bash
make vm-submit
```

## CONSEQUENCES OF VIOLATING THIS RULE

1. **Submitted files not in vm-submit list**: Will be **DELETED** before grading
2. **Instructions to modify pristine files**: Will be **DISREGARDED**
3. **Solutions that require modifying pristine files**: **CANNOT BE IMPLEMENTED**

## DEBUGGING CONSTRAINTS

When debugging, you are ONLY allowed to:

1. ✅ Add debug output (`dbg()` calls) to allowed files
2. ✅ Modify logic in allowed files
3. ✅ Add helper functions to allowed files (same file only)
4. ❌ **CANNOT** modify header files
5. ❌ **CANNOT** modify pristine .c files
6. ❌ **CANNOT** add new files to kernel

## IMPORTANT IMPLICATIONS FOR CURRENT BUG

**The pinned vnode pages at shutdown bug MUST be fixed using ONLY:**
- `kernel/mm/pframe.c` ✅ (allowed)
- `kernel/vm/pagefault.c` ✅ (allowed)
- `kernel/vm/vmmap.c` ✅ (allowed)
- `kernel/vm/shadow.c` ✅ (allowed)
- `kernel/vm/anon.c` ✅ (allowed)
- `kernel/vm/mmap.c` ✅ (allowed)

**CANNOT use:**
- `kernel/fs/vnode.c` ❌ (pristine - vnode cleanup code)
- `kernel/fs/s5fs/s5fs.c` ❌ (pristine - s5fs mount/umount)
- `kernel/include/vm/anon.h` ❌ (header file)
- `kernel/include/vm/shadow.h` ❌ (header file)

## ASSUMPTION ABOUT PRISTINE CODE

From FAQ line 566:
> "Please also remember that you must assume that the pristine kernel source is **perfect** and there is no bug in the pristine kernel source."

This means:
- libs5fs.a is perfect
- vnode.c is perfect
- s5fs.c (precompiled) is perfect
- Any pinning/unpinning behavior in pristine code is **intentional and correct**

## ACTION ITEMS

1. **Revert any modifications to header files** (anon.h, shadow.h)
2. **Find workarounds that don't require modifying headers**
3. **All solutions must work within allowed .c files only**

