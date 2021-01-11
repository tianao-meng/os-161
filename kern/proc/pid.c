


#include "opt-A2.h"
#if OPT_A2
#include <types.h>
#include <kern/errno.h>
#include <limits.h>
#include <pid.h>
#include <proc.h>
#include <kern/wait.h>
#include <lib.h>
#include <current.h>
#include <synch.h>



//PID_MIN = 2; signed i32; only for user program
//for kernel program, i will assigned the kernel program to 1;

//int max_num = PID_MAX - PID_MIN + 1;
#define max_num PID_MAX - PID_MIN + 1
//global
struct proc_id * pid_array[max_num];

//synchro
struct lock * pid_lock;


// exit but not required;
// if not required, delete when the parent exit;
struct proc_id * pid_exit_buffer[max_num];

void pid_array_create(void) {

	//kprintf("i am in pid create");

	for (int i = 0; i < max_num; i++){

		//cause pid > 0; and PID_MIN = 2;
		pid_array[i] = NULL;

	}

	for (int i = 0; i < max_num; i++){

		pid_exit_buffer[i] = NULL;

	}

	pid_lock = lock_create("pid_lock");

	if (pid_lock == NULL){

		panic("cannot create pid_lock");

	}


}

bool check_if_in_buffer(pid_t pid){


	for (int i = 0; i < max_num; i++){

		if (pid_exit_buffer[i] != NULL){

			if (pid == pid_exit_buffer[i] -> pid){
				return true;
			}

		}


	}

	return false;

}

bool check_if_in_pid_array(struct proc_id * proc){


	for (int i = 0; i < max_num; i++){

		if (pid_array[i] != NULL){

			if (proc -> pid == pid_array[i] -> pid){
				return true;
			}

		}



	}

	return false;

}


int find_available_pos(void){



	for (int i = 0; i < max_num; i++){

		if ((pid_array[i] == NULL) && (!check_if_in_buffer(i+2))){
			return i;
		}

	}

	return -1;
}


void allocate_pid(struct proc_id * parent, struct proc_id * child_return) {

	lock_acquire(pid_lock);

	// if parent has exited, we do not have to put the process in the buffer;
	for (int i = 0; i < max_num; i++){

		if (pid_exit_buffer[i] != NULL){

			if (pid_exit_buffer[i] -> parent == NULL){

				cv_destroy(pid_exit_buffer[i] -> proc_cv);
				kfree(pid_exit_buffer[i]);
				pid_exit_buffer[i] = NULL;

			}

		}

	}

	int pos = find_available_pos();

	if (pos == -1){

		//child_return = kmalloc(sizeof(struct proc_id));
		child_return -> pid = -1;
		return;

		
	}

	pid_t next_available_pid = pos + 2;
	//child_return = kmalloc(sizeof(struct proc_id));
	if (child_return == NULL){

		panic("out of memory to create pid");

	}
	child_return -> pid = next_available_pid;
	if (parent == NULL){
		child_return -> parent = NULL;
		child_return -> proc_cv = cv_create(curproc -> p_name);
		//child_return -> exit_code = -1;
		//child_return -> require = 0;
	} else {

		child_return -> parent = parent;
		//for wait pid
		child_return -> proc_cv = cv_create(curproc -> p_name);
		//child_return -> exit_code = -1;
		//child_return -> require = 0;

	}

	pid_array[pos] = child_return;
	lock_release(pid_lock);
	

}

struct proc_id * get_proc_pid(pid_t pid){

	for (int i = 0; i < max_num; i++){

		if (pid_array[i] != NULL){
			if (pid == pid_array[i] -> pid){
				return pid_array[i];
			}			
		}


	}

	return NULL;


}

struct proc_id * get_proc_pid_inbuffer(pid_t pid){

	for (int i = 0; i < max_num; i++){

		if (pid_exit_buffer[i] != NULL){
			if (pid == pid_exit_buffer[i] -> pid){
				return pid_exit_buffer[i];
			}
		}



	}

	return NULL;


}

int wait(struct proc_id * parent, pid_t child_pid, int * childret){

	KASSERT(parent != NULL);
	KASSERT(child_pid > 1);

	lock_acquire(pid_lock);
	int err;
	struct proc_id * child = get_proc_pid(child_pid);
	
	if (child == NULL){

		child = get_proc_pid_inbuffer(child_pid);

		if (child == NULL){

			err = ESRCH;
			lock_release(pid_lock);
			return err;

		}



	}

	if (child -> parent -> pid != parent -> pid){

		err = ECHILD;
		lock_release(pid_lock);
		return err;

	}
	parent -> require = child_pid;

	// if the child still not exit, wait until it exit
	if (check_if_in_pid_array(child)){

		parent -> require = child_pid;

		cv_wait(parent -> proc_cv, pid_lock);

		//parent -> require = 0;
		for (int i = 0; i < max_num; i++){

			if (pid_array[i] != NULL){

				if (pid_array[i] -> pid == child -> pid){

					*childret = child -> exit_code;

					cv_destroy(child -> proc_cv);
					kfree(child);
					pid_array[i] = NULL;
					break;

				}

			}



		}

		lock_release(pid_lock);

		return 0;

	} else {

		if (check_if_in_buffer(child -> pid)){


			for (int i = 0; i < max_num; i++){
				if ((pid_exit_buffer[i] != NULL) && (pid_exit_buffer[i] -> pid == child -> pid)){

					*childret = child -> exit_code;

					//safe to exit
					for (int j = 0; j < max_num; j++){

						if (pid_exit_buffer[i] != NULL){
							if (pid_exit_buffer[i] -> pid == child -> pid){

								cv_destroy(pid_exit_buffer[i] -> proc_cv);
								kfree(pid_exit_buffer[i]);
								pid_exit_buffer[i] = NULL;
								lock_release(pid_lock);
								return 0;

							}							
						}



					}

					// cv_destroy(child -> proc_cv);
					// kfree(child);
					// pid_exit_buffer[i] = NULL;
					//lock_release(pid_lock);
					

				}

			}




		}



	}
	err = ESRCH;
	return err;





}

void exit(struct proc_id * proc, int exitcode){


	proc -> exit_code = exitcode;
	//pid_t pid = proc -> pid;

	lock_acquire(pid_lock);
	if ((proc -> parent != NULL) && (proc -> parent -> require == proc -> pid)){
		
		//proc -> parent -> require = 0;
		cv_signal(proc -> parent -> proc_cv, pid_lock);
		return;

	} else {

		if (proc -> parent != NULL){

			for (int i = 0; i < max_num; i++){

				if (pid_exit_buffer[i] == NULL){

					pid_exit_buffer[i] = proc;
					break;
					
				}
			}

			for (int i = 0; i < max_num; i++){

				if (pid_array[i] != NULL){
					if (pid_array[i] -> pid == proc -> pid){

							pid_array[i] = NULL;
							break;
						}					
				}



			}

			lock_release(pid_lock);

			return;

		} else {

			for (int i = 0; i < max_num; i++){

				if (pid_array[i] != NULL){
					if (pid_array[i] -> pid == proc -> pid){

							pid_array[i] = NULL;
							break;
						}				
				}



			}

			lock_release(pid_lock);		
			return;	

		}




	}
	


}

void delete_pid(struct proc_id * proc) {

	if (proc == NULL) {
		return;
	} else {

		for (int i = 0; i < max_num; i++){

			if (pid_array[i] != NULL){
				if (pid_array[i] -> pid == proc -> pid){

					pid_array[i] = NULL;
					break;

				}

			}



		}
		cv_destroy(proc -> proc_cv);
		kfree(proc);


	}

}




#endif

