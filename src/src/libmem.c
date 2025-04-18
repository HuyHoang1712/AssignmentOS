/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */
#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "cpu.h"
static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
//Thêm 1 new rg vào trong freerg_list 
//Bởi vì mặc định sử dụng vma0 
//Nên chỉ cần lấy đầu tiên là đc
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start > rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
//Lấy vùng chứa biến tại rgid
//Hưng đã sửa
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  {
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
        return NULL;

    if (mm->symrgtbl[rgid].rg_start <= mm->symrgtbl[rgid].rg_end)
        return &mm->symrgtbl[rgid];

    for (int i = rgid - 1; i >= 0; i--) {
        if (mm->symrgtbl[i].rg_start <= mm->symrgtbl[i].rg_end)
            return &mm->symrgtbl[i];
    }

    return NULL; // không có vùng nào hợp lệ
}
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  //pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct rgnode;
  //Lưu trữ thong tin vùng nhớ được cấp phát
  /* TODO: commit the vmaid */
  // rgnode.vmaid
  //Bước 1 : Thử tìm vùng nhớ trống
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) //Vùng này đã đc map tới bộ nhớ vật lí
  {
    //Nếu tìm thấy thì cập nhập vào bảng ký hiệu để dễ quản lí
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    //Lưu địa chỉ vùng nhớ cấp phát trả về cho liballoc
    *alloc_addr = rgnode.rg_start;
    //Tránh tranh cấp vùng nhớ ảo
    //pthread_mutex_unlock(&mmvm_lock);
    return 0;//Thành công
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  //Bước 2 : Trường hợp cấp phát thất bại
  /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
  /*Attempt to increate limit to get space */
  //Lấy thông tin vùng nhớ hiện tại
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (!cur_vma) {
   // pthread_mutex_unlock(&mmvm_lock);
    return -1; // Không tìm thấy VMA => không thể cấp phát thêm
  }
  //Căn chỉnh kích thước cần cấp phát theo kích thước trang bộ nhớ
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  //int inc_limit_ret;

  /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
  //Bước 3 : Gọi syscall để xin cấp phát thêm bộ nhớ từ hệ thống
  struct sc_regs regs;
  regs.a1 = 2;
  regs.a2 = cur_vma->vm_id;
  regs.a3 = inc_sz ;

  /* TODO INCREASE THE LIMIT as inovking systemcall 
   * sys_memap with SYSMEM_INC_OP 
   */
  //Gọi syscall với các tham số trên
  int old_end = cur_vma->vm_end;
  int inc_limit_ret = syscall(caller,17,&regs);
  if(inc_limit_ret<0){
 // pthread_mutex_unlock(&mmvm_lock);
  return -1;//Cấp phát thất bại
  }
  cur_vma->sbrk += inc_sz;
  rgnode.rg_start = old_end;
  rgnode.rg_end = old_end + size;
  //Nếu còn dư bộ nhớ thêm phần dư vào danh sách vùng nhớ trống
  if(inc_sz > size){
    struct vm_rg_struct *free_region = malloc(sizeof(struct vm_rg_struct));
    free_region->rg_start = old_end + size;
    free_region->rg_end = old_end + inc_sz;
    free_region->rg_next = NULL;

    // Thêm vào danh sách vùng nhớ trống
   enlist_vm_freerg_list(caller->mm,free_region);
  }
  caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
  caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
  *alloc_addr = rgnode.rg_start;
  //pthread_mutex_unlock(&mmvm_lock);
  /* SYSCALL 17 sys_memmap */

  /* TODO: commit the limit increment */

  /* TODO: commit the allocation address 
  // *alloc_addr = ...
  */

  return 0;

}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  //struct vm_rg_struct rgnode;

  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later
  //Check điều kiện rgid
  if(rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return -1;
  struct vm_rg_struct * rgnode = malloc(sizeof(struct vm_rg_struct));
  rgnode->rg_start = caller->mm->symrgtbl[rgid].rg_start;
  rgnode->rg_end = caller->mm->symrgtbl[rgid].rg_end;
  rgnode->rg_next = NULL;
  /* TODO: Manage the collect freed region to freerg_list */
  if (rgnode->rg_start >= rgnode->rg_end){
    return -1;
  }
  /*enlist the obsoleted memory region */

  if(enlist_vm_freerg_list(caller->mm,rgnode) != 0){
    return -1;//Lỗi khi thêm vào danh sách
  }
  //Xóa toàn bộ dữ liệu hiện có trong bộ nhớ vật lý
  caller->mm->symrgtbl[rgid].rg_start = 2;
  caller->mm->symrgtbl[rgid].rg_end = 1;
  
  //Đánh dấu vùng nhớ k còn sử dụng nx
  return 0;//Thành công
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  /* TODO Implement allocation on vm area 0 */
  // Dùng để lưu địa chỉ vùng nhớ cấp phát
  int addr;
  pthread_mutex_lock(&mmvm_lock);
  printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
  if(__alloc(proc,0,reg_index,size,&addr) <0 ){
    printf("Cấp phát thất bại");
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  printf("PID=%d - Region=%d - Address=%08X - Size=%d byte\n", proc->pid, reg_index, addr, size);
  print_pgtbl(proc,0,-1);
  pthread_mutex_unlock(&mmvm_lock);
  /* By default using vmaid = 0 */
  return 0;

}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  /* TODO Implement free region */
  pthread_mutex_lock(&mmvm_lock);
  printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
  if(__free(proc,0,reg_index) < 0 ){
    printf("Thu hồi thất bại");
    return -1;
  }
  /* By default using vmaid = 0 */
  printf("PID=%d - Region=%d\n", proc->pid, reg_index);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
//Xử lý khi một trang ko có mặt trong RAM
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte_2 = mm->pgd[pgn];

  if (PAGING_PAGE_SWAPPED(pte_2) || !PAGING_PAGE_PRESENT(pte_2))
  { /* Page is not online, make it actively living */
    int vicpgn_1, swpfpn_1; 
     /* 1. Tìm trang nạn nhân (FIFO) */
     if (find_victim_page(caller->mm, &vicpgn_1) != 0)
     return -1; //Tìm kiếm nạn nhân
    //int vicfpn;
    //uint32_t vicpte;
    //Đi giải mã 
     uint32_t pte_1 = caller->mm->pgd[vicpgn_1];//Nơi lưu ram cần dữ
     int vicfpn_1 = PAGING_PTE_FPN(pte_1);
    /* 2. Cấp phát frame trống trong SWAP */
    if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn_1) != 0)
      return -1;
     /* 3. SWAP: Đẩy trang nạn nhân từ RAM -> SWAP */

    //int tgtfpn = PAGING_PTE_SWP(pte);//the target frame storing our variable

    /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/

    /* TODO copy victim frame to swap 
     * SWP(vicfpn <--> swpfpn)
     * SYSCALL 17 sys_memmap 
     * with operation SYSMEM_SWP_OP
     */
    struct sc_regs regs;
    regs.a1 = 3; // page number nạn nhân
    regs.a2 = vicfpn_1; // frame bên SWAP
    regs.a3 =swpfpn_1; // định nghĩa: RAM -> SWAP
    //struct sc_regs regs;
    //regs.a1 =...
    //regs.a2 =...
    //regs.a3 =..
    if (syscall(caller, 17, &regs) != 0){
       MEMPHY_put_freefp(caller->active_mswp,swpfpn_1);
       return -1;
    }
     pte_set_swap(&caller->mm->pgd[vicpgn_1],0,swpfpn_1);
    /* SYSCALL 17 sys_memmap */
     //Đọc dữ liệu từ SWAP sang RAM
     int swpfrm = PAGING_PTE_SWP(pte_2);
     if(__swap_cp_page(caller->active_mswp,swpfrm,caller->mram,vicfpn_1)){
       MEMPHY_put_freefp(caller->mram,vicfpn_1);
       return -1;
     }
    pte_set_fpn(&mm->pgd[pgn],vicfpn_1);
    /* TODO copy target frame form swap to mem 
     * SWP(tgtfpn <--> vicfpn)
     * SYSCALL 17 sys_memmap
     * with operation SYSMEM_SWP_OP
     */

    /* TODO copy target frame form swap to mem 
    //regs.a1 =...
    //regs.a2 =...
    //regs.a3 =..
    */

    /* SYSCALL 17 sys_memmap */

    /* Update page table */
    //pte_set_swap() 
    //mm->pgd;

    /* Update its online status of the target page */
    //pte_set_fpn() &
    //mm->pgd[pgn];
    //pte_set_fpn();

    enlist_pgn_node(&caller->mm->fifo_pgn,pgn);
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
//Hàm dùng để đọc giá trị byte từ địa chỉ ảo 
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */
    int phyaddr = (fpn << NBITS(PAGING_PAGESZ)) + off;
    struct sc_regs regs;
    regs.a1 = 4;
    regs.a2 = phyaddr;
    if(syscall(caller,17,&regs) != 0)
    return -1;
    *data = regs.a3 ;
  /* TODO 
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ 
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  // int phyaddr
  //struct sc_regs regs;
  //regs.a1 = ...
  //regs.a2 = ...
  //regs.a3 = ...

  /* SYSCALL 17 sys_memmap */

  // Update data
  // data = (BYTE)

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
// GHi giá trị value vào 1 địa chỉ ảo
 int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */
  int phyaddr = (fpn << NBITS(PAGING_PAGESZ)) + off;
  struct sc_regs regs;
  regs.a1 = 5;
  regs.a2 = phyaddr;
  regs.a3 = value;
  if(syscall(caller,17,&regs)!= 0)
  return -1;
  /* TODO
   *  MEMPHY_write(caller->mram, phyaddr, value);
   *  MEMPHY WRITE
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
   */
  // int phyaddr
  //struct sc_regs regs;
  //regs.a1 = ...
  //regs.a2 = ...
  //regs.a3 = ...

  /* SYSCALL 17 sys_memmap */

  // Update data
  // data = (BYTE) 

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;
  //pthread_mutex_lock(&mmvm_lock);
  pg_getval(caller->mm, currg->rg_start + offset, data, caller);
  //pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  pthread_mutex_lock(&mmvm_lock);
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
  //destination 
  printf("===== PHYSICAL MEMORY AFTER READING =====\n");
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  pthread_mutex_unlock(&mmvm_lock);
  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
//Dùng để ghi value vào 1 cụ thể theo offset region ảo cụ thể của tiến trình
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  // Lấy trong bảng sys table
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  //Lấy vma theo vmaid

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;
  //pthread_mutex_lock(&mmvm_lock);
  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  //pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{  
  pthread_mutex_lock(&mmvm_lock);
  int val =__write(proc, 0, destination, offset, data);
  printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
pthread_mutex_unlock(&mmvm_lock);
  return val;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
//Vô dụng
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
//Tìm page để hoán trang khi ram đầy
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;
  if (!pg) return -1;

  /* TODu: Implement the theorical mechanism to find the victim page */
  struct pgn_t* prev = NULL;
  while(pg->pg_next != NULL){
    prev = pg;
    pg = pg->pg_next;
  }
  *retpgn = pg->pgn;
if (prev != NULL)
  prev->pg_next = NULL;   // Cắt node cuối ra khỏi danh sách
else
  mm->fifo_pgn = NULL;    // Trường hợp chỉ có 1 node

free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
//Tìm vùng nhớ kích thước đủ lớn
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  struct vm_rg_struct * prev = NULL;
  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  //Khởi tạo rằng chưa có vùng nhớ hợp lệ , có rồi thì cập nhập mới
  newrg->rg_start = newrg->rg_end = -1;

  /* TODO Traverse on list of free vm region to find a fit space */
  //while (...)
  // ..
  //Duyệt danh sách tìm vùng nhớ đủ lớn
  while(rgit != NULL){
    unsigned long rgit_size = rgit->rg_end -rgit->rg_start;
    if (rgit_size >= size ) //Nếu đủ lớn thì
    {  
      // Vì đây là con trỏ newrg nên khi ra khỏi hàm vẫn còn giữ giá trị
      newrg -> rg_start = rgit -> rg_start ;
      newrg -> rg_end = newrg-> rg_start + size ;
    //Cập nhập lại danh sách trống 
      //Cắt bỏ vùng vừa cấp phát ra khỏi danh sách trống
      rgit-> rg_start += size ;
      //Nếu vùng trống còn không thì loại 
      if(rgit->rg_start >= rgit->rg_end){
      //Nếu rgit là đầu tiên 
      if (prev == NULL){
       cur_vma->vm_freerg_list = rgit->rg_next;
      }
      else{
        prev ->rg_next = rgit->rg_next ;
      }
      }
     return 0; 
    }
    prev = rgit;
    rgit = rgit->rg_next ;

  }
  return -1;
}

//#endif
