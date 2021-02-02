#include "opt-A2.h"
#include <types.h>
#include <kern/errno.h>
#if OPT_A2
#include <kern/fcntl.h>
#endif
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#if OPT_A2
#include <limits.h>
#endif
#if OPT_A2
#include <pid.h>
#include <vfs.h>
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

  //kprintf("i am in sys_fork");

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

// progname_uspace is user pointer that points to user space
int sys_execv(const char *progname_uspace, char ** args_uspace){

  struct addrspace *as_old;
  struct addrspace *as_new;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;

  char * progname_kspcae;
  size_t progname_actual_len;

  result = copyinstr((const_userptr_t) progname_uspace, progname_kspcae, PATH_MAX, &progname_actual_len);

  if (result){

    return result;

  }

  size_t execv_args_len = 0;
  for (int i = 0; args_uspace[i] != NULL; i++){

    execv_args_len ++;

  }

  //execv_args_len ++; // for NULL

  char * args_kspace[execv_args_len + 1];
  size_t args_actual_len;
  size_t ele_len;

  size_t each_len_args[execv_args_len];

  for (size_t i = 0; i < execv_args_len; i ++) {

    result = copyinstr( (const_userptr_t) args_uspace[i], args_kspace[i], ARG_MAX, &ele_len);

    if (result){

      return result;

    }
    each_len_args[i] = ele_len;
    args_actual_len = args_actual_len + ele_len;


  }

  args_kspace[execv_args_len] = NULL;  

  if (args_actual_len > ARG_MAX){
    return E2BIG;
  }


  // cause our progname_kspace is char *, we do not need to do copy;
  /* Open the file. */
  result = vfs_open(progname_kspcae, O_RDONLY, 0, &v);
  if (result) {

    return result;
  }

  /* Create a new address space. */
  as_new = as_create();
  if (as_new == NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  as_old = curproc_setas(as_new);

  as_destroy(as_old);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as_new, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    return result;
  }

  userptr_t args_userspace[execv_args_len + 1];

  size_t stackptr_move;
  for (size_t i = 0; i < execv_args_len; i++){

    stackptr_move = ROUNDUP(each_len_args[i],8);
    stackptr -= stackptr_move;
    result = copyoutstr(args_userspace[i], (userptr_t) stackptr, ARG_MAX, &stackptr_move);
    args_userspace[i] = stackptr;

    if (result){

      return result;

    }
    
  }
  (userptr_t) args_userspace[execv_args_len] = NULL;

  stackptr_move = ROUNDUP(((execv_args_len + 1) * 4),8);
  stackptr -= stackptr_move;
  result = copyout(args_userspace, (userptr_t) stackptr, stackptr_move);
  if (result){

      return result;

  }

  /* Warp to user mode. */
  enter_new_process((execv_args_len + 1) /*argc*/,  (userptr_t)stackptr/*userspace addr of argv*/,
        stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  //panic("enter_new_process returned\n");
  return EINVAL;

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

  //struct proc_id * childret = kmalloc(sizeof(struct proc_id));
  //struct proc_id * childret;
  result = wait(curproc -> pid, pid, &exitstatus);
  
  if (result) {
    // cv_destroy(childret -> proc_cv);
    // kfree(childret);
    return(result);
  }

  //kprintf("return pid: %d", childret -> pid);
  //int exitcode = childret -> exit_code;

  //cv_destroy(childret -> proc_cv);
  //kfree(childret);
  exitstatus = _MKWAIT_EXIT(exitstatus);
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

