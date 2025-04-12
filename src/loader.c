
#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t avail_pid = 1;

#define OPT_CALC	"calc"
#define OPT_ALLOC	"alloc"
#define OPT_FREE	"free"
#define OPT_READ	"read"
#define OPT_WRITE	"write"
#define OPT_SYSCALL	"syscall"

static enum ins_opcode_t get_opcode(char * opt) {
	if (!strcmp(opt, OPT_CALC)) {
		return CALC;
	}else if (!strcmp(opt, OPT_ALLOC)) {
		return ALLOC;
	}else if (!strcmp(opt, OPT_FREE)) {
		return FREE;
	}else if (!strcmp(opt, OPT_READ)) {
		return READ;
	}else if (!strcmp(opt, OPT_WRITE)) {
		return WRITE;
	}else if (!strcmp(opt, OPT_SYSCALL)) {
		return SYSCALL;
	}else{
		printf("get_opcode return Opcode: %s\n", opt);
		exit(1);
	}
}

struct pcb_t * load(const char * path) {
	/* Create new PCB for the new process */
	//Cấp phát 1 vùng nhớ thật lưu pcb_t mới cho tiến trình mới từ file vào 
	struct pcb_t * proc = (struct pcb_t * )malloc(sizeof(struct pcb_t));
	//gán p_id = 1 ,2 ,3 ,4 ,...
	proc->pid = avail_pid;
	avail_pid++;
	//Tạo vùng nhớ thật cho việc quản lí phân trang bộ nhớ
	proc->page_table =
		(struct page_table_t*)malloc(sizeof(struct page_table_t));
	
	proc->bp = PAGE_SIZE;
	proc->pc = 0;

	/* Read process code from file */
	//Đọc thogn6 tin tiến trình
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		printf("Cannot find process description at '%s'\n", path);
		exit(1);		
	}
	//Lưu thông tin đường dẫn lại vào proc->path
	snprintf(proc->path, 2*sizeof(path)+1, "%s", path);

	char opcode[10];
	proc->code = (struct code_seg_t*)malloc(sizeof(struct code_seg_t));
	//Cấp phát vùng nhớ cho nơi lưu trữ mã lệnh , chỉ lưu địa chỉ của mảng text , ko lưu mảng text
	fscanf(file, "%u %u", &proc->priority, &proc->code->size);
	//Đọc priority , số tiến trình
	proc->code->text = (struct inst_t*)malloc(
		sizeof(struct inst_t) * proc->code->size
	);
	//Cấp phát vùng nhớ mảng text
	uint32_t i = 0;
	char buf[200];
	//Lặp qua từng dòng lệnh đọc các lệnh
	for (i = 0; i < proc->code->size; i++) {
		fscanf(file, "%s", opcode);
		proc->code->text[i].opcode = get_opcode(opcode);
		//Lấy op_code là calc alloc,...
		//Lấy các thông số còn lại thôi.
		switch(proc->code->text[i].opcode) {
		case CALC:
			break;
		case ALLOC:
			fscanf(
				file,
				"%u %u\n",
				&proc->code->text[i].arg_0,
				&proc->code->text[i].arg_1
			);
			break;
		case FREE:
			fscanf(file, "%u\n", &proc->code->text[i].arg_0);
			break;
		case READ:
		case WRITE:
			fscanf(
				file,
				"%u %u %u\n",
				&proc->code->text[i].arg_0,
				&proc->code->text[i].arg_1,
				&proc->code->text[i].arg_2
			);
			break;	
		case SYSCALL:
			fgets(buf, sizeof(buf), file);
			sscanf(buf, "%d%d%d%d",
			           &proc->code->text[i].arg_0,
			           &proc->code->text[i].arg_1,
			           &proc->code->text[i].arg_2,
			           &proc->code->text[i].arg_3
			);
			break;
		default:
			printf("Opcode: %s\n", opcode);
			exit(1);
		}
	}
	return proc;
}



