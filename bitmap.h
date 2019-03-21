/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _OUICHEFS_BITMAP_H
#define _OUICHEFS_BITMAP_H

#include <linux/bitmap.h>
#include "ouichefs.h"

/*
 * Return the first free bit (set to 1) in a given in-memory bitmap spanning
 * over multiple blocks and clear it.
 * Return 0 if no free bit found (we assume that the first bit is never free
 * because of the superblock and the root inode, thus allowing us to use 0 as an
 * error value).
 */
static inline uint32_t get_first_free_bit(unsigned long *freemap,
					  uint32_t nr_blocks)
{
	int i;
	unsigned long *map = freemap;
	uint32_t ino = 64;

	for (i = 0; i < (nr_blocks * OUICHEFS_BLOCK_SIZE / 8); i++) {
		ino = find_first_bit(map, 64);
		if (ino < 64)
			break;
		map++;
	}
	if (ino == 64)
		return 0;

	bitmap_clear(map, ino, 1);

	return i * 64 + ino;
}

/*
 * Return an unused inode number and mark it used.
 * Return 0 if no free inode was found.
 */
static inline uint32_t get_free_inode(struct ouichefs_sb_info *sbi)
{
	uint32_t ret;

	ret = get_first_free_bit(sbi->ifree_bitmap, sbi->nr_ifree_blocks);
	if (ret)
		sbi->nr_free_inodes--;
	return ret;
}

/*
 * Return an unused block number and mark it used.
 * Return 0 if no free block was found.
 */
static inline uint32_t get_free_block(struct ouichefs_sb_info *sbi)
{
	uint32_t ret;

	ret = get_first_free_bit(sbi->bfree_bitmap, sbi->nr_bfree_blocks);
	if (ret)
		sbi->nr_free_blocks--;
	return ret;
}

/*
 * Mark the i-th bit in freemap as free (i.e. 1)
 */
static inline int put_free_bit(unsigned long *freemap, uint32_t nr_blocks,
				uint32_t i)
{
	unsigned long *map = freemap + i / 64;

	/* i is greater than freemap size */
	if (i > nr_blocks * OUICHEFS_BLOCK_SIZE * 8)
		return -1;

	bitmap_set(map, i % 64, 1);

	return 0;
}

/*
 * Mark an inode as unused.
 */
static inline void put_inode(struct ouichefs_sb_info *sbi, uint32_t ino)
{
	if (put_free_bit(sbi->ifree_bitmap, sbi->nr_ifree_blocks, ino))
		return;
	sbi->nr_free_inodes++;
}

/*
 * Mark a block as unused.
 */
static inline void put_block(struct ouichefs_sb_info *sbi, uint32_t bno)
{
	if (put_free_bit(sbi->bfree_bitmap, sbi->nr_bfree_blocks, bno))
		return;
	sbi->nr_free_blocks++;
}

#endif	/* _OUICHEFS_BITMAP_H */
