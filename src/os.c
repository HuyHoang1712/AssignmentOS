
#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"
//Hưng thêm
#include "queue.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//Hưng tự thêm
#define MM_FIXED_MEMSZ
static int time_slot;
//Thời gian tối đa trong 1 lần vào, nhập từ input
static int num_cpus;
//Số lượng CPU, nhập từ input
static int done = 0;
//Biến trạng thái xđ khi nào tất cả tiến trình hoàn thành

#ifdef MM_PAGING
//Nếu có Phân trang
static int memramsz;
//Kích thước bộ nhớ RAM, nhập từ input
static int memswpsz[PAGING_MAX_MMSWP];
//Kích thước các bộ nhớ SWAP
/*Lưu thông tin liên quan việc quản lí toàn bộ bộ nhớ
RAM, ảo , swap. giúp cấp phát bộ nhớ cho tiến trình, xđ khi nào cần dùng swap
*/ 
struct mmpaging_ld_args {
	/* A dispatched argument struct to compact many-fields passing to loader */
	int vmemsz; // Kích thước bộ nhớ ảo của tiến trình
	//VD: tiến trình dùng 512MB bộ nhớ ảo thì 256MB Ram chỉ có thể, còn lại SWAP
	struct memphy_struct *mram;
	//Con trỏ đến bộ nhớ vật lý
	struct memphy_struct **mswp;
	//Mảng chứa các vùng bộ nhớ SWAP
	struct memphy_struct *active_mswp;
	//vùng swap hiện tại đang sử dụng
	int active_mswp_id;
	//ID của vùng swap đang sử dụng
	struct timer_id_t  *timer_id;
	//Con trỏ đến bộ đếm thời gian
};
#endif
//Dùng để quản lí thông tin việc nạp tiến trình vào hệ thống
//Của cả hệ thống ko riêng gì bất kỳ CPU nào
static struct ld_args{
	char ** path;
	//Mảng chứa đường dẫn đến các tệp tin thực thi của từng tiến trình
	unsigned long * start_time;
	//Mảng chứa thời gian khởi chạy của từng tiến trình
#ifdef MLQ_SCHED
	unsigned long * prio;
	//Mảng chứa thứ tự ưu tiên của mỗi tiến trình
#endif
} ld_processes;
int num_processes;// Số lượng tiến trình
//Lưu thong tin từng CPU trong hệ thống mô phỏng 
struct cpu_args {
	struct timer_id_t * timer_id;
	//Bộ đếm thời gian
	int id; //ID của CPU
};
//Mô phỏng hoạt động của 1 CPU trong hệ thống đa nhiệm
//Liên tục kiểm tra thực thi tiến trình.

static void * cpu_routine(void * args) {
	//args là con trỏ đến cấu trúc cpu_args
	//Lấy các thông tin của cpu này vào
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */

	int time_left = 0;//Biến đếm thời gian còn lại của tiến trình
	struct pcb_t * proc = NULL;//Trỏ đến tiến trình đang chạy trên CPU
	//CPU liên tục kiểm tra và thực thi tiến trình đến khi ko còn
	while (1) {
		/* Check the status of current process */
		if (proc == NULL) {
			//Ko có tiến trình đang chạy load vào 1 tiến trình
			/* No process is running, the we load new process from
		 	* ready queue */
			proc = get_proc();
			//Lấy tiến trình mới từ hàng đợi
			if (proc == NULL) {
                           next_slot(timer_id);
						   //Làm xong trong 1s và đợi bộ đếm tăng lên giây tiếp theo, trong khi các tiến trình khác vẫn đang làm công việc [0,1]s
                           continue; /* First load failed. skip dummy load */
                        }
		}else if (proc->pc == proc->code->size) {
			/* The porcess has finish it job */
			//Tiến trình chạy xong, lấy tiến trình mới
			printf("\tCPU %d: Processed %2d has finished\n",
				id ,proc->pid);
			dequeue_running(proc);
			free(proc);//Giải phóng bộ nhớ máy tính thực cung cấp cho proc trong loader, ko liên quan gì đến bộ nhớ mô phỏng
			
			proc = get_proc();//Lấy tiến trình mới
			time_left = 0;// Gán thời gian còn lại tiến trình đã hoàn thành = 0
		}else if (time_left == 0) {
			//Hết thời gian trong CPU
			/* The process has done its job in current time slot */
			printf("\tCPU %d: Put process %2d to run queue\n",
				id, proc->pid);
			put_proc(proc);//Cất ngược lại queue
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		if (proc == NULL && done) {
			/* No process to run, exit */
			//Khi hoàn tất tất cả tiến trình thì dừng while
			printf("\tCPU %d stopped\n", id);
			break;
		}else if (proc == NULL) {
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			//Vẫn có thể còn những tiến trình mới để chạy trong các khung thời gian kế tiếp, bỏ qua khung thời gian hiện tại
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			//CPu vừa lấy tiến trình mới hoặc tiến trình trước đã hết lượt chạy
			printf("\tCPU %d: Dispatched process %2d\n",
				id, proc->pid);
			time_left = time_slot;//Cài thời gian tiếp cho tiến trình mới vào
		}
		
		/* Run current process */
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);//Ngắt kết nối bộ đếm thời gina
	pthread_exit(NULL);//Kết thúc luồng an toàn và giải phóng tài nguyên
}
//Hàm giúp quản lý việc nạp tiến trình từ các dữ liệu cấu hình vào mô hình (ready_queue)
static void * ld_routine(void * args) {
#ifdef MM_PAGING
    //Nếu có phân trang -> lấy thông tin RAM ,SWAP của mô hình (trước đó có thể đã sử dụng)
	struct memphy_struct* mram = ((struct mmpaging_ld_args *)args)->mram;
	struct memphy_struct** mswp = ((struct mmpaging_ld_args *)args)->mswp;
	struct memphy_struct* active_mswp = ((struct mmpaging_ld_args *)args)->active_mswp;
	struct timer_id_t * timer_id = ((struct mmpaging_ld_args *)args)->timer_id;
#else
	struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif
	int i = 0;
	printf("ld_routine\n");
	while (i < num_processes) {
		struct pcb_t * proc = load(ld_processes.path[i]);
		//Đọc thông tin tiến trình từ path[i] và tạo cho nó 1 pcb
#ifdef MLQ_SCHED
		proc->prio = ld_processes.prio[i];
		//Nếu có mode này thì gán cho nó prio thích hợp
#endif
		while (current_time() < ld_processes.start_time[i]) {
			next_slot(timer_id);
		}
		//Chờ đến thời gian để đưa tiến trình vào hệ thống
#ifdef MM_PAGING //Nếu hệ thống có phân trang, cấp phát và khởi tạo quản lý bộ nhớ cho tiến trình
		proc->mm = malloc(sizeof(struct mm_struct));
		//Cấu trúc mm_struct giúp quản lí chỉ riêng 1 tiến trình
		//Tạo ra vùng nhớ trên máy tính thực để lưu biến mm
		init_mm(proc->mm, proc);//Khởi tạo thông tin phân trang
		proc->mram = mram;//Khi tiến trình cần truy xuất,thì truy xuất,chứ tiến trình kko chiếm hoàn toàn
		proc->mswp = mswp;//
		proc->active_mswp = active_mswp;
#endif
		printf("\tLoaded a process at %s, PID: %d PRIO: %ld\n",
			ld_processes.path[i], proc->pid, ld_processes.prio[i]);
		add_proc(proc);
		//Sau khi tới giờ thì đưa nó vào ready_queue
		free(ld_processes.path[i]);//Giải phóng vùng nhớ trên máy tính thật
		i++;//Tăng i ( số proc đã tải lên mô hình)
		next_slot(timer_id);//Đợi đợt tiếp theo
	}
	free(ld_processes.path);
	free(ld_processes.start_time);
	done = 1;
	detach_event(timer_id);//Ngắt kết nối với bộ đếm thời gian
	pthread_exit(NULL);//Kết thúc luồng an toàn
}
//Đọc cấu hình từ đường dẫn path thiết lập các tham số hệ thống
static void read_config(const char * path) {
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		//Mở cấu hình để đọc
		//Nếu k mở đc chương trình báo lỗi và thoát
		printf("Cannot find configure file at %s\n", path);
		exit(1);
	}
	//Đọc 3 giá trị đầu tiên từ tệp
	fscanf(file, "%d %d %d\n", &time_slot, &num_cpus, &num_processes);
	//Cấp phát bộ nhớ để lưu đường dẫn cho ld_routine
	ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
	//Cấp phát vùng nhớ lưu thời gian bắt đầu từng tiến trình
	ld_processes.start_time = (unsigned long*)
		malloc(sizeof(unsigned long) * num_processes);
#ifdef MM_PAGING
	int sit;
#ifdef MM_FIXED_MEMSZ
	/* We provide here a back compatible with legacy OS simulatiom config file
         * In which, it have no addition config line for Mema, keep only one line
	 * for legacy info 
         *  [time slice] [N = Number of CPU] [M = Number of Processes to be run]
         */
        memramsz    =  0x100000; //Gần 1MB
        memswpsz[0] = 0x1000000;//16MB
	for(sit = 1; sit < PAGING_MAX_MMSWP; sit++)
		memswpsz[sit] = 0;//Mặc định các swap khác 0MB
#else
//Bộ nhớ tùy chỉnh dọc thêm dòng MEM_RAM_SZ
	/* Read input config of memory size: MEMRAM and upto 4 MEMSWP (mem swap)
	 * Format: (size=0 result non-used memswap, must have RAM and at least 1 SWAP)
	 *        MEM_RAM_SZ MEM_SWP0_SZ MEM_SWP1_SZ MEM_SWP2_SZ MEM_SWP3_SZ
	*/
	fscanf(file, "%d\n", &memramsz);
	//Đọc 1 số nguyên và bỏ qua dấu xuống dòng
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++)
		fscanf(file, "%d", &(memswpsz[sit])); 
    //Đọc kích thước các vùng swap
       fscanf(file, "\n"); /* Final character */
#endif
#endif

#ifdef MLQ_SCHED
	ld_processes.prio = (unsigned long*)
		malloc(sizeof(unsigned long) * num_processes);
		//Cấp phát vùng nhớ cho 1 bảng các prio
#endif
	int i;
	for (i = 0; i < num_processes; i++) {
		ld_processes.path[i] = (char*)malloc(sizeof(char) * 100);
		//Cấp phát 100 byte lưu đường dẫn file tiến trình
		ld_processes.path[i][0] = '\0';
		//Đặt kí tự đầu tiên để đảm bảo chuỗi trống
		strcat(ld_processes.path[i], "input/proc/");
		//Lúc này chuỗi path[i] bao gồm "input/proc/" 
		//Ghép 2 chuỗi
		char proc[100];
#ifdef MLQ_SCHED
		fscanf(file, "%lu %s %lu\n", &ld_processes.start_time[i], proc, &ld_processes.prio[i]);
		
		//Nếu có MLQ_SCHED thì đọc 3 biến : thời gian bắt đầu, tên của proc, độ ưu tiên
#else
		fscanf(file, "%lu %s\n", &ld_processes.start_time[i], proc);
		//Không cóa thì chỉ đọc 2 biến
#endif
		strcat(ld_processes.path[i], proc);
		printf("Process:%s\n",ld_processes.path[i]);
		// ghép 2 con trỏ chuỗi lại vs nhau
		//Lúc này chuỗi sẽ bao gồm: input/proc/p1s.
		//Dùng để sử dụng cho ld_routine
	}
}
//XOng hàm thì ta đã có các struct liên quan:ld_args,các biến static

int main(int argc, char * argv[]) {
	/* Read config */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n");
		return 1;
	}//Không cóa đường dẫn chứa cấu hình , in thông báo và thoát
	char path[100];
	path[0] = '\0';
	strcat(path, "input/");
	strcat(path, argv[1]);
	printf("Full path: %s\n", path); 
	read_config(path);
	//Đọc file cấu hình "input/os_1_mlq_paging"

	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	//Mô phỏng Nhiều CPU bằng cách dùng nhiều thread
	struct cpu_args * args =
		(struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	// args lưu mảng các cấu hình cpu_args
	pthread_t ld;
	//Luồng sử lý đưa tiến trình vào mô phỏng (queue) hay còn gọi loader
	
	/* Init timer */
	int i;
	for (i = 0; i < num_cpus; i++) {
		args[i].timer_id = attach_event();
		//Gán bộ đếm thời gian cho từng CPU
		args[i].id = i;
		//Gán id cho CPU
	}
	struct timer_id_t * ld_event = attach_event();
	//Tạo ra bộ đếm thời gian cho mmpaging_struct
	start_timer();
	//Bắt đầu đếm thời gian

#ifdef MM_PAGING
	/* Init all MEMPHY include 1 MEMRAM and n of MEMSWP */
	//Khởi tạo các bộ nhớ RAM và SWAP
	int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */
	//Cờ mặc định là ram

	struct memphy_struct mram;
	struct memphy_struct mswp[PAGING_MAX_MMSWP];

	/* Create MEM RAM */
	init_memphy(&mram, memramsz, rdmflag);
	//Cấp phát 1 vùng nhớ trên máy tính thật cho mô phỏng ram

        /* Create all MEM SWAP */ 
	int sit;
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++)
	       init_memphy(&mswp[sit], memswpsz[sit], rdmflag);
	//Cấp phát 1 vùng nhớ trên máy tình thật cho mô phỏng swap

	/* In Paging mode, it needs passing the system mem to each PCB through loader*/
	struct mmpaging_ld_args *mm_ld_args = malloc(sizeof(struct mmpaging_ld_args));
    //Cấp phát 1 vùng nhớ trên mt thật cho việc quản lí toàn bộ bộ nhớ
	mm_ld_args->timer_id = ld_event;
	//gán bộ đếm thời gian cho quản lí bộ nhớ
	mm_ld_args->mram = (struct memphy_struct *) &mram;
	//Đưa các vùng nhớ cấp phát vào quản lí
	mm_ld_args->mswp = (struct memphy_struct**) &mswp;
	mm_ld_args->active_mswp = (struct memphy_struct *) &mswp[0];
	//Gán swap 0 làm bộ nhớ swap đầu tiên
        mm_ld_args->active_mswp_id = 0;
#endif

	/* Init scheduler */
	init_scheduler();
	//Chuẩn bị cho sched

	/* Run CPU and loader */
#ifdef MM_PAGING
	pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
	//Tạo ra luồng thực thi hàm la_routine
#else
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL,
			cpu_routine, (void*)&args[i]);
	}
   //Tạo ra luồng thực thi cho các cpu
	/* Wait for CPU and loader finishing */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);
	}
	pthread_join(ld, NULL);

	/* Stop timer */
	stop_timer();

	return 0;

}



