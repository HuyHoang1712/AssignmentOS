// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
//di chuyển con trỏ (cursor) trong bộ nhớ vật lý den851 một vị trí offset
int MEMPHY_mv_csr(struct memphy_struct *mp, int offset)
{
   int numstep = 0;

   mp->cursor = 0;
   while (numstep < offset && numstep < mp->maxsz)
   {
      /* Traverse sequentially */
      mp->cursor = (mp->cursor + 1) % mp->maxsz;
      numstep++;
   }

   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
// Hàm hỗ trợ việc đọc 1 byte tuần tự
int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value)
{
   if (mp == NULL)
      return -1;

   if (!mp->rdmflg) // nếu k random 
      return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   *value = (BYTE)mp->storage[addr];

   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct *mp, int addr, BYTE *value)
{
   if (mp == NULL)
      return -1; 

   if (mp->rdmflg)
      *value = mp->storage[addr]; // nếu là radom thì truy xuất
   else /* Sequential access device */
      return MEMPHY_seq_read(mp, addr, value); //Nếu là tuần tự thì gọi hàm

   return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
//Tương tự trên
int MEMPHY_seq_write(struct memphy_struct *mp, int addr, BYTE value)
{

   if (mp == NULL)
      return -1;

   if (!mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   mp->storage[addr] = value;

   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct *mp, int addr, BYTE data)
{
   if (mp == NULL)
      return -1;

   if (mp->rdmflg)
      mp->storage[addr] = data;
   else /* Sequential access device */
      return MEMPHY_seq_write(mp, addr, data);

   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
//Khởi tạo danh sách các frame vật lí trống trong bộ nhớ vật lí mô phỏng 
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
   /* This setting come with fixed constant PAGESZ */
   int numfp = mp->maxsz / pagesz; // Số frame
   struct framephy_struct *newfst, *fst; 
   int iter = 0;

   if (numfp <= 0)
      return -1; 
    //Cấp phát phần tử đầu tiên
   /* Init head of free framephy list */
   fst = malloc(sizeof(struct framephy_struct)); // Khởi tạo 
   fst->fpn = iter;
   mp->free_fp_list = fst;

   /* We have list with first element, fill in the rest num-1 element member*/
   for (iter = 1; iter < numfp; iter++)
   {
      // Cấp phát các phần tử tiếp theo
      newfst = malloc(sizeof(struct framephy_struct));
      newfst->fpn = iter;
      newfst->fp_next = NULL;
      fst->fp_next = newfst;
      fst = newfst;
   }

   return 0;
}
//Cấp phát 1 frame vật lí từ danh sách các frame còn trống trong bộ nhớ Ram
// mp: con trỏ đến cấu trúc quản lí bộ nhớ vật lí ( đối tượng chính là RAM) 
// retfpn là con trỏ trả về số hiệu frame đc cấp phát
int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn)
{
   struct framephy_struct *fp = mp->free_fp_list;

   if (fp == NULL)
      return -1; // Hết frame trống

   *retfpn = fp->fpn; // lấy frame đầu tiên
   mp->free_fp_list = fp->fp_next; //Bỏ phần tử đầu

   /* MEMPHY is iteratively used up until its exhausted
    * No garbage collector acting then it not been released
    */
   free(fp); // Giải phóng vùng nhớ quản lí 

   return 0;
}


  /*TODO dump memphy contnt mp->storage
   *     for tracing the memory content
   */
  int MEMPHY_dump(struct memphy_struct *mp)
{
  if (mp == NULL) return -1;

  printf("=== MEMPHY DUMP ===\n");
  for (int i = 0; i < mp->maxsz; i++) {
    printf("Addr %03d: %02x\n", i, mp->storage[i]);
  }
  return 0;
}
//Trả lại 1 frame vào danh sách trống
int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn)
{
   struct framephy_struct *fp = mp->free_fp_list;
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

   /* Create new node with value fpn */
   newnode->fpn = fpn;
   newnode->fp_next = fp;
   mp->free_fp_list = newnode;

   return 0;
}

/*
 *  Init MEMPHY struct
 */
//Khởi tạo 1 bộ nhớ vật lí
int init_memphy(struct memphy_struct *mp, int max_size, int randomflg)
{
   mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
   mp->maxsz = max_size;
   memset(mp->storage, 0, max_size * sizeof(BYTE));

   MEMPHY_format(mp, PAGING_PAGESZ);

   mp->rdmflg = (randomflg != 0) ? 1 : 0;

   if (!mp->rdmflg) /* Not Ramdom acess device, then it serial device*/
      mp->cursor = 0;

   return 0;
}

// #endif
