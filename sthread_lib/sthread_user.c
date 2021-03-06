/* Simplethreads Instructional Thread Package
 * 
 * sthread_user.c - Implements the sthread API using user-level threads.
 *
 *    You need to implement the routines in this file.
 *
 * Change Log:
 * 2002-04-15        rick
 *   - Initial version.
 * 2005-10-12        jccc
 *   - Added semaphores, deleted conditional variables
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <sthread.h>
#include <sthread_user.h>
#include <sthread_ctx.h>
#include <sthread_time_slice.h>
#include <sthread_user.h>
#include "RBT.h"


struct _sthread {
	sthread_ctx_t *saved_ctx;
	sthread_start_func_t start_routine_ptr;
	long wake_time;
  	int join_tid;
  	void *join_ret;
  	void *args;
  	int tid;          /* meramente informativo */

	int priority;
	int vruntime;
	int nice;
	int ticks;
	int sleeptime;
	int runtime;
	int waittime;

};

struct _sthread_mon {

 	sthread_mutex_t mutex;
	struct _sthread_mon *next;
	struct rbt *tree;
};

struct monitor_list {
	
	struct _sthread_mon *first;
};

static struct monitor_list *mon_lst;


static struct rbt *exe_thr_list;         /* lista de threads executaveis */
static queue_t *dead_thr_list;        /* lista de threads "mortas" */
static queue_t *sleep_thr_list;
static queue_t *join_thr_list;       
static queue_t *zombie_thr_list;
static struct _sthread *active_thr;   /* thread activa */
static int tid_gen;                   /* gerador de tid's */


#define CLOCK_TICK 10000

#define MIN_DELAY 5
#define MIN_PRIORITY 1
#define MAX_PRIORITY 10
#define BASE_VRUNTIME 0
static long Clock;


/*********************************************************************/
/* Part 1: Creating and Scheduling Threads                           */
/*********************************************************************/


void sthread_user_free(struct _sthread *thread);
void rbt_dump(struct node *ptr);
void sthread_aux_start(void){
	splx(LOW);
	active_thr->start_routine_ptr(active_thr->args);
	sthread_user_exit((void*)0);
}

void sthread_user_dispatcher(void);

void sleep_thr_dump();

void sthread_dump() 
{
	puts("=== dump start ===");
	puts("active thread");
	printf("id: %d priority: %d vruntime: %d \n", active_thr->tid, active_thr->priority, active_thr->vruntime);
	printf("runtime: %d sleeptime: %d \n", active_thr->runtime, active_thr->sleeptime);
	printf("waittime: %ld \n", active_thr->wake_time);
	puts("");
	puts(">>>> RB-Tree <<<<");
	rbt_dump(exe_thr_list->root);
	puts("");	
	puts(">>>>SleepList<<<<");
	puts("");
	sleep_thr_dump();
	puts(">>>>BlockedList<<<<");
	printf("id: priority: vruntime: \n");
	printf("runtime: sleeptime: \n");
	printf("waittime: \n");
	puts("Dump End");
}

void sleep_thr_dump()
{
	int min = -1;
	int i, count;

	queue_element_t *count_ptr = sleep_thr_list->first; 

	queue_element_t *tmp1 = sleep_thr_list->first;
	if(tmp1 != NULL){
		queue_element_t *tmp2 = tmp1->next;

		for(count = 0; count_ptr != NULL; count++, count_ptr = count_ptr->next);

		for(i = 0; i < count; i++){
			for(; tmp2 != NULL; tmp2 = tmp2->next){
				if(tmp1->thread->wake_time > tmp2->thread->wake_time && tmp2->thread->wake_time > min)
					tmp1 = tmp2;
			}
			for(tmp2 = tmp1; tmp2 != NULL; tmp2 = tmp2->next)
				if(tmp1->thread->wake_time == tmp2->thread->wake_time){
					printf("id: %d priority: %d vruntime: %d\n", tmp2->thread->tid, tmp2->thread->priority, tmp2->thread->vruntime);
					printf("runtime: %d sleeptime: %d\n", tmp2->thread->runtime, tmp2->thread->sleeptime);
					printf("waittime: %d\n", tmp2->thread->waittime);
					puts("");
				}
			min = tmp1->thread->wake_time;
			tmp1 = sleep_thr_list->first;
			tmp2 = tmp1->next;
		}
	}
}

void monitor_dump()
{	
	struct _sthread_mon *ptr = mon_lst->first;
	
	while(ptr != NULL) {
		rbt_dump(ptr->tree->root);
		ptr = ptr->next;
	}

}

void rbt_dump(struct node *ptr) 
{
	if (ptr->vruntime == 0)
		return;
	rbt_dump(ptr->left);
	queue_element_t *tmp = ptr->queue->first;
	while(tmp != NULL){
		printf("id: %d priority: %d vruntime: %d \n", tmp->thread->tid, tmp->thread->priority, tmp->thread->vruntime);
		printf("runtime: %d sleeptime: %d \n", tmp->thread->runtime, tmp->thread->sleeptime);
		printf("waittime: %d \n", tmp->thread->waittime);
		puts("");
		tmp = tmp->next;
	}
	rbt_dump(ptr->right);
}


int sthread_nice(int nice)
{
	if (nice >= 0 && nice <= 10) {
		active_thr->nice = nice;
		return active_thr->priority + nice;
	} else {
		puts("Invalid nice");
		return -1;
	}
}

void sthread_user_init(void) {

  	exe_thr_list = rbt_init();
	dead_thr_list = create_queue();
	sleep_thr_list = create_queue();
	join_thr_list = create_queue();
	zombie_thr_list = create_queue();
	tid_gen = 1;

	struct _sthread *main_thread = malloc(sizeof(struct _sthread));
	main_thread->start_routine_ptr = NULL;
	main_thread->args = NULL;
	main_thread->saved_ctx = sthread_new_blank_ctx();
	main_thread->wake_time = 0;
	main_thread->join_tid = 0;
	main_thread->join_ret = NULL;
	main_thread->tid = tid_gen++;

	main_thread->priority = 1;
	main_thread->vruntime = 1;
	main_thread->nice = 0;
	main_thread->ticks = 0;
	
	main_thread->sleeptime = 0;
	main_thread->runtime = 0;
	main_thread->waittime = 0;
  
  	active_thr = main_thread;

  	Clock = 1;
  	sthread_time_slices_init(sthread_user_dispatcher, CLOCK_TICK);
}


sthread_t sthread_user_create(sthread_start_func_t start_routine, void *arg, int priority)
{
  	struct _sthread *new_thread = (struct _sthread*)malloc(sizeof(struct _sthread));

	sthread_ctx_start_func_t func = sthread_aux_start;
  	new_thread->args = arg;
 	new_thread->start_routine_ptr = start_routine;
  	new_thread->wake_time = 0;
  	new_thread->join_tid = 0;
  	new_thread->join_ret = NULL;
  	new_thread->saved_ctx = sthread_new_ctx(func);

	new_thread->priority = priority;
	new_thread->vruntime = exe_thr_list->first->vruntime;
	new_thread->nice = 0;
	new_thread->ticks = 0;
	new_thread->sleeptime = 0;
	new_thread->runtime = 0;
	new_thread->waittime = 0;
  
  	splx(HIGH);
	new_thread->tid = tid_gen++;

  	rbt_insert(exe_thr_list, new_thread->vruntime, new_thread);

  	splx(LOW);
  	return new_thread;
}


void sthread_user_exit(void *ret) {
	splx(HIGH);
   
   	int is_zombie = 1;

   	// unblock threads waiting in the join list
   	queue_t *tmp_queue = create_queue();   
   	while (!queue_is_empty(join_thr_list)) {
      		struct _sthread *thread = queue_remove(join_thr_list);
     
      		//printf("Test join list: join_tid=%d, active->tid=%d\n", thread->join_tid, active_thr->tid);

      		if (thread->join_tid == active_thr->tid) {
         		thread->join_ret = ret;
			rbt_insert(exe_thr_list, thread->vruntime ,thread);
         		is_zombie = 0;
      		} else {
			queue_insert(tmp_queue,thread);
      		}
   	}
   	delete_queue(join_thr_list);
   	join_thr_list = tmp_queue;
 
   	if (is_zombie) {
      		queue_insert(zombie_thr_list, active_thr);
   	} else {
      		queue_insert(dead_thr_list, active_thr);
   	}
   

   	if(rbt_is_empty(exe_thr_list)) {  /* pode acontecer se a unica thread em execucao fizer */
    		rbt_destroy(exe_thr_list);              /* sthread_exit(0). Este codigo garante que o programa sai bem. */
    		delete_queue(dead_thr_list);
    		sthread_user_free(active_thr);
    		printf("Exec queue is empty!\n");
    		exit(0);
  	}

  
   	// remove from exec list
   	struct _sthread *old_thr = active_thr;
   	active_thr = rbt_remove_first(exe_thr_list);
   	sthread_switch(old_thr->saved_ctx, active_thr->saved_ctx);

   	splx(LOW);
}

void add_waitting_time(struct node *node){
	
	if (node->vruntime == 0)
		return;
	
	queue_element_t *tmp = node->queue->first;

	add_waitting_time(node->left);
	for(;tmp != NULL; tmp = tmp->next)
		tmp->thread->waittime++;
	add_waitting_time(node->right);

}

void sthread_user_dispatcher(void)
{
   	Clock++;
   	
	active_thr->ticks++;
	active_thr->runtime += CLOCK_TICK;

   	active_thr->ticks++;
 	active_thr->vruntime += active_thr->ticks * (active_thr->priority + active_thr->nice);
 	
	queue_t *tmp_queue = create_queue();

	add_waitting_time(exe_thr_list->root);	

   	while (!queue_is_empty(sleep_thr_list)) {
      	struct _sthread *thread = queue_remove(sleep_thr_list);
      
      		if (thread->wake_time == Clock) {
         		thread->wake_time = 0;
         		rbt_insert(exe_thr_list,thread->vruntime, thread);
      		} else {
			thread->sleeptime++;
         		queue_insert(tmp_queue,thread);
      		}
   	}	
   	delete_queue(sleep_thr_list);
   	sleep_thr_list = tmp_queue;
  	
   	if (active_thr->ticks >= MIN_DELAY && !rbt_is_empty(exe_thr_list) && active_thr->vruntime > exe_thr_list->first->vruntime)
	   	sthread_user_yield();
}


void sthread_user_yield(void)
{
  	splx(HIGH);
  	active_thr->ticks = 0;
  	struct _sthread *old_thr;
  	old_thr = active_thr;
  	rbt_insert(exe_thr_list, old_thr->vruntime, old_thr);
  	active_thr = rbt_remove_first(exe_thr_list);
  	sthread_switch(old_thr->saved_ctx, active_thr->saved_ctx);
  	splx(LOW);
}




void sthread_user_free(struct _sthread *thread)
{
  	sthread_free_ctx(thread->saved_ctx);
  	free(thread);
}


/*********************************************************************/
/* Part 2: Join and Sleep Primitives                                 */
/*********************************************************************/

int sthread_user_join(sthread_t thread, void **value_ptr)
{
   /* suspends execution of the calling thread until the target thread
      terminates, unless the target thread has already terminated.
      On return from a successful pthread_join() call with a non-NULL 
      value_ptr argument, the value passed to pthread_exit() by the 
      terminating thread is made available in the location referenced 
      by value_ptr. When a pthread_join() returns successfully, the 
      target thread has been terminated. The results of multiple 
      simultaneous calls to pthread_join() specifying the same target 
      thread are undefined. If the thread calling pthread_join() is 
      canceled, then the target thread will not be detached. 

      If successful, the pthread_join() function returns zero. 
      Otherwise, an error number is returned to indicate the error. */

   
   	splx(HIGH);
   	// checks if the thread to wait is zombie
   	int found = 0;
   	queue_t *tmp_queue = create_queue();
   	while (!queue_is_empty(zombie_thr_list)) {
      		struct _sthread *zthread = queue_remove(zombie_thr_list);
      		if (thread->tid == zthread->tid) {
         		*value_ptr = thread->join_ret;
         		queue_insert(dead_thr_list,thread);
         		found = 1;
      		} else {
         		queue_insert(tmp_queue, zthread);
      		}
   	}
   	delete_queue(zombie_thr_list);
   	zombie_thr_list = tmp_queue;
  
   	if (found) {
       		splx(LOW);
       		return 0;
   	}
   	// search active queue
   	if (active_thr->tid == thread->tid)
      		found = 1;
   
	queue_element_t *qe = NULL;

   	// search exe
   	struct node *rbt_node = rbt_find(exe_thr_list, thread->vruntime);
   	
   	if (rbt_node != NULL) {
 
		qe = rbt_node->queue->first;
		while (!found && qe != NULL) {
			if (qe->thread->tid == thread->tid) {
			found = 1;
 			}
			qe = qe->next;
 		}
 	}
	// search sleep
	qe = sleep_thr_list->first;
	while (!found && qe != NULL) {
		if (qe->thread->tid == thread->tid)
			found = 1;
		qe = qe->next;
   	}

   	// search join
   	qe = join_thr_list->first;
   	while (!found && qe != NULL) {
      		if (qe->thread->tid == thread->tid) {
         		found = 1;
      		}
      	qe = qe->next;
   	}

   	// if found blocks until thread ends
   	if (!found) {
      		splx(LOW);
      		return -1;
   	} else {
      		active_thr->join_tid = thread->tid;
      
      		struct _sthread *old_thr = active_thr;
      		queue_insert(join_thr_list, old_thr);
      		active_thr = rbt_remove_first(exe_thr_list);
      		sthread_switch(old_thr->saved_ctx, active_thr->saved_ctx);
  
      		*value_ptr = thread->join_ret;
   	}
   
   	splx(LOW);
   	return 0;
}


/* minimum sleep time of 1 clocktick.
  1 clock tick, value 10 000 = 10 ms */
int sthread_user_sleep(int time)
{
   	splx(HIGH);
   
   	long num_ticks = time / CLOCK_TICK;
   	if (num_ticks == 0) {
      		splx(LOW);
      		return 0;
   	}
   
   	active_thr->wake_time = Clock + num_ticks;
	active_thr->sleeptime += num_ticks;
   	queue_insert(sleep_thr_list,active_thr);

   	sthread_t old_thr = active_thr;
   	active_thr = rbt_remove_first(exe_thr_list);
   	sthread_switch(old_thr->saved_ctx, active_thr->saved_ctx);
   
   	splx(LOW);
   	return 0;
}

/* --------------------------------------------------------------------------*
 * Synchronization Primitives                                                *
 * ------------------------------------------------------------------------- */

/*
 * Mutex implementation
 */

struct _sthread_mutex
{
  	lock_t l;
  	struct _sthread *thr;
  	queue_t* queue;
};

sthread_mutex_t sthread_user_mutex_init()
{
  	sthread_mutex_t lock;

	if(!(lock = malloc(sizeof(struct _sthread_mutex)))) {
    		printf("Error in creating mutex\n");
    		return 0;
  	}	

  	/* mutex initialization */
  	lock->l=0;
  	lock->thr = NULL;
  	lock->queue = create_queue();
  
  	return lock;
}

void sthread_user_mutex_free(sthread_mutex_t lock)
{
  	delete_queue(lock->queue);
  	free(lock);
}

void sthread_user_mutex_lock(sthread_mutex_t lock)
{
  	while(atomic_test_and_set(&(lock->l))) {}

  	if(lock->thr == NULL){
    		lock->thr = active_thr;
    		atomic_clear(&(lock->l));
  	} else {
    		queue_insert(lock->queue, active_thr);
    
    		atomic_clear(&(lock->l));

    		splx(HIGH);
    		struct _sthread *old_thr;
    		old_thr = active_thr;
    		//rbt_insert(exe_thr_list,old_thr->vruntime, old_thr);
    		active_thr = rbt_remove_first(exe_thr_list);
    		sthread_switch(old_thr->saved_ctx, active_thr->saved_ctx);

    		splx(LOW);
  	}
}

void sthread_user_mutex_unlock(sthread_mutex_t lock)
{
  	if(lock->thr!=active_thr){
    		printf("unlock without lock!\n");
    		return;
  	}

  	while(atomic_test_and_set(&(lock->l))) {}

  	if(queue_is_empty(lock->queue)){
    		lock->thr = NULL;
  	} else {
    		lock->thr = queue_remove(lock->queue);
    		rbt_insert(exe_thr_list, lock->thr->vruntime, lock->thr);
  	}	

  	atomic_clear(&(lock->l));
}

/*
 * Monitor implementation
 */



sthread_mon_t sthread_user_monitor_init()
{
  	sthread_mon_t mon;
  	if(!(mon = malloc(sizeof(struct _sthread_mon)))){
    		printf("Error creating monitor\n");
    		return 0;
  	}

  	mon->mutex = sthread_user_mutex_init();
  	mon->tree = rbt_init();
	
	mon->next = mon_lst->first;
	mon_lst->first = mon;

  	return mon;
}

void sthread_user_monitor_free(sthread_mon_t mon)
{	
	struct _sthread_mon *ptr1 = mon_lst->first;

	if (ptr1 == mon) {
		sthread_user_mutex_free(ptr1->mutex);
		rbt_destroy(ptr1->tree);
		mon_lst->first =ptr1->next;
		free(ptr1);
		return;
	}
	
	if (ptr1->next != NULL) {
	
		struct _sthread_mon *ptr2 = ptr1->next;

		while(ptr2 != mon) {
			ptr1 = ptr2;
			ptr2 = ptr2->next;
		}
	
		ptr1 = ptr2->next;

 		sthread_user_mutex_free(ptr2->mutex);
 	 	rbt_destroy(ptr2->tree);
  		free(ptr2);
	} else 
		puts("ERROR: No such monitor");
}

void sthread_user_monitor_enter(sthread_mon_t mon)
{
  	sthread_user_mutex_lock(mon->mutex);
}

void sthread_user_monitor_exit(sthread_mon_t mon)
{
	sthread_user_mutex_unlock(mon->mutex);
}

void sthread_user_monitor_wait(sthread_mon_t mon)
{
  	struct _sthread *temp;

  	if(mon->mutex->thr != active_thr){
    		printf("monitor wait called outside monitor\n");
    		return;
  	}

  	/* inserts thread in queue of blocked threads */
  	temp = active_thr;
  	rbt_insert(mon->tree,temp->vruntime, temp);

  	/* exits mutual exclusion region */
  	sthread_user_mutex_unlock(mon->mutex);

  	splx(HIGH);
  	struct _sthread *old_thr;
  	old_thr = active_thr;
  	active_thr = rbt_remove_first(exe_thr_list);
  	sthread_switch(old_thr->saved_ctx, active_thr->saved_ctx);
  	splx(LOW);
}

void sthread_user_monitor_signal(sthread_mon_t mon)
{
  	struct _sthread *temp;

  	if(mon->mutex->thr != active_thr){
    		printf("monitor signal called outside monitor\n");
    		return;
  	}

  	while(atomic_test_and_set(&(mon->mutex->l))) {}
  	if(!rbt_is_empty(mon->tree)){
    		/* changes blocking queue for thread */
    		temp = rbt_remove_first(mon->tree);
    		queue_insert(mon->mutex->queue, temp);
  	}
  	atomic_clear(&(mon->mutex->l));
}

void sthread_user_monitor_signalall(sthread_mon_t mon)
{
  	struct _sthread *temp;

  	if(mon->mutex->thr != active_thr){
    		printf("monitor signalall called outside monitor\n");
    		return;
  	}

  	while(atomic_test_and_set(&(mon->mutex->l))) {}
  	while(!rbt_is_empty(mon->tree)){
    	/* changes blocking queue for thread */
    		temp = rbt_remove_first(mon->tree);
    		queue_insert(mon->mutex->queue, temp);
  	}
  	atomic_clear(&(mon->mutex->l));
}


/* The following functions are dummies to 
 * highlight the fact that pthreads do not
 * include monitors.
 */
sthread_mon_t sthread_dummy_monitor_init()
{
   	printf("WARNING: pthreads do not include monitors!\n");
   	return NULL;
}


void sthread_dummy_monitor_free(sthread_mon_t mon)
{
   	printf("WARNING: pthreads do not include monitors!\n");
}


void sthread_dummy_monitor_enter(sthread_mon_t mon)
{
   	printf("WARNING: pthreads do not include monitors!\n");
}


void sthread_dummy_monitor_exit(sthread_mon_t mon)
{
   	printf("WARNING: pthreads do not include monitors!\n");
}


void sthread_dummy_monitor_wait(sthread_mon_t mon)
{
   	printf("WARNING: pthreads do not include monitors!\n");
}


void sthread_dummy_monitor_signal(sthread_mon_t mon)
{
   	printf("WARNING: pthreads do not include monitors!\n");
}

void sthread_dummy_monitor_signalall(sthread_mon_t mon)
{
   	printf("WARNING: pthreads do not include monitors!\n");
}

