
#include "cpu.h"
#include "mem.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"

int calc(struct pcb_t *proc) 
{
	return ((unsigned long)proc & 0UL);//ép kiểu con trỏ proc thành số nguyên và thực hiện phép AND 0 
}
// Luôn trả về kết quả không

int alloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{ //Gọi mem cấp phát vùng nhớ
	addr_t addr = alloc_mem(size, proc);//Thành công trả về địa chỉ vùng nhớ
	if (addr == 0)
	{
		return 1;//Cấp phát thất bại
	}
	else
	{
		proc->regs[reg_index] = addr;//Lưu địa chỉ vào thanh ghi của tiến trình
		return 0;
	}
}

int free_data(struct pcb_t *proc, uint32_t reg_index)
{
	return free_mem(proc->regs[reg_index], proc);//Gọi mem thu hồi bộ nhớ
}

int read(
	struct pcb_t *proc, // Process executing the instruction
	uint32_t source,	// Index of source register
	uint32_t offset,	// Source address = [source] + [offset]
	uint32_t destination)
{ // Index of destination register

	BYTE data;//Lưu giá trị đọc được từ bộ nhớ ( nhưng chỉ đọc 1 byte)
	//Tất cả giá trị thanh ghi từ bộ thanh ghi của proc
	if (read_mem(proc->regs[source] + offset, proc, &data))
	{
		proc->regs[destination] = data;
		return 0;//Thành công 
	}
	else
	{
		return 1;//Thất bại
	}
}

int write(
	struct pcb_t *proc,	// Process executing the instruction
	BYTE data,		// Data to be wrttien into memory
	uint32_t destination, // Index of destination register
	uint32_t offset)
{ // Destination address =
	// [destination] + [offset]
	return write_mem(proc->regs[destination] + offset, proc, data);
 //Viết đúng 1 byte dữ liệu vào vị trí [des] + [off]
}

int run(struct pcb_t *proc)
{ //Lệnh run thực thi 1 lệnh của tiến trình bằng cách check PC, lấy lệnh, giải mã, gọi hàm xử lí
	/* Check if Program Counter point to the proper instruction */
	if (proc->pc >= proc->code->size)
	{
		//proc-> code lưu trữ text của proc
		return 1;//Thất bại khi pc vượt quá kích thước mã lệnh 
	}

	struct inst_t ins = proc->code->text[proc->pc];
	//Lấy 1 lệnh
	proc->pc++;
	int stat = 1;
switch (ins.opcode)
	{
	case CALC:
		stat = calc(proc);
		break;
	case ALLOC:
#ifdef MM_PAGING
        //Nếu dủng phân trang thì gôi
		stat = liballoc(proc, ins.arg_0, ins.arg_1);
#else
		stat = alloc(proc, ins.arg_0, ins.arg_1);
#endif
		break;
	case FREE:
#ifdef MM_PAGING
		stat = libfree(proc, ins.arg_0);
#else
		stat = free_data(proc, ins.arg_0);
#endif
		break;
	case READ:
#ifdef MM_PAGING
		stat = libread(proc, ins.arg_0, ins.arg_1, &ins.arg_2);
#else
		stat = read(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	case WRITE:
#ifdef MM_PAGING
		stat = libwrite(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#else
		stat = write(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	case SYSCALL:
		stat = libsyscall(proc, ins.arg_0, ins.arg_1, ins.arg_2, ins.arg_3);
		break;
	default:
		stat = 1;
	}
	return stat;
}
