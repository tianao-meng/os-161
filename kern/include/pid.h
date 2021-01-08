#if OPT_A2
#ifndef _PID_H_
#define _PID_H_

#include "opt-A2.h"

struct proc_id {

	pid_t pid;
	struct proc_id * parent;
	int exit_code;
	struct cv * proc_cv;
	pid_t require;

}


void pid_array_create(void);
int allocate_pid(struct proc_id * parent, struct proc_id * child_return);
int wait(struct proc_id * parent, pid_t child_pid, struct proc_id * childret);
void exit(struct proc_id * proc, int exitcode);
void delete_pid(proc_id * proc);

#endif /* _PID_H_ */
	
