
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;//ko còn sử dụng
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
void print_running_list(struct queue_t *queue) {
    printf("Running list: ");
	if (queue->size == 0 ) return;
    for (int i = 0; i < queue->size; i++) {

        if (queue->proc[i] != NULL) {
    printf("%d ", queue->proc[i]->pid);
}
    }
    printf("\n");
}
//Hưng tự thêm
struct pcb_t * dequeue_for_running_list(struct queue_t * q , struct pcb_t * proc){
	if (q->size == 0 || proc == NULL) return NULL;

	int found_index = -1;
	for (int i = 0; i < q->size; i++) {
		if (q->proc[i] == proc) {
			found_index = i;
			break;
		}
	}

	if (found_index == -1) {
		return NULL; // Không tìm thấy
	}

	// Dịch phần còn lại lên 1 ô
	for (int i = found_index + 1; i < q->size; i++) {
		q->proc[i - 1] = q->proc[i];
	}

	q->size--;
	printf("Sau khi dequeue: %d", proc->pid);
	print_running_list(&running_list);
	return proc;
}
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif
//Hàm này check xem hàng đợi có rỗng hay không
int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
		//Nếu có bất kỳ hàng đợi nào k rỗng trả về -1
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
	//Trả về 1 nếu rỗng hết , về 0 nếu còn trong 1 trong 2 hàng đợi
}
//Khởi tạo bộ lặp lịch, chuẩn bị các hàng đợi và slot
void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}
void reset_slots_if_needed(void) {
    for (int i = 0; i < MAX_PRIO; i++) {
        if (slot[i] > 0) {
            return;
        }
    }
    for (int i = 0; i < MAX_PRIO; i++) {
        slot[i] = MAX_PRIO - i;
    }
}
#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	//Bước 1: Khóa bảo vệ hàng đợi tránh tranh chấp
	pthread_mutex_lock(&queue_lock);
	//Bước 2: Duyệt danh sách tìm tiến trình 
	while(proc == NULL){
	for(int i = 0 ; i <MAX_PRIO ; i++){
     if(slot[i] > 0 && !empty(&mlq_ready_queue[i])){
		proc = dequeue(&mlq_ready_queue[i]);
		slot[i]--;
		break;
	 }
	}
	if(proc == NULL){
		reset_slots_if_needed();
		if(queue_empty() == 1){
			break;
		}
	}

}
    enqueue(&running_list,proc);
	if(proc != NULL)
    printf("Sau khi enqueue: %d ", proc->pid);
	print_running_list(&running_list);
    //Bước 3: Mở khóa
	pthread_mutex_unlock(&queue_lock);
	return proc;	
}

void put_mlq_proc(struct pcb_t * proc) {
	//pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	//pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	//pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	//pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
    //Đưa vào hàng đợi 
	
	//Khóa lại
	pthread_mutex_lock(&queue_lock);
	put_mlq_proc(proc);
	dequeue_for_running_list(&running_list,proc);
	/* TODO: put running proc to running_list */
    //Mở khóa
	pthread_mutex_unlock(&queue_lock);	

	
}

void add_proc(struct pcb_t * proc) {
	    //Đưa vào hàng đợi 
		
		//Khóa lại
		pthread_mutex_lock(&queue_lock);
		add_mlq_proc(proc);
		proc->ready_queue = &ready_queue;
		proc->mlq_ready_queue = mlq_ready_queue;
        proc->running_list = & running_list;
		/* TODO: put running proc to running_list */
		//Mở khóa
		pthread_mutex_unlock(&queue_lock);	
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);

    if (!empty(&ready_queue)) {
        proc = dequeue(&ready_queue);
    }
    enqueue(&running_list,proc);
    pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	dequeue_for_running_list(&running_list,proc);
	/* TODO: put running proc to running_list */
	pthread_mutex_unlock(&queue_lock);
}
void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */
	pthread_mutex_unlock(&queue_lock);
}
#endif
void dequeue_running(struct pcb_t * proc){
	dequeue_for_running_list(proc->running_list,proc);
}


