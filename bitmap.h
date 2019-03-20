#ifndef _OUICHEFS_BITMAP_H
#define _OUICHEFS_BITMAP_H

#include <linux/bitmap.h>
#include "ouichefs.h"

static inline uint32_t first_free_bit(unsigned long *freemap,
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

static inline uint32_t get_free_inode(struct ouichefs_sb_info *sbi)
{
	uint32_t ret;

	ret = first_free_bit(sbi->ifree_bitmap, sbi->nr_ifree_blocks);
	if (ret)
		sbi->nr_free_inodes--;
	return ret;
}

static inline uint32_t get_free_block(struct ouichefs_sb_info *sbi)
{
	uint32_t ret;

	ret = first_free_bit(sbi->bfree_bitmap, sbi->nr_bfree_blocks);
	if (ret)
		sbi->nr_free_blocks--;
	return ret;
}

static inline void put_free_bit(unsigned long *freemap, uint32_t nr_blocks,
				uint32_t i)
{
	unsigned long *map = freemap + i / 64;

	/* i is greater than freemap size */
	if (i > nr_blocks * OUICHEFS_BLOCK_SIZE * 8)
		return;

	bitmap_set(map, i % 64, 1);
}

static inline void put_inode(struct ouichefs_sb_info *sbi, uint32_t ino)
{
	put_free_bit(sbi->ifree_bitmap, sbi->nr_ifree_blocks, ino);
	sbi->nr_free_inodes++;
}

static inline void put_block(struct ouichefs_sb_info *sbi, uint32_t bno)
{
	put_free_bit(sbi->bfree_bitmap, sbi->nr_bfree_blocks, bno);
	sbi->nr_free_blocks++;
}

#endif	/* _OUICHEFS_BITMAP_H */
