// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */
#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
/*
 * init_pte - Initialize PTE entry
 */
//Khởi tạo 1 entry trong bảng trang 

int init_pte(uint32_t *pte,
             int pre,    // present: trang đã tham chiếu tới bộ nhớ vật lí
             int fpn,    // FPN: frame vật lí chứa trang
             int drt,    // dirty: có bị chỉnh sửa gì k
             int swp,    // swap: có bị hoán đổi ra swap k
             int swptyp, // swap type: loại hoán trang (RAM , SSD,...)
             int swpoff) // swap offset: vị trí offet trong swap
{
  if (pre != 0) { //Trang đã nằm trong bộ nhớ vật lí 
    if (swp == 0) { // Non swap ~ page online //Trang ánh xạ đến frame
      // if (fpn == 0)
      //   return -1;  // Invalid setting //Hệ thống coi frame = 0 là k hợp lệ

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      // set bit 31 = 1 
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      //set bit 30 = 0 
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
      // CHưa đc sửa
      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
      // set 12 bit cho fpn
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
// CHỉnh sửa pte nếu đã swap từ ram sang SWAP
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
//CHỉnh sửa nếu chuyển ngược lại Ram
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}
//Hàm xin trả lại bộ nhớ cho ram
void tra_lai_cho_ram(struct pcb_t *caller, struct framephy_struct *list) {
  struct framephy_struct *cur = list;

  while (cur != NULL) {
      struct framephy_struct *next = cur->fp_next;

      // Thêm frame vào danh sách RAM tự do
      cur->fp_next = caller->mram->free_fp_list;
      caller->mram->free_fp_list = cur;

      cur = next;
  }
}
/*
 * vmap_page_range - map a range of page at aligned address
 */
//Hàm ánh xạ 1 dải khung trang vật lí (frame ) vào 1 vùng nhớ địa chỉ ảo của tiến trình
//CHính là bước Insert PTE của Fig 6
// Vmap_page là cái ánh xạ vào các trang ảo , range là nhiều
int vmap_page_range(struct pcb_t *caller,           // process call //Con trỏ đến đối tượng
                    int addr,                       // start address which is aligned to pagesz //addr là địa chỉ trong kgian bộ nhớ ảo mà tiến trình muốn ánh xạ đến
                    int pgnum,                      // num of mapping page  // SỐ trang trong bộ nhớ cần ánh xạ
                    struct framephy_struct *frames, // list of the mapped frames // danh sách các khung trang vật lí mà đc ánh xạ vào ko gian bộ nhớ ảo
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp // Dùng để return vùng nhớ ảo này
{                                                   // no guarantee all given pages are mapped
  //  lƯU Ý LÀ CÁC TRANG MUỐN ÁNH XẠ ĐẾN LÀ ĐANG TRỐNG HAY K
  //Chắc là đang trống , nếu k thì việc xóa toàn bộ dữ liệu của vùng nhớ ảo đó có thể ảnh hưởng tiến trình khác
  //struct framephy_struct *fpit;
  int pgn = PAGING_PGN(addr); // Xác định vị trí trang từ địa chỉ đã canh chỉnh cho trước

  /* TODO: update the rg_end and rg_start of ret_rg 
  //Cập nhập thông tin cho ret_rg
  //ret_rg->rg_end =  ....
  //ret_rg->rg_start = ...
  //ret_rg->vmaid = ...
  */
 struct framephy_struct *frames_run = frames;//Dùng để di chuyển trong linked list
 ret_rg->rg_start = addr;
 ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
 ret_rg->rg_next = NULL;
  /* TODO map range of frame to address space
   *      [addr to addr + pgnum*PAGING_PAGESZ
   *      in page table caller->mm->pgd[]
   */
  //ÁNh xạ các trang vật lí vào không gian bộ nhớ ảo
  //Nếu đã có trang đang sử dụng
  for (int i = 0; i < pgnum; i++) {
    int page_index = i + pgn;
    if (PAGING_PAGE_PRESENT(caller->mm->pgd[page_index])) {
      tra_lai_cho_ram(caller,frames_run);
      return -1;
    }
}
  int pgit = 0 ;
  for(; pgit  < pgnum; pgit ++){
    int virtual_page = pgn + pgit ; //CHỉ số trang trong bộ nhớ ảo
    int frame = frames_run->fpn;//Lấy số khung trang vât lí 
    struct framephy_struct* frame_tach_roi = frames_run;
    //Gán frames tiếp theo cho lần sau chạy
    frames_run = frames_run->fp_next;
    //Thêm frame tách rời vào ds đã sử dụng
    frame_tach_roi->fp_next = caller->mram->used_fp_list;
    caller->mram->used_fp_list = frame_tach_roi;
    init_pte(&caller->mm->pgd[virtual_page],1,frame,0,0,0,0);
    printf("[DEBUG] PTE after init: pgd[%d] = 0x%08x\n", virtual_page, caller->mm->pgd[virtual_page]);
  /* Tracking for later page replacement activities (if needed)
   * Enqueue new usage page */
  //Thêm page đã map đó vào fifo
  enlist_pgn_node(&caller->mm->fifo_pgn, pgn + pgit);
}
  return 0;
}
//Hàm xin lấy victim cho swap
struct pgn_t* find_victim(struct mm_struct *mm)
{
  struct pgn_t *pg = mm->fifo_pgn;
  if (!pg) return NULL;

  /* TODu: Implement the theorical mechanism to find the victim page */
  struct pgn_t* prev = NULL;
  while(pg->pg_next != NULL){
    prev = pg;
    pg = pg->pg_next;
  }
if (prev != NULL)
  prev->pg_next = NULL;   // Cắt node cuối ra khỏi danh sách
else
  mm->fifo_pgn = NULL;    // Trường hợp chỉ có 1 node
return pg;
}
/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */
//Hàm này mục tiêu  cung cấp 1 dải frame cho tiến trình và lưu các thông tin liên quan vào danh sách frm_list
//Hàm này giúp tạo frame cho hàm trên
int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  //Hiện thực frm_lst bằng cách linked_list
  //frm_lst là con trỏ trả về danh sách frame đang trống
  if(req_pgnum > caller->mram->maxsz / 256) return -3000;  // Kích thước yêu cầu quá lớn
  int pgit, fpn;
  struct framephy_struct *newfp_str = NULL;//head danh sách
  struct framephy_struct  *fp_hung = NULL;//con trỏ mới ra đời
  /* TODO: allocate the page 
  //caller-> ...
  //frm_lst-> ...
  */
  int check = 0 ; //Kiểm tra xem việc xin bộ nhớ có thành công không
//Lặp qua tìm đủ các frame trống , hoặc bị ép trống
  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
  /* TODO: allocate the page 
   */
   //Cấp phát bộ nhớ cho khung trang mới
    fp_hung = malloc(sizeof(struct framephy_struct));
    if (!fp_hung){
      tra_lai_cho_ram(caller,newfp_str);
      return -1 ;
    }
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0)
    {//Vì memphy_get_freefp đã giải phóng vùng nhớ thực quản lí frame nên cần tạo lại
      fp_hung->fpn = fpn;
      fp_hung->owner = caller->mm;
     // Thêm vào đầu danh sách
      fp_hung->fp_next = newfp_str;
      newfp_str = fp_hung; 
    }
    else
    { // TODO: ERROR CODE of obtaining somes but not enough frames
      //Nếu không cấp phát được thì ép buộc các page đầu tiên trả về ram 
      //Lấy cái được thêm vài fifo đầu tiên
      struct pgn_t * first_page = caller->mm->fifo_pgn;
      if(first_page == NULL){
        tra_lai_cho_ram(caller,newfp_str);
        return -3000;
      }
      //Tìm kiếm nạn nhân
      first_page = find_victim(caller->mm);

      uint32_t pte = caller->mm->pgd[first_page->pgn];
      int fpn_cho_ram = PAGING_FPN(pte);
      //Giải mã pgd của nó để lấy ram và chuyển toàn bộ nội dung này qua bộ nhớ SWAP
      if(MEMPHY_get_freefp(caller->active_mswp, &fpn) != 0){
        tra_lai_cho_ram(caller,newfp_str);
        //Trả page vừa mới xin lại cho nó
        enlist_pgn_node(&caller->mm->fifo_pgn,first_page->pgn);
        free(first_page);
        return -1;
      }
        // Nếu chuyển qua đc 
      if(__swap_cp_page(caller->mram,fpn_cho_ram,caller->active_mswp,fpn) == 0) {
      //Cập nhập pte 
      pte_set_swap(&caller->mm->pgd[first_page->pgn], 0, fpn); // Cập nhật offset và loại
      fp_hung->fpn = fpn_cho_ram;
      fp_hung->owner = caller->mm;
      fp_hung->fp_next = newfp_str;  // Gắn phần tử hiện tại trỏ đến đầu cũ
      newfp_str = fp_hung;           // Cập nhật đầu danh sách mới


      //Cập nhập vùng nhớ trống của active_mswp
      struct framephy_struct* frame_can_chuyen = malloc(sizeof(struct framephy_struct));
      frame_can_chuyen->owner = caller->mm;
      frame_can_chuyen->fpn = fpn;
      frame_can_chuyen->fp_next = caller->active_mswp->used_fp_list;
      caller->active_mswp->used_fp_list = frame_can_chuyen;
      }
      else{
        tra_lai_cho_ram(caller,newfp_str);
        //Trả page vừa mới xin lại cho nó
        enlist_pgn_node(&caller->mm->fifo_pgn,first_page->pgn);
        free(first_page);
        return -1;
      }
}
}
  //Trả về danh sách
  *frm_lst = newfp_str;
  return check;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
//Hàm giúp ánh xạ 1 vùng địa chỉ ảo sang Ram thông qua việc xin cấp phát khung trang và kiên kết chúng với các địa chỉ ảo 
//phục vụ cho việc khi xin thêm rg trong 1 cái vùng nhỏ vma liên tục để lưu cái gì đó trong mm-vm.c, 
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  //Lưu danh sách các frame RAM xin được
  int ret_alloc; 

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);
  
  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
#ifdef MMDBG
    printf("OOM: vm_map_ram out of memory \n");
#endif
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
//Copy 1 frame từ src sang dst ( vật lí sang vật lí)
 int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn)
{
  int cellidx;
  int addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
//Khởi tạo Memory_Management 
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
  memset(mm->pgd, 0, PAGING_MAX_PGN * sizeof(uint32_t));
  for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++) {
    mm->symrgtbl[i].rg_start = 2;
    mm->symrgtbl[i].rg_end = 1;
}
  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* TODO update VMA0 next */
   vma0->vm_next = NULL;

  /* Point vma owner backward */
  vma0->vm_mm = mm;  // Trả về

  /* TODO: update mmap */
  mm->mmap = vma0;
  
  return 0;
}

//Khởi tạo 1 vm_rg
struct vm_rg_struct *init_vm_rg(int rg_start, int rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}
//Thêm 1 rg vào linked list
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}
//Thêm 1 trang vào danh sách liên kết , chủ yếu dùng để thêm vào fifo_pgn
int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));
  
  pnode->pgn = pgn; // gán pgn
  pnode->pg_next = *plist;//thêm vào đầu danh sách, chút có gì lấy thì lấy cuối
  *plist = pnode;

  return 0;
}
//In ra các frame trong danh sách liên kết, có thể là các frame trống frame đã sử dụng hoặc frame xin cấp phát 
int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[%d]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

//In ra danh sách các region có thể trong free region gồm địa chỉ bắt đầu và kết thúc
int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[%ld->%ld]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

// In ra danh sách các vma gồm địa chỉ bắt đầu và kết thúc
int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[%d]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}
//In ra bảng ánh xạ của 1 tiến trình 
int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start, pgn_end;
  int pgit;
  //Không chỉ định vùng nhớ kết thúc
  if (end == -1)
  {
    pgn_start = 0;// Gán thứ tự trang ảo bắt đầu bằng 0 nhưng sau đó lại gán lại
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end; // Lấy địa chỉ cuối của vma0 
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d\n", start, end);
  if (caller == NULL) { printf("NULL caller\n"); return -1;}

  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }
  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    printf("Page Number: %d -> Frame Number: %d\n", pgit,PAGING_PTE_FPN(caller->mm->pgd[pgit]));
  }
  printf("================================================================\n");
  return 0;
}

// #endif
