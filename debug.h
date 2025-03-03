/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ouiche_fs debug utils  
 *
 * Copyright (C) 2025 Luna Dremmen
 */

// undefine DEBUG to disable all logging!
#define DEBUG

#ifdef DEBUG

#include "linux/kern_levels.h"
#include "linux/printk.h"
#include "ouichefs.h"
#include "linux/buffer_head.h"

#endif


/*
 * Prints the content of a single block.
 * @sb: superblock of block
 * @block_no: block number
 * @prefix: optional prefix for every line
 */
void bflag_write_check(struct super_block *sb, uint32_t bno) {
#ifdef DEBUG
  //TODO: Check if block bno has b flag set and print smth if thats the case!
#endif
}



/*
 * Prints the content of a single block.
 * @sb: superblock of block
 * @bno: block number
 * @prefix: optional prefix for every line
 */
void dump_block(struct super_block *sb, uint32_t bno, char *prefix) {
#ifdef DEBUG

  struct buffer_head *bh = sb_bread(sb, bno);
	struct ouichefs_file_index_block *block =
		(struct ouichefs_file_index_block *)bh->b_data;
  
  if(!bh) {
    pr_err("Could not load requested block with bno %x on partition %s.\n", bno, sb->s_id);
    return;
  }
  print_hex_dump(KERN_DEBUG, prefix, DUMP_PREFIX_ADDRESS, 16, 4, block, OUICHEFS_BLOCK_SIZE, false);
#endif
}


/*
 * Compares two blocks.
 * @sb: superblock of blocks
 * @first_bno: first block number
 * @second_bno: second block number
 *
 * @return: 0 if blocks are the same, != 0 otherwise
 */
int32_t compare_blocks(struct super_block *sb, uint32_t first_bno, uint32_t second_bno) {
#ifdef DEBUG

  struct buffer_head *first_bh = sb_bread(sb, first_bno);
  if(!first_bh) return -EIO;

  struct ouichefs_file_index_block *first_block =
		(struct ouichefs_file_index_block *)first_bh->b_data;
  
  struct buffer_head *second_bh = sb_bread(sb, second_bno);
	if(!second_bh) return -EIO;
  
  struct ouichefs_file_index_block *second_block =
		(struct ouichefs_file_index_block *)second_bh->b_data;

  return(memcmp(first_block, second_block,OUICHEFS_BLOCK_SIZE));

#endif
}
