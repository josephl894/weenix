# CLAUDE CODE ASSISTANT - WEENIX SAFEGUARDS

**Purpose:** Prevent wasting time on solutions that cannot be implemented due to file modification constraints.

---

## **🚨 CRITICAL RULE: CHECK FILE MODIFICATION RIGHTS FIRST**

**Before proposing ANY solution, agents MUST verify:**

1. ✅ **Is this file in the vm-submit list?**
   - See `ALLOWED-FILES.md` for complete list
   - Only 22 .c files + Config.mk can be modified

2. ❌ **Is this a header file (.h)?**
   - **NO header files can EVER be modified**
   - This includes `anon.h`, `shadow.h`, `vmmap.h`, `pframe.h`, etc.

3. ❌ **Is this a file system file?**
   - `kernel/fs/vnode.c` - PRISTINE
   - `kernel/fs/s5fs/*.c` - ALL PRISTINE
   - `kernel/fs/vn_mmobj_ops.c` - PRISTINE

4. ❌ **Is this a precompiled library?**
   - `kernel/libs5fs.a` - Cannot modify binary
   - `kernel/libdrivers.a` - Cannot modify binary

---

## **AGENT DECISION TREE**

### When Analyzing a Bug:

```
START
  ↓
Does the solution require modifying files?
  ↓
YES → Are ALL modified files in vm-submit list?
        ↓
        YES → ✅ SOLUTION IS LEGAL - Propose it
        ↓
        NO → ❌ SOLUTION IS ILLEGAL
             ↓
             Can we achieve the same goal using ONLY vm-submit files?
               ↓
               YES → ✅ Propose alternative approach
               ↓
               NO → ❌ REJECT this solution path
                    ↓
                    Document why it's illegal
                    Propose different strategy
```

---

## **COMMON MISTAKES TO AVOID**

### ❌ Mistake 1: Modifying Header Files

**Wrong:**
```c
// In kernel/include/vm/anon.h
extern mmobj_ops_t anon_mmobj_ops;  // ❌ ILLEGAL
```

**Correct:**
```c
// In kernel/vm/pagefault.c (allowed file)
extern mmobj_ops_t anon_mmobj_ops;  // ✅ LEGAL
```

### ❌ Mistake 2: Fixing Bugs in Pristine Code

**Wrong:**
```
Problem: s5fs.c has double-pinning bug at line 183
Solution: Remove pframe_pin(vp) from s5fs.c  // ❌ ILLEGAL - pristine file
```

**Correct:**
```
Problem: s5fs.c has double-pinning (pristine, assume correct)
Solution: Our code must work around it using allowed files only  // ✅ LEGAL
```

### ❌ Mistake 3: Adding Unpinning to Vnode Cleanup

**Wrong:**
```
Problem: vnode_flush_all() doesn't unpin pages
Solution: Add pframe_unpin() loop to vnode.c  // ❌ ILLEGAL - pristine file
```

**Correct:**
```
Problem: vnode cleanup doesn't unpin (pristine, assume intentional)
Solution: Ensure our student code unpins vnode pages BEFORE shutdown  // ✅ LEGAL
```

---

## **AGENT RESPONSE TEMPLATE**

When analyzing a proposed fix, agents MUST include:

```markdown
## File Modification Check

**Files to be modified:**
- [ ] kernel/vm/pagefault.c - ✅ In vm-submit (LEGAL)
- [ ] kernel/include/vm/anon.h - ❌ Header file (ILLEGAL)

**Legality Assessment:** ❌ ILLEGAL (modifies header)

**Legal Alternative:**
Move extern declaration to pagefault.c instead of header.

**Reasoning:**
Cannot modify headers per kernel spec line 327. Can achieve same
functionality by adding extern in .c file directly.
```

---

## **VM-SUBMIT FILE LIST (QUICK REFERENCE)**

**VM Student Code (22 files):**
```
✅ kernel/main/kmain.c
✅ kernel/proc/{kmutex,kthread,proc,sched,sched_helper}.c
✅ kernel/fs/{namev,open,vfs_syscall,vn_special}.c
✅ kernel/api/{access,syscall}.c
✅ kernel/mm/pframe.c
✅ kernel/proc/fork.c
✅ kernel/vm/{anon,brk,mmap,pagefault,shadow,vmmap}.c
```

**Everything Else: ❌ PRISTINE**

---

## **DEBUGGING CONSTRAINTS**

### What You CAN Do:

1. ✅ Add `dbg()` calls to vm-submit files
2. ✅ Modify logic in vm-submit files
3. ✅ Add helper functions to vm-submit files (same file only)
4. ✅ Add extern declarations in vm-submit .c files
5. ✅ Instrument/log in vm-submit files

### What You CANNOT Do:

1. ❌ Modify ANY header file
2. ❌ Modify ANY pristine .c file
3. ❌ Add new files to kernel
4. ❌ Modify precompiled libraries (.a files)
5. ❌ Change include paths or Makefile structure
6. ❌ Work around constraints by modifying build system

---

## **AGENT CHECKLIST BEFORE PROPOSING SOLUTION**

- [ ] Listed ALL files that need modification
- [ ] Verified each file is in vm-submit list
- [ ] Confirmed NO header files are modified
- [ ] Confirmed NO pristine .c files are modified
- [ ] If illegal, provided legal alternative
- [ ] If no legal alternative exists, explained clearly and proposed different approach
- [ ] Considered pristine code assumptions (assume perfect, work around it)

---

## **ESCALATION PROTOCOL**

If an agent proposes an illegal solution:

1. **Stop immediately** - Do not implement
2. **Document** why it's illegal (which file, which constraint)
3. **Brainstorm alternatives** using only legal files
4. **If stuck** - Explicitly state "No legal solution found with current approach, need different strategy"
5. **Do NOT** waste user's time implementing illegal solutions

---

## **CURRENT BUG CONTEXT**

**Problem:** Vnode pages pinned at shutdown

**Illegal Approaches Already Tried:**
- ❌ Modify header files (FIX #35-37) - REVERTED
- ❌ Fix s5fs.c double-pinning (Strategy C) - Cannot modify pristine
- ❌ Add unpinning to vnode_flush_all() - Cannot modify vnode.c

**Legal Approaches Still Available:**
- ✅ Add extern to .c files instead of headers
- ✅ Comprehensive logging in pframe.c
- ✅ Search for missing unpin calls in student code
- ✅ Test removing all unpinning to understand pristine behavior

---

## **VERIFICATION COMMAND**

Before implementing ANY fix, run:
```bash
# Check if file is in vm-submit
make vm-submit 2>&1 | grep -q "filename.c" && echo "✅ LEGAL" || echo "❌ ILLEGAL"
```

---

**Last Updated:** 2025-12-06

**Maintained By:** User directive to prevent time-wasting on illegal solutions
