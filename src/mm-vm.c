// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
//tìm vm_area_struct theo vm_id trong mm_struct( phụ trách việc quản lí toàn bộ vùng nhớ ảo của 1 tiến trình)
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;//Lấy con trỏ đầu danh sách

  if (mm->mmap == NULL)
    return NULL;
    //Không có danh sách

  int vmait = pvma->vm_id;
  //Lấy id của vma đầu
  while (vmait < vmaid)
  {
    if (pvma == NULL)//Kiểm tra NULL
      return NULL;

    pvma = pvma->vm_next;
    vmait = pvma->vm_id;
  }

  return pvma;
}
//Hoán frame từ RAM sang SWAP 
int __mm_swap_page(struct pcb_t *caller, int vicfpn , int swpfpn)
{
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
//Tạo 1 vùng nhớ mới (region ) trong 1 vm_area_struct cụ thể vị trí brk
//nhưng lúc này chưa liên kết mà chỉ tạo ra các thông tin trong region_struc
//Kiểu chỉ dùng để tạo thông tin region cho hàm chính
 struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 //Lấy area cần ra 
  newrg = malloc(sizeof(struct vm_rg_struct));
 //Cấp phát 1 vùng nhớ thực để lưu cấu trúc region mới này
  /* TODO: update the newrg boundary
  //Xác định địa chỉ của region
  // newrg->rg_start = ...
  // newrg->rg_end = ...
  */
  if (!newrg || !cur_vma)
  return NULL;
  newrg->rg_start = cur_vma->vm_end;
  newrg->rg_end   = cur_vma->vm_end + alignedsz;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
//Hàm này kiểm tra xem 1 region nhớ ảo mới yêu cầu cấp phát có bị trùng với bất kì area nào k
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma = caller->mm->mmap;
  
  /* TODO validate the planned memory area is not overlapped */
  while(vma != NULL ){
    if(!(vmaend <= vma->vm_start || vma->vm_end <=vmastart))
    return 1;
    vma = vma->vm_next;
  }
  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  //Lưu ý biến tăng thêm chỉ là lượng yêu cầu trừ cho phần còn lại trong sbrk
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  //Cấp phát vùng nhớ lưu thông tin của vm_rg mới
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  //Làm tròn trang 
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  //Số trang cần xin thêm
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  //Lấy thông tin về vm_rg mới
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  //Lấy vma cần thêm
  int old_end = cur_vma->vm_end;
  if (!area || !cur_vma)
  return -1;
  /*Validate overlap of obtained region */
  //Nếu có chồng lấn thì dừng ko cấp phát tài nguyên ram
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1; /*Overlap and failed allocation */

  /* TODO: Obtain the new vm area based on vmaid */
  //cur_vma->vm_end... 
  // inc_limit_ret...
  newrg->rg_start = area->rg_start;
  newrg->rg_end = area->rg_end;
  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0)
    return -1; /* Map the memory to MEMRAM */
  //Cập nhập giới hạn mới
  cur_vma->vm_end = area->rg_end;
  return 0;
}

// #endif
