#include "opt-A2.h"
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#if OPT_A2
#include <pid.h>
#endif
#include <syscall.h>
#include <current.h>
#include <proc.h>
#if OPT_A2
#include <mips/trapframe.h>
#endif
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>





#if OPT_A2

int sys_fork(pid_t *retval, struct trapframe *tf){

  struct addrspace *as_child;
  int err;
  struct proc * child = proc_create_runprogram(curproc -> p_name);
  if (child == NULL){
    err = ENOMEM;
    proc_destroy(child);
    return err;
  }

  if (child -> pid -> pid == -1){

    err = ENPROC;
    proc_destroy(child);
    return err;

  }

  err = as_copy(curproc_getas() , &as_child);
  if (err){
    proc_destroy(child);
    as_destroy(as_child);
    return err;
  }


  // do heap copy to ensure the synchro; remenber to free
  struct trapframe *ctf = kmalloc(sizeof(struct trapframe));
  if (ctf == NULL){

    proc_destroy(child);
    as_destroy(as_child);
    err = ENOMEM;
    return err;

  }
  *ctf = *tf;

  err = thread_fork(curthread -> t_name, child, enter_forked_process, ctf, (unsigned long)as_child);
  if (err){

    proc_destroy(child);
    as_destroy(as_child);
    return err;

  }

  *retval = child -> pid -> pid;
  return 0;


}
    
#endif /* OPT_A2 */

/* this implementation of sys__exit does not do anything with the exit code */
/* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {



struct addrspace *as;
struct proc *p = curproc;
/* for now, just include this to keep the compiler from complaining about
   an unused variable */
(void)exitcode;
#if OPT_A2
exit(curproc -> pid, exitcode);
curproc -> pid = NULL;
#endif

DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

KASSERT(curproc->p_addrspace != NULL);
as_deactivate();
/*
 * clear p_addrspace before calling as_destroy. Otherwise if
 * as_destroy sleeps (which is quite possible) when we
 * come back we'll be calling as_activate on a
 * half-destroyed address space. This tends to be
 * messily fatal.
 */
as = curproc_setas(NULL);
as_destroy(as);

/* detach this thread from its process */
/* note: curproc cannot be used after this call */
proc_remthread(curthread);

/* if this is the last user process in the system, proc_destroy()
   will wake up the kernel menu thread */
proc_destroy(p);

thread_exit();
/* thread_exit() does not return, so we should never get here */
panic("return from thread_exit in sys_exit\n");
  

}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{

#if OPT_A2

  *retval = curproc -> pid -> pid;
  return(0);
  
#else

/* for now, this is just a stub that always returns a PID of 1 */
/* you need to fix this to make it work properly */
*retval = 1;
return(0);
  
#endif /* OPT_A2 */

}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{

  int exitstatus;
  int result;


  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  #if OPT_A2

  struct proc_id * childret;
  result = wait(curproc -> pid, pid, childret);
  if (result) {
    return(result);
  }
  int exitcode = childret -> exit_code;
  exitstatus = _MKWAIT_EXIT(exitcode);
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);

  #else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
  #endif

}

