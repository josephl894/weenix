Documentation for Kernel Assignment 1
=====================================

+------------------------+
| BUILD & RUN (Required) |
+------------------------+

Replace "(Comments?)" with the command the grader should use to compile your kernel (should simply be "make").
    To compile the kernel, the grader should type: (Comments?)
If you have additional instruction for the grader, replace "(Comments?)" with your instruction (or with the word "none" if you don't have additional instructions):
    Additional instructions for building/running this assignment: (Comments?)

+-------------------------+
| SELF-GRADING (Required) |
+-------------------------+

Replace each "(Comments?)" with appropriate information and each stand-alone "?"
with a numeric value:

(A.1) In main/kmain.c:
    (a) In bootstrap(): ? out of 3 pts
    (b) In initproc_create(): ? out of 3 pts

(A.2) In proc/proc.c:
    (a) In proc_create(): ? out of 4 pts
    (b) In proc_cleanup(): ? out of 7 pts
    (c) In do_waitpid(): ? out of 6 pts

(A.3) In proc/kthread.c:
    (a) In kthread_create(): ? out of 2 pts
    (b) In kthread_cancel(): ? out of 1 pt
    (c) In kthread_exit(): ? out of 3 pts

(A.4) In proc/sched_helper.c:
    (a) In sched_wakeup_on(): ? out of 1 pt

(A.5) In proc/sched.c:
    (a) In sched_make_runnable(): ? out of 1 pt

(A.6) In proc/kmutex.c:
    (a) In kmutex_lock(): ? out of 1 pt
    (b) In kmutex_lock_cancellable(): ? out of 1 pt
    (c) In kmutex_unlock(): ? out of 2 pts

(B) Kshell : ? out of 20 pts
    What is your kshell command to invoke faber_thread_test(): (Comments?)
    What is your kshell command to invoke sunghan_test(): (Comments?)
    What is your kshell command to invoke sunghan_deadlock_test(): (Comments?)

(C.1) waitpid any test, etc. (? out of 4 pts)
(C.2) Context switch test (? out of 1 pt)
(C.3) wake me test, etc. (? out of 2 pts)
(C.4) wake me uncancellable test, etc. (? out of 2 pts)
(C.5) cancel me test, etc. (? out of 4 pts)
(C.6) reparenting test, etc. (? out of 2 pts)
(C.7) show race test, etc. (? out of 3 pts)
(C.8) kill child procs test (? out of 2 pts)
(C.9) proc kill all test (? out of 2 pts)

(D.1) sunghan_test(): producer/consumer test (? out of 9 pts)
(D.2) sunghan_deadlock_test(): deadlock test (? out of 4 pts)

(E) Additional self-checks: (? out of 10 pts)
    Please provide details, add subsections and/or items as needed; or, say that "none is needed".
    Details: (Comments?)

Missing/incomplete required section(s) in README file (procs-README.txt): (-? pts)
Submitted binary file : (-? pts)
Submitted extra (unmodified) file : (-? pts)
Wrong file location in submission : (-? pts)
Extra printout when running with DBG=error,test in Config.mk : (-? pts)
Incorrectly formatted or mis-labeled "conforming dbg() calls" : (-? pts)
Cannot compile : (-? pts)
Compiler warnings : (-? pts)
"make clean" : (-? pts)
Kernel panic : (-? pts)
Kernel freezes : (-? pts)
Cannot halt kernel cleanly : (-? pts)

+--------------------------------------+
| CONTRIBUTION FROM MEMBERS (Required) |
+--------------------------------------+

1)  Names and USC e-mail addresses of team members: ?
2)  Is the following statement correct about your submission (please replace
        "(Comments?)" with either "yes" or "no", and if the answer is "no",
        please list percentages for each team member)?  "Each team member
        contributed equally in this assignment": (Comments?)

+---------------------------------+
| BUGS / TESTS TO SKIP (Required) |
+---------------------------------+

Are there are any tests mentioned in the grading guidelines test suite that you
know that it's not working and you don't want the grader to run it at all so you
won't get extra deductions, please replace "(Comments?)" below with your list.
(Of course, if the grader won't run such tests in the plus points section, you
will not get plus points for them; if the garder won't run such tests in the
minus points section, you will lose all the points there.)  If there's nothing
the grader should skip, please replace "(Comments?)" with "none".

Please skip the following tests: (Comments?)

+--------------------------------------------------------------------------------------------+
| ADDITIONAL INFORMATION FOR GRADER (Optional, but the grader should read what you add here) |
+--------------------------------------------------------------------------------------------+

+-----------------------------------------------+
| OTHER (Optional) - Not considered for grading |
+-----------------------------------------------+

Comments on design decisions: (Comments?)

