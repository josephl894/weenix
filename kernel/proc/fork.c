/******************************************************************************/
/* Important Fall 2025 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
        vmarea_t *vma, *clone_vma;
        pframe_t *pf;
        mmobj_t *to_delete, *new_shadowed;

        KASSERT(regs != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc->p_state == PROC_RUNNING);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        proc_t *child = proc_create(curproc->p_comm);
        KASSERT(child != NULL);
        KASSERT(child->p_state == PROC_RUNNING);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(child->p_pagedir != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        vmmap_t *clone_map = vmmap_clone(curproc->p_vmmap);
        if (clone_map == NULL) {
        
                dbg(DBG_PRINT, "(GRADING3B 1)\n");
                return -ENOMEM;
        }

       
        vmmap_destroy(child->p_vmmap);

  
        child->p_vmmap = clone_map;
        clone_map->vmm_proc = child;

   
        clone_vma = list_head(&clone_map->vmm_list, vmarea_t, vma_plink);
        list_iterate_begin(&curproc->p_vmmap->vmm_list, vma, vmarea_t, vma_plink) {
                
                KASSERT(clone_vma != NULL);

                if (vma->vma_flags & MAP_PRIVATE) {
                       
                        mmobj_t *parent_shadow = shadow_create();
                        mmobj_t *child_shadow = shadow_create();

                        if (parent_shadow == NULL || child_shadow == NULL) {
                               
                                if (parent_shadow != NULL) {
                                        parent_shadow->mmo_ops->put(parent_shadow);
                                }
                                if (child_shadow != NULL) {
                                        child_shadow->mmo_ops->put(child_shadow);
                                }
                              
                                dbg(DBG_PRINT, "(GRADING3B 1)\n");
                                return -ENOMEM;
                        }

                     
                        mmobj_t *old_obj = vma->vma_obj;


                        old_obj->mmo_ops->ref(old_obj);

                    
                        parent_shadow->mmo_shadowed = old_obj;
                        child_shadow->mmo_shadowed = old_obj;
                        parent_shadow->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(old_obj);
                        child_shadow->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(old_obj);

                      
                        list_remove(&vma->vma_olink);
                        vma->vma_obj = parent_shadow;

 
                        clone_vma->vma_obj = child_shadow;

                      
                        list_insert_tail(mmobj_bottom_vmas(parent_shadow), &vma->vma_olink);
                        list_insert_tail(mmobj_bottom_vmas(child_shadow), &clone_vma->vma_olink);

                        dbg(DBG_PRINT, "(GRADING3B 1)\n");
                } else {
                     
                        clone_vma->vma_obj = vma->vma_obj;
                        vma->vma_obj->mmo_ops->ref(vma->vma_obj);
                        list_insert_tail(&vma->vma_obj->mmo_un.mmo_vmas, &clone_vma->vma_olink);
                        dbg(DBG_PRINT, "(GRADING3B 1)\n");
                }

            
                if (clone_vma->vma_plink.l_next != &clone_map->vmm_list) {
                        clone_vma = list_item(clone_vma->vma_plink.l_next, vmarea_t, vma_plink);
                } else {
                        clone_vma = NULL;
                }

                dbg(DBG_PRINT, "(GRADING3B 1)\n");
        } list_iterate_end();

        
        pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
        tlb_flush_all();

      
        int i;
        for (i = 0; i < NFILES; i++) {
                if (curproc->p_files[i] != NULL) {
                        child->p_files[i] = curproc->p_files[i];
                        fref(child->p_files[i]);
                        dbg(DBG_PRINT, "(GRADING3B 1)\n");
                }
        }

       
        child->p_brk = curproc->p_brk;
        child->p_start_brk = curproc->p_start_brk;

        
        kthread_t *child_thr = kthread_clone(curthr);
        // KASSERT(child_thr != NULL);
        // dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(child_thr->kt_kstack != NULL); 
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

     
        child_thr->kt_proc = child;
        list_insert_tail(&child->p_threads, &child_thr->kt_plink);

       
        regs_t child_regs;
        memcpy(&child_regs, regs, sizeof(regs_t));
        child_regs.r_eax = 0;

      
        child_thr->kt_ctx.c_pdptr = child->p_pagedir;
        child_thr->kt_ctx.c_esp = fork_setup_stack(&child_regs, child_thr->kt_kstack);
        child_thr->kt_ctx.c_eip = (uint32_t)userland_entry;
        child_thr->kt_ctx.c_ebp = child_thr->kt_ctx.c_esp;

        sched_make_runnable(child_thr);

        dbg(DBG_PRINT, "(GRADING3B 1)\n");
        
        return child->p_pid;
}
