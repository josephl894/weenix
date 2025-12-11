# Vnode Refcount Leak Debug Plan

## Current Status: DEBUGGING
**Date:** 2025-12-05
**Config:** VM=1, S5FS=1, DBG=error,test,temp
**Test:** Phase 2 - /usr/bin/hello

## Bug Summary
**Error:** `Vnode 42 mode 100 device 200 flags 0 is still in use with refcount=2 and 0 res pages`
- Vnode has refcount=2 at shutdown, should be 1 (only root filesystem ref)
- **Leak = 1 extra reference**

## Previous Debug Output Analysis

### File Mappings Created (from vmmap_map):
```
vmmap_map: mapped file vnode=cfe8a1f4 vn_mode=4000 vn_devid=0 refcount=3 lopage=32840 npages=6
vmmap_map: mapped file vnode=cfe8a1f4 vn_mode=4000 vn_devid=0 refcount=4 lopage=32846 npages=1
```
- **SAME vnode mapped TWICE** (vnode=cfe8a1f4)
- Refcount goes 3→4 after second mapping
- vn_mode=4000 (octal) = regular file = `/usr/bin/hello`

### Vmareas Cleaned Up (from vmmap_destroy):
```
1. vma_start=32825 obj=cfe11034 (bottom=cfe11034) - anonymous (stack)
2. vma_start=32840 obj=cfe12004 (bottom=cfe8a1fc) - shadow with file-backed bottom ✓
3. vma_start=32846 obj=cfe11004 (bottom=cfe11004) - ANONYMOUS! ✗
```

**THE PROBLEM:**
- Second file mapping (lopage=32846) is being cleaned up as ANONYMOUS instead of shadow!
- When `obj == bottom`, it means NO shadow object = pure anonymous mapping
- But it was originally file-backed!

### Refcount Arithmetic:
```
Initial vnode refcount: 3 (root FS + other refs)
After first s5fs_mmap(): 4 (file mapped)
After second s5fs_mmap(): 5 (same file mapped again)
During cleanup of first vmarea: shadow_put() → put(vnode mmobj) → 5→4
During cleanup of second vmarea: anon_put() → NO put(vnode mmobj)! → stays 4
Expected final: 3
Actual final: 4
Reported at shutdown: refcount=2 (possibly different vnode? need to verify)
```

## Root Cause Hypothesis

**Issue:** ELF loader's `_elf32_map_segment()` in `kernel/api/elf32.c` lines 118-131

When an ELF segment has:
- `filesz < memsz` (segment has BSS portion that needs zero-initialization)
- `filesz` doesn't end on page boundary

**Sequence:**
1. **Lines 99-116:** Map file portion → creates file-backed vmarea with shadow
   - vmmap_map(file, lopage=32846, npages=1, MAP_PRIVATE|MAP_FIXED)
   - Creates shadow object over vnode's mmobj
   - Vnode refcount incremented

2. **Lines 118-131:** Map BSS portion
   - Calculates: `lopage = ADDR_TO_PN(addr + filesz)` = 32846
   - **BUG:** Same lopage as file portion!
   - Calls: vmmap_map(NULL, lopage=32846, npages=1, MAP_PRIVATE|MAP_FIXED)
   - Because lopage is non-zero and range NOT empty, vmmap_map calls:
     ```c
     vmmap_remove(map, 32846, 1)  // Line 361 in vmmap.c
     ```
   - This **REMOVES the file-backed vmarea** we just created!
   - vmmap_remove() calls `put()` on shadow
   - Shadow `put()` calls `put()` on vnode mmobj
   - Vnode refcount decremented (premature release!)
   - Then creates ANONYMOUS vmarea in its place

3. **Cleanup:** Second vmarea is now anonymous → anon_put() → no vnode release → **LEAK**

## Debug Tracking Added

### Files Modified for Debug:
1. **kernel/vm/vmmap.c:388-389** - Track file mappings
   ```c
   dbg(DBG_TEMP, "vmmap_map: mapped file vnode=%p vn_mode=%o vn_devid=%d refcount=%d lopage=%d npages=%d\n", ...)
   ```

2. **kernel/vm/vmmap.c:405-406** - Track shadow creation
   ```c
   dbg(DBG_TEMP, "vmmap_map: created shadow=%p shadowed=%p (vnode mmobj) bottom=%p\n", ...)
   ```

3. **kernel/vm/vmmap.c:480-481** - Track vmarea removal
   ```c
   dbg(DBG_TEMP, "vmmap_remove: removing vmarea vma_start=%d npages=%d obj=%p\n", ...)
   ```

4. **kernel/vm/vmmap.c:148, 156-158** - Track vmmap destruction
   ```c
   dbg(DBG_TEMP, "vmmap_destroy: destroying vmmap=%p with vmareas\n", ...)
   dbg(DBG_TEMP, "vmmap_destroy: releasing vmarea obj=%p (bottom=%p) vma_start=%d npages=%d\n", ...)
   ```

5. **kernel/vm/shadow.c:161-162** - Track shadow releasing shadowed object
   ```c
   dbg(DBG_TEMP, "shadow_put: releasing shadowed obj=%p refcount=%d\n", ...)
   ```

6. **kernel/vm/anon.c:119-120, 125-126** - Track anon destruction
7. **kernel/vm/shadow.c:134-135, 144-145** - Track shadow destruction
8. **kernel/mm/pframe.c:570-571** - Track pframe freeing

## What to Look For in Next Test Run

### Expected Output Sequence:

**1. During ELF Loading:**
```
vmmap_map: mapped file vnode=<V> vn_mode=<M> vn_devid=<D> refcount=<N> lopage=<L1> npages=<N1>
vmmap_map: created shadow=<S1> shadowed=<V_mmobj> bottom=<V_mmobj>

vmmap_map: mapped file vnode=<V> vn_mode=<M> vn_devid=<D> refcount=<N+1> lopage=<L2> npages=<N2>
vmmap_map: created shadow=<S2> shadowed=<V_mmobj> bottom=<V_mmobj>

[IF BSS OVERLAP OCCURS:]
vmmap_remove: removing vmarea vma_start=<L2> npages=<N2> obj=<S2>
shadow_put: destroying obj=<S2> with 0 resident pages
shadow_put: releasing shadowed obj=<V_mmobj> refcount=<X>
[THEN ANONYMOUS MAPPING CREATED IN ITS PLACE]
```

**2. During Cleanup:**
```
vmmap_destroy: destroying vmmap=<M> with vmareas
vmmap_destroy: releasing vmarea obj=<S1> (bottom=<V_mmobj>) vma_start=<L1> npages=<N1>
shadow_put: destroying obj=<S1> with 0 resident pages
shadow_put: releasing shadowed obj=<V_mmobj> refcount=<Y>

vmmap_destroy: releasing vmarea obj=<A> (bottom=<A>) vma_start=<L2> npages=<N2>
anon_put: destroying obj=<A> with <N> resident pages
[NO VNODE RELEASE HERE - THIS IS THE BUG!]
```

### Key Questions to Answer:
1. **Does vmmap_remove get called between the two file mappings?**
   - YES → BSS overlap is removing the second file mapping
   - NO → Different bug

2. **Do we see TWO shadow creations with the SAME shadowed object?**
   - YES → Both point to same vnode mmobj
   - Verify refcount increments match

3. **Do we see a shadow_put message with "releasing shadowed obj" for EACH shadow created?**
   - If only ONE release but TWO shadows → **LEAK CONFIRMED**

4. **What is the bottom object for the second vmarea during cleanup?**
   - If `obj == bottom` → Anonymous (confirms premature replacement)
   - If `obj != bottom` → Shadow still exists

## Potential Fixes

### Option 1: Prevent BSS Overlap in elf32.c (lines 121-127)
```c
if (memsz > filesz) {
    uint32_t lopage = ADDR_TO_PN(addr + filesz);
    /* If filesz doesn't end on page boundary and we have file data,
     * skip the partial page to avoid removing the file-backed mapping */
    if (!PAGE_ALIGNED(addr + filesz) && filesz > 0) {
        lopage++;
    }
    uint32_t npages = ADDR_TO_PN(PAGE_ALIGN_UP(addr + memsz)) - lopage;
    ...
```

**BUT:** Lines 131-151 expect the anonymous mapping to overlap for the partial page copy!

### Option 2: Make vmmap_remove Shadow-Aware
- When removing a file-backed shadow vmarea that's about to be replaced with BSS
- Don't fully destroy it, just convert to anonymous
- **Complex and risky**

### Option 3: Fix the Partial Page Logic
- The partial page copy (lines 131-151) is meant to handle exactly this case
- Maybe the issue is that it's not being executed?
- Or the execution order is wrong?

## Next Steps

1. **Run test with enhanced debug tracking** (already synced)
2. **Analyze output** to confirm BSS overlap hypothesis
3. **Compare expected vs actual shadow_put calls**
4. **Verify vnode identity** (is leaked vnode same as file vnode?)
5. **Implement appropriate fix** based on findings

## Files Currently Modified
- kernel/vm/vmmap.c (debug output)
- kernel/vm/shadow.c (debug output, recursive cleanup fix)
- kernel/vm/anon.c (debug output, recursive cleanup fix)
- kernel/mm/pframe.c (debug output, list init, use-after-free fix)
- kernel/vm/vnode.c (skip pinned pages)
- kernel/proc/proc.c (vmmap lifecycle fixes)
- Config.mk (DBG=error,test,temp)

## Known Good: All Previous Bugs Fixed
✅ 1. pframe.c race condition (mark busy on alloc)
✅ 2. pageoutd trying to free pinned pages
✅ 3. vnode.c deadlock (skip pinned, don't wait)
✅ 4. vput() KASSERT for pinned pages
✅ 5. proc_create() missing VM initialization
✅ 6. anon_put() and shadow_put() unpin before free
✅ 7. do_waitpid() double-free vmmap
✅ 8. vmarea_alloc() uninitialized list links
✅ 9. pframe_alloc() uninitialized list links
✅ 10. anon_lookuppage() double-pin bug
✅ 11. shadow_lookuppage() missing pin
✅ 12. pframe_free() use-after-free
✅ 13. Recursive cleanup in anon/shadow put

**Current Bug:** #14 - Vnode refcount leak (BSS overlap hypothesis)

## Important Note
**s5fs_mmap() is NOT implemented by us** - it's in libs5fs.a (precompiled library).
We do NOT need to implement it per the kernel spec.
