# WEENIX KERNEL 3 PHASE 3 - CALL CHAIN ANALYSIS

**Created:** 2025-12-06
**Purpose:** Trace all function call chains for shadow objects and fork before implementation

---

## 🔄 SHADOW OBJECT CALL CHAINS

### 1. **Shadow Object Creation**

**Primary Callers:**
```
vmmap_map() [kernel/vm/vmmap.c:404]
  └─> shadow_create() - For MAP_PRIVATE file mappings (COW)

do_fork() [kernel/proc/fork.c:116, 123]
  ├─> shadow_create() - Parent shadow (wraps original vma_obj)
  └─> shadow_create() - Child shadow (wraps parent shadow)
```

**Call Chain:**
```
shadow_create() [shadow.c:94]
  ├─> slab_obj_alloc(shadow_allocator)
  ├─> mmobj_init(shadow, &shadow_mmobj_ops)
  └─> shadow->mmo_refcount = 1
```

**Key Points:**
- Called during fork for EACH private vmarea
- Creates COW hierarchy: bottom_obj ← parent_shadow ← child_shadow
- Each shadow has refcount = 1 at creation

---

### 2. **Shadow Object Initialization**

**Call Chain:**
```
kmain() [kernel/main/kmain.c]
  └─> shadow_init() [kernel/main/kmain_aux.c:107]
      └─> slab_allocator_create("shadow", sizeof(mmobj_t))
```

**When:** Boot time (before any shadow objects created)

---

### 3. **Shadow Lookup Page (Read Path)**

**Primary Callers (via polymorphism):**
```
pframe_lookup(obj, pagenum, forwrite, &pf) [pframe.c]
  └─> obj->mmo_ops->lookuppage(obj, pagenum, forwrite, &pf)
      └─> shadow_lookuppage() [IF obj is shadow]

pframe_get(obj, pagenum, &pf) [pframe.c]
  └─> pframe_lookup(obj, pagenum, 1, &pf) [forwrite=1]
```

**Who calls pframe_lookup?**
```
Page Fault Handler [pagefault.c]
  └─> handle_pagefault()
      └─> pframe_lookup(vma->vma_obj, pagenum, forwrite, &pf)

vmmap_read() [vmmap.c:611-626]
  └─> pframe_lookup(vma->vma_obj, pagenum, 0, &pf) [forwrite=0]

vmmap_write() [vmmap.c:691-706]
  └─> pframe_lookup(vma->vma_obj, pagenum, 1, &pf) [forwrite=1]
```

**shadow_lookuppage() Logic:**
```
shadow_lookuppage(o, pagenum, forwrite, &pf)
  │
  ├─ IF forwrite:
  │    └─> pframe_get(o, pagenum, &pf)  [Allocate page in THIS shadow]
  │         └─> pframe_lookup(o, pagenum, 1, &pf)
  │              └─> Allocates new pframe in o
  │              └─> Calls shadow_fillpage() to fill content
  │
  └─ IF !forwrite (read-only):
       └─> ITERATIVE search down shadow chain:
            current = o
            WHILE current:
              ├─> pf = pframe_get_resident(current, pagenum)
              ├─> IF pf found: return pf (pinned)
              └─> current = current->mmo_shadowed  [Move to parent]
            └─> IF not found in chain:
                 └─> pframe_lookup(bottom_obj, pagenum, 0, &pf)
```

**⚠️ RECURSION PREVENTION:**
- Uses ITERATION not recursion (kernel spec warning!)
- Prevents stack overflow on long shadow chains

---

### 4. **Shadow Fill Page (COW Copy)**

**Primary Caller:**
```
pframe_lookup() [pframe.c]
  └─> IF pframe newly allocated and NOT resident:
      └─> obj->mmo_ops->fillpage(obj, pf)
          └─> shadow_fillpage() [IF obj is shadow]
```

**shadow_fillpage() Logic:**
```
shadow_fillpage(o, pf)
  │
  ├─ ITERATIVE search for source page:
  │    current = o->mmo_shadowed  [Start from parent]
  │    WHILE current:
  │      ├─> source_pf = pframe_get_resident(current, pagenum)
  │      ├─> IF found: BREAK
  │      └─> current = current->mmo_shadowed [Move to parent]
  │
  ├─ IF no source found in chain:
  │    └─> pframe_lookup(bottom_obj, pagenum, 0, &source_pf)
  │
  └─> memcpy(pf->pf_addr, source_pf->pf_addr, PAGE_SIZE)
```

**⚠️ RECURSION PREVENTION:**
- Uses ITERATION not recursion
- Critical for long fork chains

---

### 5. **Shadow Dirty/Clean Page**

**shadow_dirtypage() Caller:**
```
pframe_dirty() [pframe.c]
  └─> obj->mmo_ops->dirtypage(obj, pf)
      └─> shadow_dirtypage() [IF obj is shadow]
          └─> pframe_set_dirty(pf)
```

**shadow_cleanpage() Caller:**
```
pframe_clean() [pframe.c]
  └─> obj->mmo_ops->cleanpage(obj, pf)
      └─> shadow_cleanpage() [IF obj is shadow]
          └─> pframe_clear_dirty(pf)
```

**Key Point:**
- Shadow pages don't write back to disk (no backing store)
- Just set/clear dirty bit

---

### 6. **Shadow Reference Counting**

**shadow_ref() Callers:**
```
vmarea creation/cloning
  └─> vma->vma_obj->mmo_ops->ref(vma->vma_obj)

pframe operations
  └─> pf->pf_obj->mmo_ops->ref(pf->pf_obj)
```

**shadow_put() Callers:**
```
vmarea destruction
  └─> vma->vma_obj->mmo_ops->put(vma->vma_obj)

pframe_free()
  └─> pf->pf_obj->mmo_ops->put(pf->pf_obj)

shadow_put() itself (cleanup)
  └─> o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed)
```

**shadow_put() Cleanup Logic:**
```
shadow_put(o)
  └─> o->mmo_refcount--
  └─> IF refcount == nrespages:  [Object no longer in use]
      ├─> FOR each pframe in o->mmo_respages:
      │    ├─> Wait if busy
      │    ├─> Unpin all pins
      │    ├─> ref(o)  [Prevent recursive cleanup]
      │    └─> pframe_free(pf)  [Calls put(o), balanced by ref above]
      ├─> IF o->mmo_shadowed:
      │    └─> o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed)  [MAY RECURSE]
      └─> slab_obj_free(shadow_allocator, o)
```

**⚠️ POTENTIAL RECURSION:**
- shadow_put() → parent->put() → grandparent->put() → ...
- But limited by fork depth (typically shallow)
- Each put() only recurses if refcount reaches cleanup threshold

---

## 🍴 FORK CALL CHAIN

### Primary Entry Point

```
sys_fork() [syscall handler]
  └─> do_fork(NULL) [kernel/proc/fork.c]
```

### do_fork() High-Level Flow

```
do_fork(regs)
  ├─> proc_create("child")  [Create child process struct]
  │
  ├─> vmmap_clone(curthr->p_proc->p_vmmap, &child->p_vmmap)
  │    └─> FOR each vmarea in parent:
  │         ├─> IF MAP_PRIVATE (not shared):
  │         │    ├─> shadow_create() → parent_shadow
  │         │    ├─> shadow_create() → child_shadow
  │         │    ├─> Setup: bottom_obj ← parent_shadow ← child_shadow
  │         │    ├─> parent vma->vma_obj = parent_shadow
  │         │    └─> child vma->vma_obj = child_shadow
  │         └─> IF MAP_SHARED:
  │              └─> child vma->vma_obj = parent vma->vma_obj (shared)
  │
  ├─> kthread_clone(curthr, &child_thr)  [Clone kernel thread]
  │    └─> Copy register context from parent
  │
  ├─> Setup child to return 0 from fork
  │
  └─> Return child->p_pid to parent
```

### vmmap_clone() Detail (Fork's Shadow Creation)

**Location:** kernel/vm/vmmap.c (likely needs implementation)

```
vmmap_clone(parent_map, &child_map)
  │
  ├─> vmmap_create() → child_map
  │
  └─> FOR each vma in parent_map->vmm_list:
       │
       ├─> vmarea_alloc() → child_vma
       ├─> Copy vma properties (start, end, prot, flags)
       │
       ├─> IF vma->vma_flags & MAP_PRIVATE:
       │    ├─> bottom_obj = mmobj_bottom_obj(vma->vma_obj)
       │    ├─> parent_shadow = shadow_create()
       │    ├─> child_shadow = shadow_create()
       │    │
       │    ├─> Setup parent_shadow:
       │    │    ├─> parent_shadow->mmo_shadowed = vma->vma_obj
       │    │    ├─> parent_shadow->mmo_un.mmo_bottom_obj = bottom_obj
       │    │    └─> vma->vma_obj->mmo_ops->ref(vma->vma_obj)  [Inc refcount]
       │    │
       │    ├─> Setup child_shadow:
       │    │    ├─> child_shadow->mmo_shadowed = parent_shadow
       │    │    └─> child_shadow->mmo_un.mmo_bottom_obj = bottom_obj
       │    │
       │    ├─> Update parent vma:
       │    │    ├─> old_obj = vma->vma_obj
       │    │    ├─> vma->vma_obj = parent_shadow
       │    │    └─> old_obj->mmo_ops->put(old_obj)  [Dec old refcount]
       │    │
       │    └─> Set child vma:
       │         └─> child_vma->vma_obj = child_shadow
       │
       └─> ELSE (MAP_SHARED):
            ├─> child_vma->vma_obj = vma->vma_obj
            └─> vma->vma_obj->mmo_ops->ref(vma->vma_obj)  [Shared, inc refcount]
```

**Result After Fork:**
```
BEFORE fork (parent has file-backed vma):
  parent_vma → vnode_obj (bottom)

AFTER fork (both use shadow objects):
  parent_vma → parent_shadow → vnode_obj (bottom)
  child_vma  → child_shadow → parent_shadow → vnode_obj (bottom)
```

---

## 🔗 POLYMORPHIC CALL CHAINS (mmobj_ops)

### Virtual Function Table Dispatch

**Three mmobj Types:**

1. **Anonymous Objects** (`anon_mmobj_ops`):
   ```c
   anon_mmobj_ops = {
       .ref = anon_ref,
       .put = anon_put,
       .lookuppage = anon_lookuppage,
       .fillpage = anon_fillpage,
       .dirtypage = anon_dirtypage,
       .cleanpage = anon_cleanpage
   };
   ```

2. **Shadow Objects** (`shadow_mmobj_ops`):
   ```c
   shadow_mmobj_ops = {
       .ref = shadow_ref,
       .put = shadow_put,
       .lookuppage = shadow_lookuppage,
       .fillpage = shadow_fillpage,
       .dirtypage = shadow_dirtypage,
       .cleanpage = shadow_cleanpage
   };
   ```

3. **Vnode Objects** (in libdrivers.a - pristine):
   ```c
   // Implementation hidden in precompiled library
   // Accessed when obj->mmo_ops != &anon_mmobj_ops && != &shadow_mmobj_ops
   ```

### Polymorphic Call Sites

**pframe_lookup():**
```c
pframe_lookup(mmobj_t *obj, pagenum, forwrite, &pf)
  └─> ret = obj->mmo_ops->lookuppage(obj, pagenum, forwrite, &pf)
       ├─> IF obj is anon:   anon_lookuppage()
       ├─> IF obj is shadow: shadow_lookuppage()
       └─> IF obj is vnode:  vnode_lookuppage() [precompiled]
```

**Recursive Shadow Lookup Example:**
```
User writes to page in child process after fork
  ↓
handle_pagefault(addr, forwrite=1)
  ↓
pframe_lookup(child_vma->vma_obj, pagenum, 1, &pf)
  ↓
child_shadow->mmo_ops->lookuppage(child_shadow, pagenum, 1, &pf)
  ↓
shadow_lookuppage() with forwrite=1
  ↓
pframe_get(child_shadow, pagenum, &pf)  [Allocate in child shadow]
  ↓
pframe_lookup(child_shadow, pagenum, 1, &pf)
  ↓
Allocates new pframe in child_shadow
  ↓
child_shadow->mmo_ops->fillpage(child_shadow, pf)
  ↓
shadow_fillpage(child_shadow, pf)
  ↓
Search for source: child_shadow → parent_shadow → vnode_obj
  ↓
pframe_lookup(parent_shadow, pagenum, 0, &source_pf)  [Read from parent]
  ↓
parent_shadow->mmo_ops->lookuppage(parent_shadow, pagenum, 0, &source_pf)
  ↓
shadow_lookuppage() with forwrite=0
  ↓
Iterative search: parent_shadow → vnode_obj
  ↓
pframe_lookup(vnode_obj, pagenum, 0, &source_pf)
  ↓
vnode_obj->mmo_ops->lookuppage()  [Reads from disk]
  ↓
Returns source_pf with data from file
  ↓
memcpy(child_pf->pf_addr, source_pf->pf_addr, PAGE_SIZE)  [COW COPY]
  ↓
Return child_pf to page fault handler
  ↓
Page table updated to map virtual address to child_pf
```

---

## ⚠️ CRITICAL RECURSION BASE CASES

### shadow_lookuppage() Base Cases

**Base Case 1:** Page found in shadow chain (read path)
```c
while (current) {
    pf = pframe_get_resident(current, pagenum);
    if (pf) return pf;  // ✅ BASE CASE
    current = current->mmo_shadowed;
}
```

**Base Case 2:** Lookup reaches bottom object
```c
pframe_lookup(bottom_obj, pagenum, 0, &pf);  // ✅ BASE CASE (vnode or anon)
```

### shadow_fillpage() Base Cases

**Base Case 1:** Page found in parent shadow chain
```c
while (current) {
    source_pf = pframe_get_resident(current, pagenum);
    if (source_pf) break;  // ✅ BASE CASE
    current = current->mmo_shadowed;
}
```

**Base Case 2:** No page in chain, get from bottom
```c
pframe_lookup(bottom_obj, pagenum, 0, &source_pf);  // ✅ BASE CASE
```

### shadow_put() Recursion

**Termination:** Recursion stops when refcount != nrespages
```c
o->mmo_refcount--;
if (o->mmo_refcount == o->mmo_nrespages) {
    // ... cleanup ...
    if (o->mmo_shadowed) {
        o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);  // Recurse
    }
}
// ✅ BASE CASE: refcount > nrespages, no cleanup
```

**Max Depth:** Limited by fork depth (typically < 10 levels)

---

## 📝 IMPLEMENTATION PRIORITIES

### Phase 3A: Shadow Objects (Week 1)

1. ✅ Already implemented: `shadow_create()`, `shadow_ref()`, `shadow_put()`
2. ⏸️ TODO: Review and test `shadow_lookuppage()` (already has code)
3. ⏸️ TODO: Review and test `shadow_fillpage()` (already has code)
4. ⏸️ TODO: Review and test `shadow_dirtypage()` and `shadow_cleanpage()`

### Phase 3B: Fork Implementation (Week 1-2)

1. ⏸️ TODO: Implement `vmmap_clone()` to create shadow hierarchy
2. ⏸️ TODO: Implement `do_fork()` to create child process
3. ⏸️ TODO: Test with `/usr/bin/fork-and-wait`

### Phase 3C: Testing (Week 2)

1. ⏸️ Test all section (B) programs twice in a row
2. ⏸️ Verify no kernel restart needed between runs
3. ⏸️ Debug any COW issues with comprehensive logging

---

**Status:** Call chain analysis complete. Ready for polymorphism documentation and logging infrastructure.
