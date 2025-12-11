# Weenix Exec/Fork Refcount Analysis

## Executive Summary

**The problem is NOT fork-related.** The test is running `/usr/bin/hello` directly from init via `kernel_execve()` - NO fork occurs. The vnode refcount leak is happening during the ELF loading process itself.

## Process Flow Analysis

### 1. What Actually Happens

From `kmain.c` line 296-300:
```c
#ifdef __VM__
    /* Phase 2 testing: run hello to test basic VM */
    char *argv[] = { NULL };
    char *envp[] = { NULL };
    kernel_execve("/usr/bin/hello", argv, envp);
```

**This is exec ONLY - no fork!** The init process directly transforms into `/usr/bin/hello`.

### 2. The File Descriptor Journey

#### Step 1: binfmt_load() opens the file
`kernel/api/binfmt.c` lines 65-68:
```c
if (0 > (fd = do_open(filename, O_RDONLY))) {
    return fd;
}
file_t *file = fget(fd);  // +1 vnode ref (via file->f_vnode)
```

**Vnode refcount: 1** (from vfs_root_vn already existing)
**After do_open: refcount = 2** (file descriptor holds a ref)

#### Step 2: binfmt_load() releases early ref
Lines 80-81:
```c
fput(file);  // -1 vnode ref
file = NULL;
```

**Vnode refcount: back to 1** (just the fd)

#### Step 3: binfmt_load() calls _elf32_load()
Line 92:
```c
err = fmt->bf_load(filename, fd, argv, envp, eip, esp);
```

#### Step 4: _elf32_load() gets file again
`kernel/api/elf32.c` line 479:
```c
file = fget(fd);  // +1 vnode ref
KASSERT(NULL != file);
```

**Vnode refcount: 2** (fd still open + file pointer)

#### Step 5: ELF segments mapped
Line 502:
```c
err = _elf32_map_progsegs(file->f_vnode, map, &header, pht, 0);
```

This calls `_elf32_map_segment()` which calls `vmmap_map()`:
```c
ret = vmmap_map(map, file->vnode, lopage, npages, perms,
                MAP_PRIVATE | MAP_FIXED, fileoff, 0, NULL);
```

**THIS IS THE KEY:** `vmmap_map()` creates shadow objects for MAP_PRIVATE mappings.

Looking at your debug output:
```
vmmap_map: refcount=3 lopage=32840 (creates shadow cfe12004)
vmmap_map: refcount=4 lopage=32846 (creates shadow cfe12034)
```

Each `vmmap_map()` with a vnode increments the vnode refcount!

#### Step 6: BSS cleanup
The second shadow (cfe12034) is removed during BSS cleanup:
```
vmmap_remove: removing shadow cfe12034
shadow_put: releasing shadowed obj refcount=4 → becomes 3
```

#### Step 7: _elf32_load() cleanup
Line 710:
```c
if (NULL != file) {
    fput(file);  // -1 vnode ref
}
```

**Vnode refcount: 2** (should be 2 at this point)

#### Step 8: binfmt_load() closes fd
Line 102:
```c
if (0 <= fd) {
    do_close(fd);  // -1 vnode ref
}
```

**Expected vnode refcount: 1** (just the remaining shadow's ref)

## The Actual Problem

### Where Does Refcount Go from 3 → 5?

After BSS cleanup, we have:
- Vnode refcount = 3 (one shadow + fd still open?)

But your debug shows:
```
vmmap_destroy: destroying vmmap=cfe90020    ← Child process
  - releases shadow (cfe12004) refcount=5   ← Should be 3!
```

**The extra +2 comes from vmmap_map() being called TWICE for the two ELF segments!**

Each call to `vmmap_map()` with a file-backed mapping creates a new mmobj and should increment the vnode refcount. Let's trace this:

1. **Initial state:** refcount = 1 (from vfs root)
2. **do_open():** refcount = 2 (fd opens file)
3. **First vmmap_map() (text segment):** refcount = 3 (creates mmobj for text)
4. **Second vmmap_map() (data segment):** refcount = 4 (creates mmobj for data)
5. **BSS cleanup removes data shadow:** refcount = 3
6. **Final state before cleanup:** refcount = 3

But you're seeing refcount = 5 during cleanup. Let me check what happens during exec...

### The exec() vmmap swap

From `elf32.c` lines 678-682:
```c
/* Give the process the new mappings. */
vmmap_t *tempmap = curproc->p_vmmap;
curproc->p_vmmap = map;
map = tempmap; /* So the old maps are cleaned up */
```

Then line 707:
```c
if (NULL != map) {
    vmmap_destroy(map);  // Destroys the OLD vmmap
}
```

**AH HA!** The issue is that we're seeing TWO vmmap_destroy calls:

1. `vmmap_destroy(cfe90020)` - The hello process's vmmap (NEW map)
2. `vmmap_destroy(cfe90058)` - Init's original vmmap (OLD map)

### Why Two vmmap_destroy Calls?

When hello exits:
1. `do_exit()` is called
2. `proc_cleanup()` destroys `curproc->p_vmmap` (the hello vmmap)
3. This releases all vmareas and their mmobj references

But WHY is the vnode refcount 5 instead of 3?

## The Root Cause

Let me check `vmmap.c` to see if `vmmap_map()` properly increments refcount:

**HYPOTHESIS:** Each `vmmap_map()` call for file-backed mappings should call `vnode->mmo_ops->ref(vnode)` to increment the refcount. If this is missing, or if it's being called extra times, that would explain the leak.

**Counter-hypothesis:** The refcount IS correct (5), but the cleanup isn't releasing all 5 references. Let's count them:

1. Root vnode reference (initial)
2. do_open() fd reference
3. First vmmap_map() creates mmobj for text segment
4. Second vmmap_map() creates mmobj for data segment (later removed)
5. ??? Where does the 5th reference come from?

## Next Steps to Debug

1. **Check vmmap_map()**: Does it call `vnode->mmo_ops->ref()` when creating file-backed mappings?

2. **Check shadow_create()**: When a shadow is created for a file-backed mapping, does it increment the bottom object's refcount?

3. **Count all vref() calls**: Trace through and count every place the vnode refcount is incremented

4. **Count all vput() calls**: Trace through and count every place the vnode refcount is decremented

## Expected Refcount Balance

For `/usr/bin/hello` with NO fork:

### Increments (+):
1. Initial vnode creation: +1 (refcount = 1)
2. do_open() in binfmt_load: +1 (refcount = 2)
3. First vmmap_map() (text segment): +1 (refcount = 3)
4. Second vmmap_map() (data/BSS segment): +1 (refcount = 4)

### Decrements (-):
1. fput() in binfmt_load line 80: -1 (refcount = 3)
2. vmmap_remove() for BSS: -1 (refcount = 2)
3. do_close() in binfmt_load line 102: -1 (refcount = 1)
4. vmmap_destroy() releases text shadow: -1 (refcount = 0) ✓

But we're seeing refcount=5 when vmmap_destroy runs, not refcount=2.

## Likely Culprits

1. **vmmap_map() is calling ref() twice per mapping** - once for the vnode and once for creating an anon/shadow?

2. **The file descriptor is not being closed** - do_close() might not be decrementing the refcount

3. **Shadow creation is adding extra refs** - when shadows are created, they might be adding refs to the bottom object incorrectly

4. **The fget() in _elf32_load is not being balanced** - line 479 does fget(fd), but there might be a missing fput()

## Action Items

1. Add debug logging to `vmmap_map()` to see if it's calling `ref()` on the vnode
2. Add debug logging to `shadow_create()` to see if it's incrementing bottom object refcount
3. Check if `do_close()` is actually being called and decrementing the refcount
4. Verify the fget/fput balance in elf32.c

## Update: The TWO vmmaps Mystery

Your debug output shows:
```
vmmap_destroy: destroying vmmap=cfe90020    ← Child process
vmmap_destroy: destroying vmmap=cfe90058    ← Parent (init)
```

But there's NO fork! So why are there two vmmaps?

**Answer:** During exec, a NEW vmmap is created for the loaded binary, then the OLD vmmap (init's original) is destroyed. The swap happens at line 678-682.

So the two vmmaps are:
1. `cfe90020` - The NEW vmmap for /usr/bin/hello
2. `cfe90058` - The OLD vmmap that init had before exec

The "parent" label in your debug is misleading - this is actually the OLD init vmmap being cleaned up as part of exec.

## Why vmmap=cfe90058 is Empty

From your debug:
```
vmmap_destroy: destroying vmmap=cfe90058    ← Parent (init)
  - no vmareas released (empty or already cleaned)
```

This makes sense! Before running `/usr/bin/hello`, init process had no user-space vmareas - it was a kernel-only process. So when exec swaps in the new vmmap, the old one is empty.

## SOLUTION FOUND: The Root Cause

### Key Discovery: `s5fs_mmap()` Implementation

From the kernel spec (line 267):
> "Please note that you do not need to implement zero_mmap() in "kernel/drivers/memdevs.c" and s5fs_mmap() in "kernel/fs/s5fs/s5fs.c" because **they were implemented for you already** (although it doesn't look like it). Their implementations are in "kernel/libdrivers.a" and "kernel/libs5fs.a"."

From the FAQ (line 2678-2679):
> "when s5fs_mmap() returned, the 3rd argument returns a pointer that points to the mmobj that lives inside the vnode (i.e., the first argument of the function), **the refcount of the vnode is incremented** because the 3rd argument is a newly added pointer to the mmobj that lives inside that vnode."

### The Refcount Flow - CORRECTED

Let's trace the complete flow:

#### Increments (+):
1. **Initial vnode creation**: refcount = 1 (exists in filesystem)
2. **do_open() in binfmt_load**: refcount = 2 (fd holds ref)
3. **s5fs_mmap() for text segment**: refcount = 3 (mmobj pointer returned)
4. **s5fs_mmap() for data segment**: refcount = 4 (second mmobj pointer)

#### Decrements (-):
1. **fput() in binfmt_load line 80**: refcount = 3 (releases early file_t ref)
2. **vmmap_remove() for BSS**: refcount = 2 (releases data segment shadow)

At this point: **refcount should be 2** (fd + text segment shadow)

3. **fput() in _elf32_load line 710**: refcount = 1 (releases file_t from fget)
4. **do_close() in binfmt_load line 102**: refcount = 0??? (closes fd)

But wait - the fd close should bring it to 0, but we need the text segment shadow to keep a reference!

### The Actual Problem: do_close() Timing

Looking at the code flow in `binfmt.c`:

```c
int binfmt_load(...) {
    fd = do_open(filename, O_RDONLY);     // refcount = 2
    file_t *file = fget(fd);               // refcount = 3 (NO! fget doesn't increment)
    fput(file);                            // refcount = 2

    err = fmt->bf_load(filename, fd, ...); // Calls _elf32_load

cleanup:
    if (0 <= fd) {
        do_close(fd);                      // This decrements refcount
    }
}
```

And in `_elf32_load()`:
```c
file = fget(fd);                           // Gets file_t, doesn't increment vnode ref
// ... load segments via vmmap_map() which calls s5fs_mmap()
fput(file);                                // Decrements file_t refcount, not vnode
```

### The Bug: fget/fput vs vref/vput Confusion

**fget/fput** operate on `file_t` structures (file descriptors), NOT on vnode refcounts!

Let me re-analyze with this understanding...

### CORRECT Refcount Flow

#### Initial State:
- Vnode refcount = 1 (exists in filesystem)

#### binfmt_load() opens file:
```c
fd = do_open(filename, O_RDONLY);
```
- This creates a file_t and increments vnode refcount
- **Vnode refcount = 2**

#### binfmt_load() does early fget/fput:
```c
file_t *file = fget(fd);  // Gets file_t, increments file_t refcount, NOT vnode
fput(file);                // Decrements file_t refcount, NOT vnode
```
- **Vnode refcount = 2** (unchanged)

#### _elf32_load() gets file:
```c
file = fget(fd);
```
- **Vnode refcount = 2** (fget doesn't touch vnode refcount)

#### First vmmap_map() (text segment):
```c
file->vn_ops->mmap(file, vma, &obj);  // Calls s5fs_mmap()
```
- `s5fs_mmap()` returns vnode's mmobj and **increments vnode refcount**
- **Vnode refcount = 3**

#### Second vmmap_map() (data segment):
```c
file->vn_ops->mmap(file, vma, &obj);  // Calls s5fs_mmap() again
```
- `s5fs_mmap()` returns vnode's mmobj and **increments vnode refcount**
- **Vnode refcount = 4**

#### BSS cleanup (vmmap_remove):
```c
vmmap_remove(map, lopage, npages);
```
- Removes the data segment shadow
- Calls `shadow->mmo_ops->put(shadow)` which calls `vput()` on bottom object
- **Vnode refcount = 3**

#### _elf32_load() cleanup:
```c
if (NULL != file) {
    fput(file);  // Decrements file_t refcount, NOT vnode
}
```
- **Vnode refcount = 3** (unchanged)

#### binfmt_load() closes fd:
```c
if (0 <= fd) {
    do_close(fd);  // Decrements file_t AND vnode refcount
}
```
- **Vnode refcount = 2**

### Expected Final State

After exec completes and before process cleanup:
- **Vnode refcount should be 2**:
  1. One ref from the text segment shadow's bottom object
  2. One ref from... WHERE?

### The Mystery: Why Refcount = 5?

Your debug shows refcount = 5 during vmmap_destroy. But according to my analysis:
- After BSS cleanup: refcount = 3
- After fput in _elf32_load: refcount = 3
- After do_close in binfmt_load: refcount = 2
- Should be 1 by the time vmmap_destroy runs!

**Wait - I need to check if do_close actually runs...**

Let me look at the actual cleanup sequence in elf32.c more carefully...
