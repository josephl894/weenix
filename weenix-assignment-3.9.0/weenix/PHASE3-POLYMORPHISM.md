# WEENIX KERNEL 3 PHASE 3 - POLYMORPHISM & RECURSION GUIDE

**Created:** 2025-12-06
**Purpose:** Document mmobj polymorphism, recursion base cases, and infinite loop prevention

---

## ⚠️ KERNEL SPEC WARNING

> **From kernel spec.txt:291-296:**
> "One major confusion in this assignment is that polymorphism got even more complicated here. In kernel 2, you were asked to explore the code for polymorphism in the file system. But the code was fairly straight-forward since you are only deal with one actual file system and there is not recursion. In this assignment, you will be implementing something called mmobj which has 3 actual instances (i.e., anonymous objects, shadow objects, and there is also a type of mmobj in a vnode) and **some polymorphic function calls may be recursive. So, if you are not carefully, you will end up with an infinite loop.** As all recursive function goes, you should analyze it. You need to figure out what the base cases are and when to go into recursion. Think about this carefully before you write a lot of code."

---

## 📚 THE THREE MMOBJ TYPES

### 1. Anonymous Objects (`anon_mmobj_ops`)

**Purpose:** Private, non-file-backed memory (stack, heap, BSS)

**Location:** `kernel/vm/anon.c`

**Operations Table:**
```c
mmobj_ops_t anon_mmobj_ops = {
    .ref = anon_ref,
    .put = anon_put,
    .lookuppage = anon_lookuppage,
    .fillpage = anon_fillpage,
    .dirtypage = anon_dirtypage,
    .cleanpage = anon_cleanpage
};
```

**Characteristics:**
- ✅ **LEAF NODE** - No parent (`mmo_shadowed` = NULL)
- ✅ **BASE CASE** - Recursion terminates here
- **Backing:** None (pages allocated on demand, zero-filled)
- **Refcount:** Drops to 0 when no vmareas reference it
- **Typical Use:** User stack, heap from brk()/mmap(ANON)

**Type Check:**
```c
if (obj->mmo_ops == &anon_mmobj_ops) {
    // This is an anonymous object
}
```

---

### 2. Shadow Objects (`shadow_mmobj_ops`)

**Purpose:** Copy-on-write (COW) for private mappings after fork()

**Location:** `kernel/vm/shadow.c`

**Operations Table:**
```c
mmobj_ops_t shadow_mmobj_ops = {
    .ref = shadow_ref,
    .put = shadow_put,
    .lookuppage = shadow_lookuppage,
    .fillpage = shadow_fillpage,
    .dirtypage = shadow_dirtypage,
    .cleanpage = shadow_cleanpage
};
```

**Characteristics:**
- ❌ **INTERNAL NODE** - Has parent (`mmo_shadowed` != NULL)
- ❌ **NOT A BASE CASE** - May recurse to parent
- **Backing:** Parent shadow or bottom object (anon/vnode)
- **Refcount:** Complex - managed by vmareas and resident pages
- **Typical Use:** Created during fork() for MAP_PRIVATE vmareas

**Shadow Object Hierarchy After Fork:**
```
BEFORE fork:
  parent_vma → vnode_obj

AFTER fork:
  parent_vma → parent_shadow → vnode_obj
  child_vma  → child_shadow → parent_shadow → vnode_obj
```

**Type Check:**
```c
if (obj->mmo_ops == &shadow_mmobj_ops) {
    // This is a shadow object
}
```

**Key Fields:**
```c
struct mmobj {
    mmobj_ops_t *mmo_ops;           // Points to shadow_mmobj_ops
    mmobj_t *mmo_shadowed;          // Parent in shadow chain
    union {
        mmobj_t *mmo_bottom_obj;    // Leaf node (anon or vnode)
        vnode_t *mmo_vnode;         // (only for vnode objects)
    } mmo_un;
    int mmo_refcount;               // Reference count
    int mmo_nrespages;              // Number of resident pages
    list_t mmo_respages;            // List of resident pframes
};
```

---

### 3. Vnode Objects (Vnode mmobj_ops)

**Purpose:** File-backed memory (executable text, data, shared libraries)

**Location:** `kernel/fs/vnode.c` + `kernel/libs5fs.a` (PRISTINE - precompiled)

**Operations Table:**
```c
// Implementation in precompiled library - cannot view source
// Accessed via vnode->vn_mmobj
```

**Characteristics:**
- ✅ **LEAF NODE** - No parent (backed by disk file)
- ✅ **BASE CASE** - Recursion terminates here
- **Backing:** File on S5FS disk
- **Refcount:** Managed by vnode reference counting
- **Typical Use:** Program text/data segments, shared libraries

**Type Check (Negative Check):**
```c
if (obj->mmo_ops != &anon_mmobj_ops &&
    obj->mmo_ops != &shadow_mmobj_ops) {
    // This is a vnode object (or other non-anon/shadow type)
}
```

**Why Negative Check?**
- Cannot directly access vnode_mmobj_ops symbol (in precompiled library)
- Must use exclusion: "not anon AND not shadow → must be vnode"

---

## 🔄 POLYMORPHIC DISPATCH MECHANISM

### Virtual Function Table Pattern

**All mmobj operations go through function pointers:**

```c
// Generic polymorphic call
int ret = obj->mmo_ops->lookuppage(obj, pagenum, forwrite, &pf);

// Dispatches to:
// - anon_lookuppage()   if obj->mmo_ops == &anon_mmobj_ops
// - shadow_lookuppage() if obj->mmo_ops == &shadow_mmobj_ops
// - vnode_lookuppage()  if obj is vnode object
```

### Key Polymorphic Call Sites

**1. pframe_lookup() - Main dispatch point**
```c
int pframe_lookup(mmobj_t *obj, uint32_t pagenum, int forwrite, pframe_t **result)
{
    // ...
    ret = obj->mmo_ops->lookuppage(obj, pagenum, forwrite, result);
    if (ret == 0 && *result) {
        // Page found, now fill it if needed
        if (!pframe_is_resident(*result)) {
            ret = obj->mmo_ops->fillpage(obj, *result);
        }
    }
    // ...
}
```

**2. Reference counting**
```c
vma->vma_obj->mmo_ops->ref(vma->vma_obj);   // Increment refcount
vma->vma_obj->mmo_ops->put(vma->vma_obj);   // Decrement refcount
```

**3. Page operations**
```c
obj->mmo_ops->dirtypage(obj, pf);   // Mark page dirty
obj->mmo_ops->cleanpage(obj, pf);   // Clean page
```

---

## 🌳 RECURSION ANALYSIS

### Potential Recursion Points

**1. shadow_lookuppage() → pframe_lookup() → shadow_lookuppage()**

**Scenario:** Reading from shadow object, page not found in chain

```c
shadow_lookuppage(child_shadow, pagenum, forwrite=0, &pf)
  │
  ├─ Iterative search: child_shadow → parent_shadow → ... [NO RECURSION]
  │
  └─ Not found in chain, lookup from bottom:
      pframe_lookup(bottom_obj, pagenum, 0, &pf)
        │
        └─ bottom_obj->mmo_ops->lookuppage()
            ├─ IF bottom is vnode: vnode_lookuppage() ✅ BASE CASE
            ├─ IF bottom is anon:  anon_lookuppage()  ✅ BASE CASE
            └─ IF bottom is shadow: shadow_lookuppage() [RECURSION!]
```

**Protection:** Bottom object is ALWAYS anon or vnode (never shadow)
```c
// vmmap_clone() ensures bottom_obj is leaf:
mmobj_t *bottom_obj = mmobj_bottom_obj(vma->vma_obj);
child_shadow->mmo_un.mmo_bottom_obj = bottom_obj;  // Always anon or vnode
```

---

**2. shadow_fillpage() → pframe_lookup() → shadow_fillpage()**

**Scenario:** Filling shadow page, source lookup triggers another fill

```c
shadow_fillpage(child_shadow, pf)
  │
  ├─ Search for source: child_shadow → parent_shadow → ...
  │
  ├─ Not found, get from bottom:
  │   pframe_lookup(bottom_obj, pagenum, 0, &source_pf)
  │     └─ bottom_obj->mmo_ops->lookuppage()
  │         └─ ✅ BASE CASE (anon or vnode, not shadow)
  │
  └─ Source found in parent_shadow:
      pframe_lookup(parent_shadow, pagenum, 0, &source_pf)
        │
        └─ parent_shadow->mmo_ops->lookuppage()
            └─ shadow_lookuppage(parent_shadow, pagenum, forwrite=0)
                │
                └─ Search chain, may call:
                    pframe_lookup(grandparent_shadow, pagenum, 0)
                      └─ [RECURSION CONTINUES...]
                          └─ Eventually: pframe_lookup(bottom_obj, ...)
                              └─ ✅ BASE CASE
```

**Protection:**
- Recursion depth limited by fork chain depth
- Typical: < 10 levels (rarely fork more than 10 times)
- Eventually hits bottom object (anon or vnode)

---

**3. shadow_put() → parent->put() → grandparent->put()**

**Scenario:** Shadow cleanup triggers parent cleanup

```c
shadow_put(child_shadow)
  │
  └─ IF refcount == nrespages:
      ├─ Free all resident pages
      │
      └─ IF mmo_shadowed:
          child_shadow->mmo_shadowed->mmo_ops->put(parent_shadow)
            │
            └─ shadow_put(parent_shadow)
                │
                └─ IF refcount == nrespages:
                    └─ parent_shadow->mmo_shadowed->mmo_ops->put(bottom_obj)
                        │
                        └─ bottom_obj->mmo_ops->put()
                            ├─ IF vnode: vnode_put() ✅ BASE CASE
                            └─ IF anon:  anon_put()  ✅ BASE CASE
```

**Protection:**
- Each put() only recurses if refcount drops to cleanup threshold
- Most puts just decrement refcount and return (no recursion)
- Max depth = fork chain depth (typically < 10)

---

## 🛡️ BASE CASES (Recursion Terminators)

### Base Case #1: Vnode Objects

**When Hit:**
- shadow_lookuppage() searches to bottom → vnode object
- shadow_fillpage() gets source from bottom → vnode object

**Why It Terminates:**
- Vnode objects have NO parent (`mmo_shadowed` = NULL, backed by file)
- vnode_lookuppage() reads from disk, returns page
- No further recursion possible

**Code Pattern:**
```c
mmobj_t *bottom = mmobj_bottom_obj(shadow);
pframe_lookup(bottom, pagenum, 0, &pf);
// IF bottom is vnode:
//   vnode->mmo_ops->lookuppage() → reads from S5FS → BASE CASE ✅
```

---

### Base Case #2: Anonymous Objects

**When Hit:**
- shadow_lookuppage() searches to bottom → anon object
- shadow_fillpage() gets source from bottom → anon object

**Why It Terminates:**
- Anon objects have NO parent (`mmo_shadowed` = NULL)
- anon_lookuppage() allocates new zero-filled page
- No further recursion possible

**Code Pattern:**
```c
mmobj_t *bottom = mmobj_bottom_obj(shadow);
pframe_lookup(bottom, pagenum, 0, &pf);
// IF bottom is anon:
//   anon->mmo_ops->lookuppage() → allocates page → BASE CASE ✅
```

---

### Base Case #3: Page Found in Shadow Chain

**When Hit:**
- shadow_lookuppage() iterates chain, finds page resident

**Why It Terminates:**
- Page already exists, no need to recurse further
- Return immediately with pinned page

**Code Pattern:**
```c
while (current) {
    pframe_t *pf = pframe_get_resident(current, pagenum);
    if (pf) {
        pframe_pin(pf);
        *result = pf;
        return 0;  // ✅ BASE CASE - found page, stop searching
    }
    current = current->mmo_shadowed;
}
```

---

### Base Case #4: Refcount Above Cleanup Threshold

**When Hit:**
- shadow_put() decrements refcount, but refcount > nrespages

**Why It Terminates:**
- Object still in use, no cleanup needed
- No recursive put() call to parent

**Code Pattern:**
```c
shadow_put(mmobj_t *o) {
    o->mmo_refcount--;
    if (o->mmo_refcount == o->mmo_nrespages) {
        // Cleanup and recurse
        o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
    }
    // ✅ BASE CASE - refcount > nrespages, just return
}
```

---

## 🚨 INFINITE LOOP SCENARIOS & PREVENTION

### Scenario 1: Circular Shadow Chain

**Bad Code:**
```c
// WRONG - creates cycle!
shadow_a->mmo_shadowed = shadow_b;
shadow_b->mmo_shadowed = shadow_a;  // ❌ CYCLE!
```

**Symptom:** Stack overflow in shadow_lookuppage() or shadow_fillpage()

**Prevention:**
- Shadow chains MUST be acyclic trees
- Always point to parent (older object), never child
- Bottom object MUST be anon or vnode (NULL parent)

**Correct Code:**
```c
// ✅ Correct hierarchy
child_shadow->mmo_shadowed = parent_shadow;
parent_shadow->mmo_shadowed = vnode_obj;  // Leaf
vnode_obj->mmo_shadowed = NULL;           // No parent
```

---

### Scenario 2: Shadow Points to Shadow as Bottom

**Bad Code:**
```c
// WRONG - bottom object is shadow!
mmobj_t *bottom = mmobj_bottom_obj(shadow);
// If bottom is ANOTHER shadow instead of anon/vnode → infinite recursion
```

**Symptom:** Stack overflow when pframe_lookup(bottom, ...) recurses

**Prevention:**
```c
// ✅ Always ensure bottom is leaf
mmobj_t *bottom = mmobj_bottom_obj(vma->vma_obj);
KASSERT(bottom->mmo_ops == &anon_mmobj_ops ||
        (bottom->mmo_ops != &shadow_mmobj_ops &&
         bottom->mmo_ops != &anon_mmobj_ops));  // Must be vnode
```

---

### Scenario 3: Recursive pframe_lookup() in Fill

**Bad Code:**
```c
shadow_fillpage(mmobj_t *o, pframe_t *pf) {
    // WRONG - may trigger infinite recursion if not careful
    pframe_t *source;
    pframe_lookup(o, pf->pf_pagenum, 1, &source);  // ❌ forwrite=1!
}
```

**Symptom:** Stack overflow - fillpage calls lookuppage with forwrite=1, which allocates new page, calls fillpage again...

**Prevention:**
```c
// ✅ Always use forwrite=0 in fillpage
shadow_fillpage(mmobj_t *o, pframe_t *pf) {
    pframe_t *source;
    mmobj_t *parent = o->mmo_shadowed;
    pframe_lookup(parent, pf->pf_pagenum, 0, &source);  // ✅ Read-only
}
```

---

### Scenario 4: Deep Fork Chains

**Scenario:** Process forks 100 times in a row

```
process_1
  └─ fork() → process_2
      └─ fork() → process_3
          └─ ... (98 more forks) ...
              └─ fork() → process_100
```

**Shadow Chain:**
```
child_shadow_100 → ... → child_shadow_3 → child_shadow_2 → vnode_obj
(99 levels deep!)
```

**Symptom:** Stack overflow if using recursion

**Prevention:**
```c
// ✅ Use ITERATION not recursion (kernel spec mandates this!)
shadow_lookuppage() {
    mmobj_t *current = o;
    while (current) {  // ITERATIVE
        pframe_t *pf = pframe_get_resident(current, pagenum);
        if (pf) return pf;
        current = current->mmo_shadowed;  // Move to parent
    }
}

// ❌ BAD - recursive version
shadow_lookuppage() {
    pframe_t *pf = pframe_get_resident(o, pagenum);
    if (pf) return pf;
    return o->mmo_shadowed->mmo_ops->lookuppage(...);  // RECURSIVE - BAD!
}
```

---

## 🔍 DEBUGGING INFINITE LOOPS

### Detection Strategy #1: Recursion Depth Counter

**Add to shadow.c:**
```c
static int shadow_lookup_depth = 0;
static int shadow_fill_depth = 0;

shadow_lookuppage(...) {
    shadow_lookup_depth++;
    dbg(DBG_TEMP, "SHADOW_LOOKUP_DEPTH=%d obj=%p pagenum=%d\n",
        shadow_lookup_depth, o, pagenum);
    KASSERT(shadow_lookup_depth < 50 && "Infinite shadow_lookuppage recursion!");

    // ... function body ...

    shadow_lookup_depth--;
}
```

**Benefits:**
- Catches infinite loops before stack overflow
- Shows recursion depth in debug output
- Threshold 50 is reasonable (typical < 10)

---

### Detection Strategy #2: Chain Traversal Logging

**Add to iteration loops:**
```c
shadow_lookuppage(...) {
    mmobj_t *current = o;
    int chain_depth = 0;

    while (current) {
        chain_depth++;
        dbg(DBG_TEMP, "SHADOW_CHAIN_DEPTH=%d current=%p ops=%p shadowed=%p\n",
            chain_depth, current, current->mmo_ops, current->mmo_shadowed);

        if (chain_depth > 100) {
            panic("Shadow chain too deep - possible cycle!");
        }

        pframe_t *pf = pframe_get_resident(current, pagenum);
        if (pf) return pf;
        current = current->mmo_shadowed;
    }
}
```

---

### Detection Strategy #3: Parent Pointer Validation

**Add assertions in shadow_create():**
```c
shadow_create() {
    mmobj_t *shadow = slab_obj_alloc(shadow_allocator);
    mmobj_init(shadow, &shadow_mmobj_ops);
    shadow->mmo_shadowed = NULL;  // Initially no parent
    shadow->mmo_un.mmo_bottom_obj = NULL;  // Set later
    return shadow;
}

// When setting parent:
void shadow_set_parent(mmobj_t *shadow, mmobj_t *parent) {
    KASSERT(shadow->mmo_ops == &shadow_mmobj_ops);
    KASSERT(parent != shadow);  // ✅ Prevent self-reference
    KASSERT(shadow->mmo_shadowed == NULL);  // ✅ Only set once

    shadow->mmo_shadowed = parent;
    parent->mmo_ops->ref(parent);
}
```

---

## ✅ IMPLEMENTATION CHECKLIST

### Before Writing Code

- [x] Read this polymorphism guide completely
- [x] Understand all 3 mmobj types
- [x] Identify all base cases
- [x] Understand why iteration > recursion
- [ ] Add recursion depth counters to shadow.c
- [ ] Add chain traversal logging
- [ ] Plan test cases for deep fork chains

### During Implementation

- [ ] Use iteration NOT recursion in shadow_lookuppage()
- [ ] Use iteration NOT recursion in shadow_fillpage()
- [ ] Always check: bottom object is anon or vnode (never shadow)
- [ ] Always use forwrite=0 when looking up source pages
- [ ] Never create circular shadow chains
- [ ] Test with single fork first, then multiple forks

### After Implementation

- [ ] Test `/usr/bin/fork-and-wait` (5 points!)
- [ ] Run twice in a row without restart
- [ ] Check debug output for infinite recursion warnings
- [ ] Verify refcount cleanup with shadow_count
- [ ] Test deep fork chains (fork in loop)

---

**Status:** Polymorphism guide complete. Ready for logging infrastructure implementation.
