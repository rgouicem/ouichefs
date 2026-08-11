/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */
#ifndef _OUICHEFS_H
#define _OUICHEFS_H

#include <linux/fs.h>

#define OUICHEFS_MAGIC 0x48434957

#define OUICHEFS_SB_BLOCK_NR 0

#define OUICHEFS_BLOCK_SIZE (1 << 12) /* 4 KiB */
#define OUICHEFS_FILE_MAX_BLOCKS (OUICHEFS_BLOCK_SIZE >> 2)
#define OUICHEFS_MAX_FILESIZE \
	(OUICHEFS_FILE_MAX_BLOCKS * OUICHEFS_BLOCK_SIZE) /* 4 MiB */
#define OUICHEFS_FILENAME_LEN 28
#define OUICHEFS_MAX_SUBFILES 128

/*
 * ouiche_fs partition layout
 *
 * +---------------+
 * |  superblock   |  1 block
 * +---------------+
 * |  inode store  |  sb->nr_istore_blocks blocks
 * +---------------+
 * | ifree bitmap  |  sb->nr_ifree_blocks blocks
 * +---------------+
 * | bfree bitmap  |  sb->nr_bfree_blocks blocks
 * +---------------+
 * |    data       |
 * |      blocks   |  rest of the blocks
 * +---------------+
 *
 */

struct ouichefs_inode {
	__le32 i_mode; /* File mode */
	__le32 i_uid; /* Owner id */
	__le32 i_gid; /* Group id */
	__le32 i_size; /* Size in bytes */
	__le32 i_ctime; /* Inode change time (sec)*/
	__le64 i_nctime; /* Inode change time (nsec) */
	__le32 i_atime; /* Access time (sec) */
	__le64 i_natime; /* Access time (nsec) */
	__le32 i_mtime; /* Modification time (sec) */
	__le64 i_nmtime; /* Modification time (nsec) */
	__le32 i_blocks; /* Block count */
	__le32 i_nlink; /* Hard links count */
	__le32 index_block; /* Block with list of blocks for this file */
};

struct ouichefs_inode_info {
	uint32_t index_block;
	struct inode vfs_inode;
};

#define OUICHEFS_INODES_PER_BLOCK \
	(OUICHEFS_BLOCK_SIZE / sizeof(struct ouichefs_inode))

struct ouichefs_sb_info {
	uint32_t magic; /* Magic number */

	uint32_t nr_blocks; /* Total number of blocks (incl sb & inodes) */
	uint32_t nr_inodes; /* Total number of inodes */

	uint32_t nr_istore_blocks; /* Number of inode store blocks */
	uint32_t nr_ifree_blocks; /* Number of inode free bitmap blocks */
	uint32_t nr_bfree_blocks; /* Number of block free bitmap blocks */

	uint32_t nr_free_inodes; /* Number of free inodes */
	uint32_t nr_free_blocks; /* Number of free blocks */

	unsigned long *ifree_bitmap; /* In-memory free inodes bitmap */
	unsigned long *bfree_bitmap; /* In-memory free blocks bitmap */

	struct mutex inode_bitmap_lock; /* Mutex for inode bitmap */
	struct mutex block_bitmap_lock; /* Mutex for block bitmap */
};

struct ouichefs_file_index_block {
	__le32 blocks[OUICHEFS_FILE_MAX_BLOCKS];
};

struct ouichefs_dir_block {
	struct ouichefs_file {
		__le32 inode;
		char filename[OUICHEFS_FILENAME_LEN];
	} files[OUICHEFS_MAX_SUBFILES];
};

/* superblock functions */
int ouichefs_fill_super(struct super_block *sb, void *data, int silent);

/* inode functions */
int ouichefs_init_inode_cache(void);
void ouichefs_destroy_inode_cache(void);
struct inode *ouichefs_iget(struct super_block *sb, unsigned long ino);

/* file functions */
extern const struct file_operations ouichefs_file_ops;
extern const struct file_operations ouichefs_dir_ops;
extern const struct address_space_operations ouichefs_aops;
int ouichefs_truncate(struct inode *inode);

/* Getters for superblock and inode */
#define OUICHEFS_SB(sb) ((sb)->s_fs_info)
#define OUICHEFS_INODE(inode) \
	(container_of(inode, struct ouichefs_inode_info, vfs_inode))

#endif /* _OUICHEFS_H */
