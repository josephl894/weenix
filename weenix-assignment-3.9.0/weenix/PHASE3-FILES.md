# WEENIX KERNEL 3 PHASE 3 - FILE MODIFICATION GUIDE

**Created:** 2025-12-06
**Purpose:** Track allowed/pristine files for Phase 3 (shadow objects + fork)

---

## ✅ ALLOWED FILES (vm-submit)

### **Phase 3 PRIMARY TARGETS** (Must Implement)

**Shadow Objects:**
- `kernel/vm/shadow.c` - Shadow object operations (COW semantics)

**Fork System Call:**
- `kernel/proc/fork.c` - Fork implementation

**Address Space Manipulation:**
- `kernel/vm/vmmap.c` - Memory map operations (already modified in Phase 2)

### **Phase 3 SUPPORTING FILES** (May Need Modification)

**Virtual Memory:**
- `kernel/vm/anon.c` - Anonymous object operations
- `kernel/vm/pagefault.c` - Page fault handler (already modified in Phase 2)
- `kernel/vm/brk.c` - Break system call (Phase 4)
- `kernel/vm/mmap.c` - Mmap system call (Phase 4)

**Memory Management:**
- `kernel/mm/pframe.c` - Page frame operations (already has FIX #47-48)

**Process Management:**
- `kernel/proc/proc.c` - Process operations
- `kernel/proc/kthread.c` - Kernel thread operations

**System Calls:**
- `kernel/api/syscall.c` - System call handler
- `kernel/api/access.c` - Memory access validation

**File System:**
- `kernel/fs/namev.c` - Name resolution
- `kernel/fs/open.c` - File opening
- `kernel/fs/vfs_syscall.c` - VFS system calls
- `kernel/fs/vn_special.c` - Special vnodes

**Scheduling:**
- `kernel/proc/sched.c` - Scheduler
- `kernel/proc/sched_helper.c` - Scheduler helpers
- `kernel/proc/kmutex.c` - Mutex implementation

**Testing:**
- `kernel/main/kmain.c` - Main kernel entry (for test harness)

**Configuration:**
- `Config.mk` - Build configuration

**Documentation:**
- `vm-README.txt` - Submission documentation

---

## ❌ PRISTINE FILES (CANNOT MODIFY)

### **ALL Header Files**
```
kernel/**/*.h
```
⚠️ **ALWAYS VERIFY**: `git status <file>` before editing

### **File System Implementation**
```
kernel/fs/vnode.c         - Vnode operations (caused FIX #47-48 discovery)
kernel/fs/vfs.c           - VFS core
kernel/fs/s5fs/*.c        - S5 filesystem (all files)
```

### **Precompiled Libraries**
```
kernel/libs5fs.a          - S5FS implementation
kernel/libdrivers.a       - Device drivers
```

### **Driver Code**
```
kernel/drivers/**/*.c     - All drivers (except what's in libdrivers.a)
```

---

## 🎯 PHASE 3 IMPLEMENTATION CHECKLIST

### Shadow Objects (`kernel/vm/shadow.c`)

**Required Functions** (search for `NOT_YET_IMPLEMENTED("VM: ...)`):
- [ ] `shadow_init()` - Initialize shadow object
- [ ] `shadow_ref()` - Increment reference count
- [ ] `shadow_put()` - Decrement reference, cleanup if zero
- [ ] `shadow_lookuppage()` - Lookup page (may be recursive!)
- [ ] `shadow_fillpage()` - Fill page from parent (COW trigger)
- [ ] `shadow_dirtypage()` - Mark page dirty (COW write)
- [ ] `shadow_cleanpage()` - Clean page

**Key Concepts:**
- **Copy-On-Write (COW)**: Child shares parent pages until write occurs
- **Recursive lookups**: shadow → parent shadow → ... → vnode (base case)
- **Base case**: Vnode objects are leaf nodes (no parent)

### Fork System Call (`kernel/proc/fork.c`)

**Required Functions**:
- [ ] `do_fork()` - Main fork implementation

**Key Steps:**
1. Create new process (child)
2. Clone address space with shadow objects (COW)
3. Copy kernel thread context
4. Return 0 to child, child PID to parent

### Address Space (`kernel/vm/vmmap.c`)

**Likely Needed** (already modified in Phase 2):
- [ ] `vmmap_clone()` - Clone address space for fork (creates shadow objects)
- [ ] Review existing vmmap_read/write modifications (FIX #43)

---

## 📊 MMOBJ POLYMORPHISM REFERENCE

### Three mmobj Types (Kernel Spec Warning!)

**1. Anonymous Objects** (`kernel/vm/anon.c`)
- **Purpose**: Private memory (stack, heap)
- **Parent**: None
- **Base Case**: YES (leaf node)
- **mmo_ops**: `anon_mmobj_ops`

**2. Shadow Objects** (`kernel/vm/shadow.c`)
- **Purpose**: Copy-on-write (fork child)
- **Parent**: Can be anon, shadow, or vnode
- **Base Case**: NO (may recurse to parent)
- **mmo_ops**: `shadow_mmobj_ops`

**3. Vnode Objects** (in `kernel/fs/vnode.c` - PRISTINE)
- **Purpose**: File-backed memory (text/data segments)
- **Parent**: None (backed by disk file)
- **Base Case**: YES (leaf node)
- **mmo_ops**: Neither anon nor shadow (checked via pointer comparison)

### Recursion Base Cases

**Safe Recursion Pattern**:
```c
// In shadow_lookuppage():
if (page found in shadow) {
    return page;  // Base case: page in current shadow
}
// Recurse to parent
return parent->mmo_ops->lookuppage(parent, pagenum);  // May recurse further

// Eventually hits base case:
// - Vnode object: reads from disk
// - Anon object: allocates new page
```

**Infinite Loop Prevention**:
- Max depth: Parent chain should terminate at vnode or anon
- If child forks child forks child... depth increases but must terminate
- Add assertion: `KASSERT(recursion_depth < 100)`

---

## 🔍 FILE VERIFICATION COMMANDS

**Before editing any file:**
```bash
# Check if file is pristine
git status kernel/vm/shadow.c

# Check if file has local changes
git diff kernel/vm/shadow.c

# Verify file is in vm-submit
grep "shadow.c" Makefile
```

**After Phase 3 completion:**
```bash
# Verify only allowed files modified
git status | grep "modified:" | sort
```

---

## 🚨 COMMON PITFALLS

1. **Modifying header files** → Will be replaced during grading
2. **Modifying vnode.c** → Pristine (learned in FIX #47-48)
3. **Infinite recursion in shadow_lookuppage** → Kernel spec warns about this!
4. **C89 violations** → Declare all variables at function start
5. **Forgetting to delete NOT_YET_IMPLEMENTED()** → Will trigger panic

---

## 📝 DEBUGGING INFRASTRUCTURE

### Logging Macros (Already Defined)

Use `DBG_TEMP` for Phase 3 debugging:
```c
dbg(DBG_TEMP, "PHASE3_SHADOW_LOOKUP: obj=%p pagenum=%d parent=%p\n",
    obj, pagenum, obj->mmo_shadowed);
```

### Recursion Depth Tracking

Add to shadow.c:
```c
static int shadow_recursion_depth = 0;

// In recursive functions:
shadow_recursion_depth++;
dbg(DBG_TEMP, "SHADOW_RECURSION_DEPTH=%d\n", shadow_recursion_depth);
KASSERT(shadow_recursion_depth < 50 && "Infinite shadow recursion!");
// ... function body ...
shadow_recursion_depth--;
```

---

**Status**: Phase 3 file matrix complete. Ready for call chain analysis.
