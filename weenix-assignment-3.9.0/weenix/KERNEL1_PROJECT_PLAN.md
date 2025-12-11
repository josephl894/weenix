# WEENIX KERNEL 1 (PROCS) - 10-DAY PROJECT PLAN

**Team Size:** 2 People
**Duration:** 10 Days
**Assignment:** Kernel Assignment 1 - Process and Thread Management

---

## TABLE OF CONTENTS

1. [Complete TODO List](#part-1-complete-todo-list-with-implementation-checklist)
2. [Files and Functions Master List](#part-2-files-and-functions-master-list)
3. [10-Day Timeline with Testing](#part-3-10-day-timeline-with-testing-verification)
4. [Function Assignment Matrix](#part-4-function-assignment-matrix)
5. [Critical Implementation Notes](#critical-implementation-notes)
6. [Testing Summary](#summary-testing-strategy)

---

## PART 1: COMPLETE TODO LIST WITH IMPLEMENTATION CHECKLIST

### Phase 1: Process & Thread Management
- [ ] **proc_create()** - Creates new process, sets up pid, parent-child relationship, initializes lists
- [ ] **proc_cleanup()** - Cleans up dying process, reparents children to init, wakes parent
- [ ] **proc_kill()** - Cancels all threads in a process
- [ ] **proc_kill_all()** - Kills all processes except idle and init
- [ ] **do_waitpid()** - Parent waits for child, reaps zombie, cleans up resources
- [ ] **kthread_create()** - Creates kernel thread, initializes context, adds to process
- [ ] **kthread_cancel()** - Marks thread as cancelled, calls sched_cancel()
- [ ] **kthread_exit()** - Thread self-terminates, switches to new thread

### Phase 2: Scheduling
- [ ] **sched_make_runnable()** - Adds thread to run queue with IPL protection
- [ ] **sched_switch()** - Context switches to next runnable thread, handles IPL correctly
- [ ] **sched_cancellable_sleep_on()** - Puts thread to sleep on wait queue, checks kt_cancelled
- [ ] **sched_cancel()** - Wakes thread from cancellable sleep

### Phase 3: Synchronization
- [ ] **kmutex_init()** - Initializes mutex structure and wait queue
- [ ] **kmutex_lock()** - Acquires mutex or sleeps waiting
- [ ] **kmutex_lock_cancellable()** - Acquires mutex with cancellation support
- [ ] **kmutex_unlock()** - Releases mutex and wakes next waiter

### Phase 4: Integration & Testing
- [ ] **bootstrap()** - Creates idle process and thread, switches context
- [ ] **initproc_create()** - Creates init process (PID 1)
- [ ] **initproc_run()** - Runs init, launches kshell or tests
- [ ] **Kshell commands** - Add faber, sunghan, deadlock commands
- [ ] **Config.mk** - Set DRIVERS=1, CS402TESTS=8
- [ ] **Testing** - Run all C.1-C.9, D.1-D.2 tests
- [ ] **README** - Fill out procs-README.txt

---

## PART 2: FILES AND FUNCTIONS MASTER LIST

### kernel/proc/proc.c (Process Management)

```c
/**
 * Creates a new process with given name, assigns PID, sets up parent-child relationship
 *
 * @param name - Process name for identification
 * @return Pointer to newly created proc_t structure
 *
 * Implementation notes:
 * - Allocate process with slab_obj_alloc(proc_allocator)
 * - Assign PID from next_pid counter (atomic increment)
 * - Set p_pproc to curproc (parent process)
 * - Add to parent's p_children list
 * - Initialize p_threads, p_children lists
 * - Initialize p_wait queue
 * - Add to global proc_list
 * - Check parent's kt_state != KT_EXITED before adding child
 */
proc_t *proc_create(char *name);

/**
 * Cleans up current process: reparents children, wakes parent, prepares for destruction
 *
 * @param status - Exit status to pass to parent
 *
 * Implementation notes:
 * - CRITICAL: Set curthr->kt_state = KT_EXITED FIRST
 * - Reparent all children to init process
 * - Set p_state = PROC_DEAD
 * - Wake parent on p_wait queue
 * - Call sched_switch() (never returns)
 */
void proc_cleanup(int status);

/**
 * Kills all threads in specified process by cancelling them
 *
 * @param p - Process to kill
 * @param status - Exit status for the process
 *
 * Implementation notes:
 * - Set p_status to status
 * - Iterate through p_threads list
 * - Call kthread_cancel() on each thread
 */
void proc_kill(proc_t *p, int status);

/**
 * Kills all processes except idle (PID 0) and init (PID 1)
 *
 * Implementation notes:
 * - Iterate through global proc_list
 * - Skip processes with PID_IDLE and PID_INIT
 * - Call proc_kill() on all others
 */
void proc_kill_all(void);

/**
 * Parent waits for child process to exit, reaps zombie, returns exit status
 *
 * @param pid - PID to wait for (-1 means any child)
 * @param options - Wait options (unused in basic implementation)
 * @param status - Pointer to store child's exit status
 * @return PID of reaped child, or -ECHILD if no children
 *
 * Implementation notes:
 * - Search p_children list for matching child
 * - If no match found, return -ECHILD
 * - If child not PROC_DEAD, sleep on child's p_wait queue
 * - Loop after waking to check again
 * - When child is PROC_DEAD:
 *   - Copy p_status to *status
 *   - Remove from p_children list
 *   - Clean up child proc_t with slab_obj_free()
 *   - Return child's PID
 */
pid_t do_waitpid(pid_t pid, int options, int *status);
```

**Complexity:** High - Critical for process lifecycle
**Dependencies:** Requires kthread functions, sched functions

---

### kernel/proc/kthread.c (Thread Management)

```c
/**
 * Creates kernel thread in given process, initializes context and state
 *
 * @param p - Process to create thread in
 * @param func - Function for thread to execute
 * @param arg1 - First argument to func
 * @param arg2 - Second argument to func
 * @return Pointer to newly created kthread_t structure
 *
 * Implementation notes:
 * - Allocate thread with slab_obj_alloc(kthread_allocator)
 * - Set kt_proc = p
 * - Initialize kt_state = KT_NO_STATE
 * - Allocate kernel stack with alloc_stack()
 * - Initialize context with context_setup()
 * - Add thread to process's p_threads list
 * - Initialize kt_wchan = NULL
 * - Initialize kt_cancelled = 0
 */
kthread_t *kthread_create(proc_t *p, kthread_func_t func, long arg1, void *arg2);

/**
 * Marks thread as cancelled and calls scheduler to handle cancellation
 *
 * @param kthr - Thread to cancel
 * @param retval - Return value to set for thread
 *
 * Implementation notes:
 * - Set kthr->kt_retval = retval
 * - Set kthr->kt_cancelled = 1
 * - Call sched_cancel(kthr)
 */
void kthread_cancel(kthread_t *kthr, void *retval);

/**
 * Current thread exits with return value, never returns
 *
 * @param retval - Return value for thread
 *
 * Implementation notes:
 * - Set curthr->kt_retval = retval
 * - Set curthr->kt_state = KT_EXITED
 * - Wake all threads waiting on curthr
 * - Call sched_switch() - NEVER RETURNS
 */
void kthread_exit(void *retval);
```

**Complexity:** Medium - Thread lifecycle management
**Dependencies:** Requires sched functions

---

### kernel/proc/sched.c (Scheduler)

```c
/**
 * Adds thread to run queue with interrupt protection
 *
 * @param thr - Thread to make runnable
 *
 * Implementation notes:
 * - Save current IPL with intr_getipl()
 * - Set IPL to HIGH to disable interrupts
 * - Set thr->kt_state = KT_RUN
 * - Enqueue thread on kt_runq with ktqueue_enqueue()
 * - Restore original IPL
 */
void sched_make_runnable(kthread_t *thr);

/**
 * Context switches to next runnable thread, MUST save/restore IPL
 *
 * Implementation notes:
 * - CRITICAL: Save old_ipl = intr_getipl() at start
 * - Set IPL to HIGH
 * - Dequeue next thread from kt_runq
 * - If queue empty:
 *   - Set IPL to LOW
 *   - Call intr_wait()
 *   - Set IPL to HIGH
 *   - Retry dequeue
 * - Update curthr and curproc
 * - Call context_switch()
 * - CRITICAL: Restore IPL with intr_setipl(old_ipl)
 */
void sched_switch(void);

/**
 * Puts thread to cancellable sleep on wait queue, checks kt_cancelled flag
 *
 * @param q - Wait queue to sleep on
 * @return 0 if woken normally, -EINTR if cancelled
 *
 * Implementation notes:
 * - Check curthr->kt_cancelled before sleeping
 * - If cancelled, return -EINTR immediately
 * - Set curthr->kt_state = KT_SLEEP_CANCELLABLE
 * - Enqueue on wait queue
 * - Call sched_switch()
 * - After waking, check kt_cancelled again
 * - Return -EINTR if cancelled, 0 otherwise
 */
int sched_cancellable_sleep_on(ktqueue_t *q);

/**
 * Cancels sleeping thread, removes from wait queue if cancellable
 *
 * @param kthr - Thread to cancel
 *
 * Implementation notes:
 * - Set kthr->kt_cancelled = 1
 * - If kthr->kt_state == KT_SLEEP_CANCELLABLE:
 *   - Remove from wait queue with ktqueue_remove()
 *   - Call sched_make_runnable(kthr)
 * - Otherwise, just leave kt_cancelled set
 */
void sched_cancel(struct kthread *kthr);
```

**Complexity:** High - Core scheduling logic, IPL handling critical
**Dependencies:** Standalone, but used by everything

---

### kernel/proc/kmutex.c (Mutex Synchronization)

```c
/**
 * Initializes mutex structure and wait queue
 *
 * @param mtx - Mutex to initialize
 *
 * Implementation notes:
 * - Call sched_queue_init(&mtx->km_waitq)
 * - Set mtx->km_holder = NULL
 */
void kmutex_init(kmutex_t *mtx);

/**
 * Acquires mutex, blocks if held by another thread
 *
 * @param mtx - Mutex to acquire
 *
 * Implementation notes:
 * - While mtx->km_holder != NULL and km_holder != curthr:
 *   - Call sched_sleep_on(&mtx->km_waitq)
 * - Set mtx->km_holder = curthr
 */
void kmutex_lock(kmutex_t *mtx);

/**
 * Acquires mutex with cancellation support, returns -EINTR if cancelled
 *
 * @param mtx - Mutex to acquire
 * @return 0 if acquired, -EINTR if cancelled
 *
 * Implementation notes:
 * - While mtx->km_holder != NULL and km_holder != curthr:
 *   - ret = sched_cancellable_sleep_on(&mtx->km_waitq)
 *   - If ret == -EINTR, return -EINTR
 * - Set mtx->km_holder = curthr
 * - Return 0
 */
int kmutex_lock_cancellable(kmutex_t *mtx);

/**
 * Releases mutex, MUST release before waking next thread
 *
 * @param mtx - Mutex to release
 *
 * Implementation notes:
 * - KASSERT(curthr == mtx->km_holder)
 * - CRITICAL: Set mtx->km_holder = NULL FIRST
 * - Then call sched_wakeup_on(&mtx->km_waitq)
 * - (If you wake first, woken thread sees mutex held and sleeps again)
 */
void kmutex_unlock(kmutex_t *mtx);
```

**Complexity:** Medium - Synchronization primitives
**Dependencies:** Requires sched_cancellable_sleep_on, sched_wakeup_on

---

### kernel/main/kmain.c (Bootstrap & Init)

```c
/**
 * Creates idle process (PID 0), switches to its context
 *
 * @param arg1 - Unused
 * @param arg2 - Unused
 * @return NULL (never returns)
 *
 * Implementation notes:
 * - Call pt_template_init()
 * - Create idle process with proc_create("idle")
 * - KASSERT idle PID == PID_IDLE (0)
 * - Create idle thread with kthread_create(idle_proc, idleproc_run, 0, NULL)
 * - Set curproc = idle_proc
 * - Set curthr = idle_thr
 * - Call context_make_active(&idle_thr->kt_ctx) - NEVER RETURNS
 */
void *bootstrap(int arg1, void *arg2);

/**
 * Creates init process (PID 1) and thread
 *
 * @return Pointer to init thread
 *
 * Implementation notes:
 * - Create init process with proc_create("init")
 * - KASSERT init PID == PID_INIT (1)
 * - Create init thread with kthread_create(init_proc, initproc_run, 0, NULL)
 * - KASSERT init_proc->p_pproc == curproc (parent is idle)
 * - Return init thread
 */
kthread_t *initproc_create(void);

/**
 * Init process main function, launches tests or kshell, waits for all children
 *
 * @param arg1 - Unused
 * @param arg2 - Unused
 * @return NULL
 *
 * Implementation notes:
 * - KASSERT curproc->p_pid == PID_INIT
 * - If DRIVERS:
 *   - Register kshell commands (faber, sunghan, deadlock)
 *   - Create kshell process and thread
 *   - Make kshell runnable
 * - Else:
 *   - Create test process based on CS402INITCHOICE
 *   - Make test thread runnable
 * - CRITICAL: Wait for ALL children with loop:
 *   while ((child_pid = do_waitpid(-1, 0, &status)) != -ECHILD)
 * - Return NULL
 */
void *initproc_run(int arg1, void *arg2);

/**
 * Kshell command handler for faber test
 *
 * Implementation notes:
 * - Create test process
 * - Create test thread with faber_thread_test
 * - Make runnable
 * - Wait for ALL children (including reparented)
 */
static int kshell_faber_test(kshell_t *ksh, int argc, char **argv);

/**
 * Kshell command handler for sunghan test
 */
static int kshell_sunghan_test(kshell_t *ksh, int argc, char **argv);

/**
 * Kshell command handler for deadlock test
 */
static int kshell_sunghan_deadlock_test(kshell_t *ksh, int argc, char **argv);
```

**Complexity:** Low - Mostly integration code
**Dependencies:** Requires all proc, kthread, sched functions

---

## PART 3: 10-DAY TIMELINE WITH TESTING VERIFICATION

### Day 1-2: Foundation Setup & Process Management

#### Person A - Process Creation & Destruction

**Tasks:**
- Read spec sections 1-3 (Processes, Threads, Scheduler)
- Implement `proc_create()` in kernel/proc/proc.c
- Implement `kthread_create()` in kernel/proc/kthread.c

**Testing for Person A (Day 2):**

```c
// Add to initproc_run() temporarily:
void *test_proc_create(int arg1, void *arg2) {
    dbg(DBG_PRINT, "TEST: Created process PID=%d\n", curproc->p_pid);
    dbg(DBG_PRINT, "TEST: Parent PID=%d\n", curproc->p_pproc->p_pid);
    return NULL;
}

// In initproc_run():
proc_t *test_proc = proc_create("test_proc");
dbg(DBG_PRINT, "TEST: test_proc PID=%d, expected=2\n", test_proc->p_pid);
KASSERT(test_proc->p_pid == 2);  // Should be PID 2 (after idle=0, init=1)

kthread_t *test_thr = kthread_create(test_proc, test_proc_create, 0, NULL);
dbg(DBG_PRINT, "TEST: Created thread in process %d\n", test_thr->kt_proc->p_pid);
KASSERT(test_thr->kt_proc == test_proc);
KASSERT(!list_empty(&test_proc->p_threads));
```

**Expected Output:**
```
TEST: test_proc PID=2, expected=2
TEST: Created thread in process 2
```

**Verification Checklist:**
- [ ] PID counter increments correctly
- [ ] Parent-child link established (test_proc->p_pproc == init)
- [ ] Child appears in parent's p_children list
- [ ] Thread appears in process's p_threads list
- [ ] kt_proc points back to process
- [ ] No assertion failures

**Sync Point:** Share implemented functions, verify they compile

---

#### Person B - Bootstrap & Idle Process

**Tasks:**
- Read spec sections 1-3 (Processes, Threads, Scheduler)
- Implement `bootstrap()` in kernel/main/kmain.c
- Implement stub for `idleproc_run()` (just panic for now)
- Implement stub for `initproc_create()` (just panic for now)

**Testing for Person B (Day 2):**

```bash
# In Config.mk, set:
DRIVERS=0
CS402INITCHOICE=2

# Run weenix:
make clean
make
./weenix -n
```

**Expected Output:**
```
(GRADING1A 1.b)  # From bootstrap creating idle
(GRADING1A 1.b)  # From bootstrap setting curproc/curthr
panic(...): reached idleproc_run stub  # Expected for now
```

**Verification Checklist:**
- [ ] Bootstrap creates idle process with PID 0
- [ ] Bootstrap creates idle thread
- [ ] curproc and curthr are set correctly
- [ ] context_make_active() switches to idle thread
- [ ] idleproc_run() is called (even if it's just a stub)
- [ ] No panic before reaching idleproc_run()

**Sync Point:** Share bootstrap code, verify compilation

**End of Day 2 Milestone:** ✅ Can create processes and threads, bootstrap runs

---

### Day 3-4: Scheduler Core

#### Person A - Scheduler Functions

**Tasks:**
- Implement `sched_make_runnable()` in kernel/proc/sched.c
- Implement `sched_switch()` in kernel/proc/sched.c

**Testing for Person A (Day 4):**

```c
// Add to initproc_run() temporarily:
void *test_switch_thread(int arg1, void *arg2) {
    dbg(DBG_PRINT, "TEST: Thread %d running\n", arg1);
    return NULL;
}

// In initproc_run():
proc_t *p1 = proc_create("switch_test");
kthread_t *t1 = kthread_create(p1, test_switch_thread, 1, NULL);
kthread_t *t2 = kthread_create(p1, test_switch_thread, 2, NULL);

dbg(DBG_PRINT, "TEST: Before making runnable\n");
sched_make_runnable(t1);
dbg(DBG_PRINT, "TEST: t1 state=%d, expected KT_RUN=%d\n", t1->kt_state, KT_RUN);
KASSERT(t1->kt_state == KT_RUN);
KASSERT(t1->kt_wchan == &kt_runq);

sched_make_runnable(t2);
dbg(DBG_PRINT, "TEST: About to switch\n");
sched_switch();  // Should switch to t1
dbg(DBG_PRINT, "TEST: Returned from switch\n");
```

**Expected Output:**
```
TEST: Before making runnable
TEST: t1 state=1, expected KT_RUN=1
TEST: About to switch
TEST: Thread 1 running
TEST: Thread 2 running
TEST: Returned from switch
```

**Verification Checklist:**
- [ ] sched_make_runnable() sets kt_state = KT_RUN
- [ ] Thread added to run queue (kt_wchan == &kt_runq)
- [ ] sched_switch() dequeues threads in FIFO order
- [ ] curthr and curproc updated correctly
- [ ] IPL saved and restored (check with dbg output)
- [ ] No infinite loops in sched_switch()

**IPL Testing (CRITICAL):**
```c
// Add this test:
uint8_t ipl_before = intr_getipl();
dbg(DBG_PRINT, "TEST: IPL before switch = %d\n", ipl_before);
sched_switch();
uint8_t ipl_after = intr_getipl();
dbg(DBG_PRINT, "TEST: IPL after switch = %d\n", ipl_after);
KASSERT(ipl_before == ipl_after);  // MUST BE EQUAL
```

---

#### Person B - Thread Lifecycle

**Tasks:**
- Implement `kthread_exit()` in kernel/proc/kthread.c
- Implement `idleproc_run()` in kernel/main/kmain.c
- Implement `initproc_create()` in kernel/main/kmain.c
- Implement stub `initproc_run()` (just dbg output for now)

**Testing for Person B (Day 4):**

```c
// Stub initproc_run() for testing:
void *initproc_run(int arg1, void *arg2) {
    dbg(DBG_PRINT, "TEST: Init process running, PID=%d\n", curproc->p_pid);
    KASSERT(curproc->p_pid == PID_INIT);

    // Test kthread_exit
    dbg(DBG_PRINT, "TEST: Init exiting\n");
    kthread_exit(NULL);

    panic("Should never reach here after kthread_exit");
    return NULL;
}
```

**Expected Output:**
```
(GRADING1A 1.c)  # From initproc_create
TEST: Init process running, PID=1
TEST: Init exiting
[System should hang or panic in idleproc_run waitpid - expected for now]
```

**Verification Checklist:**
- [ ] initproc_create() creates process with PID 1
- [ ] Init's parent is idle (p_pproc == idle_proc)
- [ ] Init appears in idle's p_children list
- [ ] kthread_exit() sets kt_state = KT_EXITED
- [ ] kthread_exit() calls sched_switch() (never returns)
- [ ] Interrupts enabled before running init

**End of Day 4 Milestone:** ✅ Basic scheduler works, can switch contexts

---

### Day 5-6: Process Cleanup & Waitpid

#### Person A - Process Cleanup

**Tasks:**
- Implement `proc_cleanup()` in kernel/proc/proc.c
- Modify `proc_create()` to check parent's kt_state

**Testing for Person A (Day 6):**

```c
// Test cleanup and reparenting:
void *test_child_thread(int arg1, void *arg2) {
    dbg(DBG_PRINT, "TEST: Child %d running\n", arg1);
    proc_cleanup(arg1);  // Exit with status = arg1
    panic("Should never reach here");
    return NULL;
}

void *test_parent_thread(int arg1, void *arg2) {
    dbg(DBG_PRINT, "TEST: Parent creating children\n");

    proc_t *c1 = proc_create("child1");
    kthread_t *t1 = kthread_create(c1, test_child_thread, 10, NULL);

    proc_t *c2 = proc_create("child2");
    kthread_t *t2 = kthread_create(c2, test_child_thread, 20, NULL);

    dbg(DBG_PRINT, "TEST: Parent has %d children\n",
        list_length(&curproc->p_children));

    sched_make_runnable(t1);
    sched_make_runnable(t2);

    dbg(DBG_PRINT, "TEST: Parent exiting\n");
    proc_cleanup(0);
    panic("Should never reach here");
    return NULL;
}

// In initproc_run():
proc_t *parent = proc_create("test_parent");
kthread_t *parent_thr = kthread_create(parent, test_parent_thread, 0, NULL);
sched_make_runnable(parent_thr);

// Let everything run
sched_switch();

// Check reparenting
dbg(DBG_PRINT, "TEST: Init now has %d children\n",
    list_length(&curproc->p_children));
// Should show orphaned processes reparented to init
```

**Expected Output:**
```
TEST: Parent creating children
TEST: Parent has 2 children
TEST: Parent exiting
TEST: Child 10 running
TEST: Child 20 running
TEST: Init now has 3 children  # Original parent + 2 reparented children
```

**Verification Checklist:**
- [ ] kt_state set to KT_EXITED before reparenting
- [ ] No infinite loop in reparenting (counter test removed)
- [ ] Children correctly moved to init's p_children list
- [ ] p_state set to PROC_DEAD
- [ ] Parent woken if waiting
- [ ] proc_cleanup() never returns

**Reparenting Edge Case Test:**
```c
// Test that no new children added during cleanup:
int safety = 0;
while (!list_empty(&curproc->p_children)) {
    if (++safety > 100) {
        panic("Reparenting loop - did you set kt_state first?");
    }
    // ... reparenting code ...
}
```

---

#### Person B - Waitpid Implementation

**Tasks:**
- Implement `do_waitpid()` in kernel/proc/proc.c
- Update `initproc_run()` to wait for all children

**Testing for Person B (Day 6):**

```c
// Test waitpid:
void *test_simple_child(int arg1, void *arg2) {
    dbg(DBG_PRINT, "TEST: Simple child PID=%d exiting with status %d\n",
        curproc->p_pid, arg1);
    proc_cleanup(arg1);
    return NULL;
}

void *test_waitpid_parent(int arg1, void *arg2) {
    int status;
    pid_t child_pid;

    proc_t *child = proc_create("waitpid_child");
    kthread_t *child_thr = kthread_create(child, test_simple_child, 42, NULL);

    dbg(DBG_PRINT, "TEST: Parent created child PID=%d\n", child->p_pid);

    sched_make_runnable(child_thr);

    // Wait for specific child
    dbg(DBG_PRINT, "TEST: Parent waiting for child\n");
    child_pid = do_waitpid(child->p_pid, 0, &status);

    dbg(DBG_PRINT, "TEST: Parent reaped PID=%d with status=%d\n",
        child_pid, status);
    KASSERT(child_pid == child->p_pid);
    KASSERT(status == 42);

    // Try waiting again - should get -ECHILD
    child_pid = do_waitpid(-1, 0, &status);
    dbg(DBG_PRINT, "TEST: Second waitpid returned %d (expected -ECHILD=%d)\n",
        child_pid, -ECHILD);
    KASSERT(child_pid == -ECHILD);

    proc_cleanup(0);
    return NULL;
}

// In initproc_run():
proc_t *parent = proc_create("waitpid_parent");
kthread_t *parent_thr = kthread_create(parent, test_waitpid_parent, 0, NULL);
sched_make_runnable(parent_thr);

// Wait for all
while ((child_pid = do_waitpid(-1, 0, &status)) != -ECHILD) {
    dbg(DBG_PRINT, "TEST: Init reaped PID=%d\n", child_pid);
}
dbg(DBG_PRINT, "TEST: Init has no more children\n");
```

**Expected Output:**
```
TEST: Parent created child PID=3
TEST: Parent waiting for child
TEST: Simple child PID=3 exiting with status 42
TEST: Parent reaped PID=3 with status=42
TEST: Second waitpid returned -10 (expected -ECHILD=-10)
TEST: Init reaped PID=2
TEST: Init has no more children
```

**Verification Checklist:**
- [ ] Waitpid finds matching child by PID
- [ ] Waitpid with -1 finds any child
- [ ] Parent sleeps if child not dead yet
- [ ] Parent wakes when child dies
- [ ] Exit status copied correctly
- [ ] Child removed from p_children list
- [ ] proc_t cleaned up (returned to slab)
- [ ] Returns -ECHILD when no children

**Edge Case Tests:**
```c
// Test waiting for non-existent child:
child_pid = do_waitpid(9999, 0, &status);
KASSERT(child_pid == -ECHILD);

// Test waiting for already-reaped child:
child_pid = do_waitpid(already_reaped_pid, 0, &status);
KASSERT(child_pid == -ECHILD);
```

**Sync Point:** Both review cleanup and waitpid logic together

**End of Day 6 Milestone:** ✅ Processes can exit cleanly, parents can reap children

---

### Day 7: Cancellation & Kill Functions

#### Person A - Cancellable Sleep

**Tasks:**
- Implement `sched_cancellable_sleep_on()` in kernel/proc/sched.c
- Implement `sched_cancel()` in kernel/proc/sched.c

**Testing for Person A (Day 7):**

```c
ktqueue_t test_queue;
sched_queue_init(&test_queue);

void *test_cancellable_thread(int arg1, void *arg2) {
    int ret;

    dbg(DBG_PRINT, "TEST: Thread sleeping cancellably\n");
    ret = sched_cancellable_sleep_on(&test_queue);

    dbg(DBG_PRINT, "TEST: Thread woke, ret=%d (expected -EINTR=%d)\n",
        ret, -EINTR);
    KASSERT(ret == -EINTR);
    KASSERT(curthr->kt_cancelled == 1);

    proc_cleanup(0);
    return NULL;
}

void *test_cancel_thread(int arg1, void *arg2) {
    kthread_t *victim = (kthread_t *)arg2;

    dbg(DBG_PRINT, "TEST: Cancelling thread\n");

    // Give victim time to sleep
    sched_make_runnable(curthr);
    sched_switch();

    sched_cancel(victim);
    dbg(DBG_PRINT, "TEST: Cancelled, victim should be runnable now\n");
    KASSERT(victim->kt_state == KT_RUN);

    proc_cleanup(0);
    return NULL;
}

// In initproc_run():
proc_t *p1 = proc_create("victim");
kthread_t *victim = kthread_create(p1, test_cancellable_thread, 0, NULL);

proc_t *p2 = proc_create("canceller");
kthread_t *canceller = kthread_create(p2, test_cancel_thread, 0, victim);

sched_make_runnable(victim);
sched_make_runnable(canceller);

// Wait for both
int status;
do_waitpid(-1, 0, &status);
do_waitpid(-1, 0, &status);
```

**Expected Output:**
```
TEST: Thread sleeping cancellably
TEST: Cancelling thread
TEST: Cancelled, victim should be runnable now
TEST: Thread woke, ret=-4 (expected -EINTR=-4)
```

**Verification Checklist:**
- [ ] Thread sleeps on queue with state KT_SLEEP_CANCELLABLE
- [ ] kt_cancelled checked before sleeping
- [ ] sched_cancel() sets kt_cancelled = 1
- [ ] Thread removed from wait queue
- [ ] Thread made runnable
- [ ] Thread returns -EINTR after waking
- [ ] Non-cancellable sleep ignores cancel (kt_cancelled set but thread stays asleep)

**Test Pre-Cancelled:**
```c
curthr->kt_cancelled = 1;
int ret = sched_cancellable_sleep_on(&test_queue);
KASSERT(ret == -EINTR);  // Should return immediately without sleeping
```

---

#### Person B - Kill Functions

**Tasks:**
- Implement `kthread_cancel()` in kernel/proc/kthread.c
- Implement `proc_kill()` in kernel/proc/proc.c
- Implement `proc_kill_all()` in kernel/proc/proc.c

**Testing for Person B (Day 7):**

```c
void *test_kill_victim(int arg1, void *arg2) {
    dbg(DBG_PRINT, "TEST: Kill victim running\n");

    // Sleep forever (should be cancelled)
    while (1) {
        sched_make_runnable(curthr);
        sched_switch();
    }

    return NULL;
}

void *test_proc_kill_parent(int arg1, void *arg2) {
    proc_t *victim = proc_create("kill_victim");
    kthread_t *t1 = kthread_create(victim, test_kill_victim, 0, NULL);
    kthread_t *t2 = kthread_create(victim, test_kill_victim, 0, NULL);

    dbg(DBG_PRINT, "TEST: Created victim with 2 threads\n");

    sched_make_runnable(t1);
    sched_make_runnable(t2);

    // Let them run
    sched_make_runnable(curthr);
    sched_switch();

    dbg(DBG_PRINT, "TEST: Killing victim process\n");
    proc_kill(victim, 99);

    dbg(DBG_PRINT, "TEST: Victim killed, status=%d\n", victim->p_status);
    KASSERT(victim->p_status == 99);
    KASSERT(t1->kt_cancelled == 1);
    KASSERT(t2->kt_cancelled == 1);

    // Wait for victim to finish
    int status;
    do_waitpid(victim->p_pid, 0, &status);
    dbg(DBG_PRINT, "TEST: Reaped victim with status=%d\n", status);

    proc_cleanup(0);
    return NULL;
}

// Test proc_kill_all:
void *test_kill_all(int arg1, void *arg2) {
    // Create several processes
    proc_t *p1 = proc_create("killall1");
    kthread_t *t1 = kthread_create(p1, test_kill_victim, 0, NULL);
    sched_make_runnable(t1);

    proc_t *p2 = proc_create("killall2");
    kthread_t *t2 = kthread_create(p2, test_kill_victim, 0, NULL);
    sched_make_runnable(t2);

    dbg(DBG_PRINT, "TEST: Calling proc_kill_all\n");
    proc_kill_all();

    dbg(DBG_PRINT, "TEST: All killed except idle and init\n");

    // Wait for all
    int status;
    while (do_waitpid(-1, 0, &status) != -ECHILD) {
        dbg(DBG_PRINT, "TEST: Reaped a child\n");
    }

    proc_cleanup(0);
    return NULL;
}
```

**Expected Output:**
```
TEST: Created victim with 2 threads
TEST: Kill victim running
TEST: Kill victim running
TEST: Killing victim process
TEST: Victim killed, status=99
TEST: Reaped victim with status=99

TEST: Calling proc_kill_all
TEST: All killed except idle and init
TEST: Reaped a child
TEST: Reaped a child
```

**Verification Checklist:**
- [ ] kthread_cancel() sets kt_retval
- [ ] kthread_cancel() sets kt_cancelled
- [ ] kthread_cancel() calls sched_cancel()
- [ ] proc_kill() sets p_status
- [ ] proc_kill() cancels all threads in p_threads list
- [ ] proc_kill_all() skips PID_IDLE (0) and PID_INIT (1)
- [ ] proc_kill_all() kills all other processes

**End of Day 7 Milestone:** ✅ Cancellation works, can kill processes

---

### Day 8: Mutex Synchronization

#### Person A & B - Mutex Implementation (Work Together)

**Tasks:**
- Implement `kmutex_init()` in kernel/proc/kmutex.c
- Implement `kmutex_lock()` in kernel/proc/kmutex.c
- Implement `kmutex_unlock()` in kernel/proc/kmutex.c
- Implement `kmutex_lock_cancellable()` in kernel/proc/kmutex.c

**Testing for Day 8:**

```c
kmutex_t test_mutex;
int shared_counter = 0;

void *test_mutex_thread(int arg1, void *arg2) {
    int iterations = 100;
    int i;

    for (i = 0; i < iterations; i++) {
        kmutex_lock(&test_mutex);

        int local = shared_counter;
        // Yield to force race condition if mutex broken
        sched_make_runnable(curthr);
        sched_switch();
        shared_counter = local + 1;

        kmutex_unlock(&test_mutex);

        // Yield between locks
        sched_make_runnable(curthr);
        sched_switch();
    }

    dbg(DBG_PRINT, "TEST: Thread %d finished\n", arg1);
    proc_cleanup(0);
    return NULL;
}

void *test_mutex_parent(int arg1, void *arg2) {
    kmutex_init(&test_mutex);

    proc_t *p1 = proc_create("mutex1");
    kthread_t *t1 = kthread_create(p1, test_mutex_thread, 1, NULL);

    proc_t *p2 = proc_create("mutex2");
    kthread_t *t2 = kthread_create(p2, test_mutex_thread, 2, NULL);

    proc_t *p3 = proc_create("mutex3");
    kthread_t *t3 = kthread_create(p3, test_mutex_thread, 3, NULL);

    sched_make_runnable(t1);
    sched_make_runnable(t2);
    sched_make_runnable(t3);

    // Wait for all
    int status;
    do_waitpid(-1, 0, &status);
    do_waitpid(-1, 0, &status);
    do_waitpid(-1, 0, &status);

    dbg(DBG_PRINT, "TEST: Expected counter=300, actual=%d\n", shared_counter);
    KASSERT(shared_counter == 300);

    proc_cleanup(0);
    return NULL;
}
```

**Expected Output:**
```
TEST: Thread 1 finished
TEST: Thread 2 finished
TEST: Thread 3 finished
TEST: Expected counter=300, actual=300
```

**If Broken (unlock wakes before releasing):**
```
TEST: Expected counter=300, actual=100  # Lost updates due to race
```

**Verification Checklist:**
- [ ] kmutex_init() initializes queue and holder
- [ ] kmutex_lock() acquires if available
- [ ] kmutex_lock() sleeps if held
- [ ] kmutex_unlock() releases FIRST, then wakes
- [ ] Multiple threads correctly serialize access
- [ ] No lost updates in counter test

**Test Cancellable Mutex:**

```c
void *test_cancel_mutex_victim(int arg1, void *arg2) {
    kmutex_t *mtx = (kmutex_t *)arg2;
    int ret;

    dbg(DBG_PRINT, "TEST: Trying to lock (will block)\n");
    ret = kmutex_lock_cancellable(mtx);

    dbg(DBG_PRINT, "TEST: Lock returned %d (expected -EINTR=%d)\n",
        ret, -EINTR);
    KASSERT(ret == -EINTR);

    proc_cleanup(0);
    return NULL;
}

void *test_cancel_mutex_holder(int arg1, void *arg2) {
    kmutex_t *mtx = (kmutex_t *)arg2;

    kmutex_lock(mtx);
    dbg(DBG_PRINT, "TEST: Holder has lock\n");

    // Start victim
    proc_t *victim_proc = proc_create("cancel_victim");
    kthread_t *victim = kthread_create(victim_proc,
                                       test_cancel_mutex_victim, 0, mtx);
    sched_make_runnable(victim);

    // Let victim block on mutex
    sched_make_runnable(curthr);
    sched_switch();

    // Cancel victim while blocked
    dbg(DBG_PRINT, "TEST: Cancelling victim\n");
    kthread_cancel(victim, NULL);

    // Release lock
    kmutex_unlock(mtx);

    // Wait for victim
    int status;
    do_waitpid(-1, 0, &status);

    proc_cleanup(0);
    return NULL;
}
```

**Expected Output:**
```
TEST: Holder has lock
TEST: Trying to lock (will block)
TEST: Cancelling victim
TEST: Lock returned -4 (expected -EINTR=-4)
```

**Sync Point:** Both review mutex implementation together

**End of Day 8 Milestone:** ✅ All mutex functions work, no race conditions

---

### Day 9: Integration & Full Test Suite

#### Both Together - Kshell Integration & Testing

**Tasks:**
- Set DRIVERS=1 in Config.mk
- Set CS402TESTS=8 (excludes C.9 from faber command)
- Add kshell commands to initproc_run()
- Implement command handlers
- Run all tests

---

#### Test 1: Run Faber Test (C.1-C.8)

```bash
# Set in Config.mk:
DRIVERS=1
CS402TESTS=8
DBG=error,test,print

make clean
make
./weenix -n
```

**In kshell, type:**
```
faber
```

**Expected Output:**
```
kshell$ faber
Running faber_thread_test...
Test C.1: ... PASSED
Test C.2: ... PASSED
Test C.3: ... PASSED
Test C.4: ... PASSED
Test C.5: ... PASSED
Test C.6: ... PASSED
Test C.7: ... PASSED
Test C.8: ... PASSED
kshell$
```

**Verification Checklist:**
- [ ] Kshell prompt returns (doesn't halt)
- [ ] All tests show PASSED
- [ ] No assertion failures
- [ ] No panics
- [ ] Can run command multiple times

---

#### Test 2: Run C.9 Separately

```bash
# Set in Config.mk:
DRIVERS=0
CS402TESTS=10
CS402INITCHOICE=0

make clean
make
./weenix -n
```

**Expected Output:**
```
Test C.1: ... PASSED
...
Test C.9: ... PASSED
Weenix halted cleanly!
It is now safe to turn off your computer.
```

**Verification Checklist:**
- [ ] C.9 runs and passes
- [ ] System halts cleanly (expected - proc_kill_all kills init)
- [ ] Message "Weenix halted cleanly!" appears

---

#### Test 3: Run Sunghan Test (D.1)

```bash
# Set in Config.mk:
DRIVERS=1
CS402TESTS=8

./weenix -n
```

**In kshell, type:**
```
sunghan
```

**Expected Output:**
```
kshell$ sunghan
Running sunghan_test...
Sunghan's test successfully completed!
Final value of 'value' is 920
Test D.1: ... PASSED
kshell$
```

**Verification Checklist:**
- [ ] Test completes and passes
- [ ] No deadlock
- [ ] Kshell prompt returns
- [ ] Can run multiple times

---

#### Test 4: Run Deadlock Test (D.2)

**In kshell:**
```
deadlock
```

**Expected Output:**
```
kshell$ deadlock
Running sunghan_deadlock_test...
[May see some test output]
Test D.2: ... PASSED
kshell$
```

**Verification Checklist:**
- [ ] Test detects would-be deadlock correctly
- [ ] Test passes
- [ ] No actual deadlock occurs
- [ ] Kshell prompt returns

---

#### Test 5: Stress Test - Run All Tests Multiple Times

```bash
# In kshell:
faber
sunghan
deadlock
faber
sunghan
deadlock
```

**Verification Checklist:**
- [ ] All tests pass each time
- [ ] No memory leaks (system doesn't slow down)
- [ ] No hanging
- [ ] Kshell responsive between tests

---

#### Test 6: All KASSERT Statements Hit

```bash
# Enable all debug output:
DBG=all

./weenix -n
```

**Verification Checklist:**
- [ ] "(GRADING1A 1.a)" appears (proc_create)
- [ ] "(GRADING1A 1.b)" appears (bootstrap)
- [ ] "(GRADING1A 1.c)" appears (initproc_create)
- [ ] "(GRADING1A 2.a)" appears (kthread_create)
- [ ] "(GRADING1A 3.a)" through "(GRADING1A 3.d)" appear (do_waitpid)
- [ ] "(GRADING1A 4.a)" through "(GRADING1A 4.c)" appear (kthread_exit/proc_cleanup)
- [ ] "(GRADING1A 5.a)" appears multiple times (scheduler)
- [ ] "(GRADING1A 6.a)" through "(GRADING1A 6.d)" appear (mutex)

**Use grep to verify:**
```bash
./weenix -n 2>&1 | grep "GRADING1A 1.a"
./weenix -n 2>&1 | grep "GRADING1A 2.a"
# etc.
```

**End of Day 9 Milestone:** ✅ All tests passing, kshell working

---

### Day 10: Documentation & Final Verification

#### Person A - README Documentation

**Tasks:**
- Fill out procs-README.txt

**README Sections to Complete:**

1. **SELF-checks:** Mark Y for each SELF-check function you implemented
2. **A.1-A.11:** Mark Y for functions implemented, N for not implemented
3. **Section B:** Describe your implementation approach
   - High-level overview of how you structured process/thread management
   - Key design decisions
   - Any deviations from suggested approach
4. **Section C:** Explain test results
   - Which tests passed
   - Any interesting observations
   - How you verified correctness
5. **Section D:** Explain mutex test results
   - Sunghan test results
   - Deadlock test results
6. **Anything worth mentioning:** Any challenges, bugs found, interesting insights
7. **Extra credit:** If applicable

---

#### Person B - Final Verification

**Tasks:**
- Run complete test suite 3 times
- Verify no debugging code left
- Check all KASSERT statements per spec

**Test 7: Clean Build Test**

```bash
make clean
make
./weenix -n
# Run all tests
faber
sunghan
deadlock
```

**Verification Checklist:**
- [ ] No compiler warnings
- [ ] No "NOT_YET_IMPLEMENTED" left in code
- [ ] Clean build from scratch works
- [ ] All tests still pass

---

**Test 8: Edge Cases**

```c
// Add these tests temporarily to initproc_run():

// Test 1: Waitpid on non-existent PID
int status;
pid_t ret = do_waitpid(9999, 0, &status);
KASSERT(ret == -ECHILD);

// Test 2: Kill already-dead process (should be safe no-op)
proc_t *dead = proc_create("already_dead");
kthread_t *t = kthread_create(dead, some_func, 0, NULL);
sched_make_runnable(t);
// Wait for it to die
do_waitpid(dead->p_pid, 0, &status);
// Try to kill again - should not crash
proc_kill(dead, 99);

// Test 3: Create many processes (stress test)
for (int i = 0; i < 50; i++) {
    proc_t *p = proc_create("stress");
    KASSERT(p != NULL);
    kthread_t *t = kthread_create(p, simple_exit, 0, NULL);
    sched_make_runnable(t);
}
// Reap all
while (do_waitpid(-1, 0, &status) != -ECHILD);
```

**Verification Checklist:**
- [ ] Edge cases don't crash
- [ ] Proper error codes returned
- [ ] No memory leaks
- [ ] Can handle 50+ processes

---

**Test 9: README Verification**

Open `procs-README.txt` and verify each section:

**Verification Checklist:**
- [ ] All SELF-check functions marked Y
- [ ] A.1-A.11 accurately filled (Y for implemented, N for not)
- [ ] Section B explains implementation approach
- [ ] Section C explains test results
- [ ] Section D explains mutex test results
- [ ] Anything worth mentioning section filled
- [ ] Extra credit section filled (if applicable)

---

#### Both Together - Code Review

**Tasks:**
- Review all implemented functions
- Check for spec compliance
- Verify no edge cases missed
- Ensure proper error handling
- Confirm IPL handling correct in all paths

**Code Review Checklist:**

**kernel/proc/proc.c:**
- [ ] proc_create() checks parent kt_state before adding child
- [ ] proc_cleanup() sets kt_state FIRST before reparenting
- [ ] do_waitpid() loops on sleep, handles -1 PID correctly
- [ ] proc_kill() cancels all threads
- [ ] proc_kill_all() skips idle and init

**kernel/proc/kthread.c:**
- [ ] kthread_create() initializes all fields correctly
- [ ] kthread_cancel() sets retval and cancelled flag
- [ ] kthread_exit() never returns

**kernel/proc/sched.c:**
- [ ] sched_make_runnable() has IPL protection
- [ ] sched_switch() saves/restores IPL correctly
- [ ] sched_cancellable_sleep_on() checks cancelled before/after
- [ ] sched_cancel() only removes if KT_SLEEP_CANCELLABLE

**kernel/proc/kmutex.c:**
- [ ] kmutex_unlock() releases BEFORE waking
- [ ] kmutex_lock_cancellable() returns -EINTR on cancel
- [ ] All mutex functions have proper KASSERT statements

**kernel/main/kmain.c:**
- [ ] bootstrap() never returns
- [ ] initproc_run() loops on waitpid(-1)
- [ ] Kshell commands wait for ALL children

---

**Test 10: Final Test Run (Clean Slate)**

```bash
# Reset everything
make clean
rm -rf user/disk0.img

# In Config.mk, set final configuration:
DRIVERS=1
CS402TESTS=8
DBG=error,test,print

# Build and run
make
./weenix -n

# Run all tests in kshell:
faber
sunghan
deadlock

# Shutdown cleanly
<Ctrl+C in qemu window or close terminal>
```

**Final Verification Checklist:**
- [ ] ✅ Clean build succeeds
- [ ] ✅ Kshell launches
- [ ] ✅ Faber test (C.1-C.8) passes
- [ ] ✅ Sunghan test (D.1) passes
- [ ] ✅ Deadlock test (D.2) passes
- [ ] ✅ No panics
- [ ] ✅ No assertion failures
- [ ] ✅ Kshell responsive between tests
- [ ] ✅ README complete
- [ ] ✅ No TODO/FIXME comments left
- [ ] ✅ Code compiles with no warnings
- [ ] ✅ All GRADING1A dbg statements present

**End of Day 10:** ✅ Complete, tested, documented implementation ready for submission

---

## PART 4: FUNCTION ASSIGNMENT MATRIX

| File | Function | Person | Day | Critical Notes |
|------|----------|--------|-----|----------------|
| kernel/proc/proc.c | proc_create() | A | 1-2 | Parent-child setup, check kt_state |
| kernel/proc/proc.c | proc_cleanup() | A | 5-6 | Set kt_state FIRST, then reparent |
| kernel/proc/proc.c | do_waitpid() | B | 5-6 | Loop on sleep, handle -1 pid |
| kernel/proc/proc.c | proc_kill() | B | 7 | Cancel all threads |
| kernel/proc/proc.c | proc_kill_all() | B | 7 | Skip idle and init |
| kernel/proc/kthread.c | kthread_create() | A | 1-2 | Context setup, add to p_threads |
| kernel/proc/kthread.c | kthread_cancel() | B | 7 | Set retval, call sched_cancel |
| kernel/proc/kthread.c | kthread_exit() | B | 3-4 | Set EXITED, wake waiters, switch |
| kernel/proc/sched.c | sched_make_runnable() | A | 3-4 | IPL protection |
| kernel/proc/sched.c | sched_switch() | A | 3-4 | **SAVE/RESTORE IPL** |
| kernel/proc/sched.c | sched_cancellable_sleep_on() | A | 7 | Check cancelled before/after |
| kernel/proc/sched.c | sched_cancel() | A | 7 | Remove if cancellable |
| kernel/proc/kmutex.c | kmutex_init() | A | 8 | Initialize queue and holder |
| kernel/proc/kmutex.c | kmutex_lock() | A | 8 | Sleep while held |
| kernel/proc/kmutex.c | kmutex_unlock() | B | 8 | **RELEASE FIRST, THEN WAKE** |
| kernel/proc/kmutex.c | kmutex_lock_cancellable() | B | 8 | Check return value |
| kernel/main/kmain.c | bootstrap() | B | 1-2 | Create idle, switch context |
| kernel/main/kmain.c | initproc_create() | B | 3-4 | Create init (PID 1) |
| kernel/main/kmain.c | initproc_run() | B | 5-6 | **Loop on waitpid** |
| kernel/main/kmain.c | kshell_*_test() | Both | 9 | Create proc, thread, waitpid loop |

---

## CRITICAL IMPLEMENTATION NOTES

### 1. IPL Handling in sched_switch() (MUST GET RIGHT)

```c
void sched_switch(void) {
    kthread_t *next_thr, *old_thr = curthr;
    uint8_t old_ipl;

    old_ipl = intr_getipl();  // SAVE at start
    intr_setipl(IPL_HIGH);

    // ... dequeue logic ...

    context_switch(&old_thr->kt_ctx, &next_thr->kt_ctx);

    intr_setipl(old_ipl);  // RESTORE after switch - THIS IS CRITICAL
}
```

**Why:** When kshell performs I/O, it expects IPL to be at INTR_KEYBOARD level. If sched_switch() doesn't restore it, driver panics.

---

### 2. Proc Cleanup Reparenting (MUST GET RIGHT)

```c
void proc_cleanup(int status) {
    curproc->p_status = status;
    curthr->kt_state = KT_EXITED;  // SET THIS FIRST!!!

    // Now reparent - no new children can be added during this loop
    while (!list_empty(&curproc->p_children)) {
        child = list_head(&curproc->p_children, proc_t, p_child_link);
        list_remove(&child->p_child_link);
        child->p_pproc = proc_initproc;
        list_insert_tail(&proc_initproc->p_children, &child->p_child_link);
    }
    // ... rest of cleanup ...
}
```

**Why:** If kt_state not set first, new children can be added during reparenting loop (infinite loop).

---

### 3. Mutex Unlock Order (MUST GET RIGHT)

```c
void kmutex_unlock(kmutex_t *mtx) {
    KASSERT(curthr == mtx->km_holder);

    mtx->km_holder = NULL;  // RELEASE FIRST
    sched_wakeup_on(&mtx->km_waitq);  // THEN WAKE
}
```

**Why:** If you wake first, woken thread checks km_holder, sees it's still held, goes back to sleep (deadlock).

---

### 4. Init Must Wait for ALL Children

```c
void *initproc_run(int arg1, void *arg2) {
    // ... launch kshell or tests ...

    // MUST loop - not just single waitpid
    while ((child_pid = do_waitpid(-1, 0, &status)) != -ECHILD) {
        dbg(DBG_PRINT, "(GRADING1A)\n");
    }

    return NULL;
}
```

**Why:** Init becomes parent of orphaned processes. Must wait for all, not just direct children.

---

## SUMMARY: TESTING STRATEGY

| Day | Testing Focus | Key Tests |
|-----|---------------|-----------|
| 1-2 | Process/Thread Creation | PID assignment, parent-child links, list membership |
| 3-4 | Scheduler | Context switch, IPL save/restore, run queue FIFO |
| 5-6 | Cleanup & Waitpid | Reparenting, status codes, zombie reaping |
| 7 | Cancellation | Return -EINTR, thread removal from queue |
| 8 | Mutex | Counter test (no lost updates), cancellable lock |
| 9 | Integration | All C.1-C.9, D.1-D.2 via kshell |
| 10 | Final | Clean build, edge cases, README, all grading statements |

---

## QUICK REFERENCE: Config.mk Settings

**For Kshell Testing (C.1-C.8, D.1-D.2):**
```makefile
DRIVERS=1
CS402TESTS=8
DBG=error,test,print
```

**For C.9 Testing:**
```makefile
DRIVERS=0
CS402TESTS=10
CS402INITCHOICE=0
DBG=error,test,print
```

**For Full Debug Output:**
```makefile
DBG=all
```

---

## PROJECT SUCCESS CRITERIA

✅ All functions implemented per spec
✅ All C.1-C.9 tests pass
✅ All D.1-D.2 tests pass
✅ Kshell responsive and stable
✅ No assertion failures
✅ No panics
✅ No memory leaks
✅ IPL handled correctly
✅ All GRADING1A statements hit
✅ README.txt complete
✅ Code compiles with no warnings
✅ Clean build from scratch works

---

**END OF PROJECT PLAN**
