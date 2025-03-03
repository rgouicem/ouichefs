// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2025 Luna Dremmen
 */

#include "linux/buffer_head.h"
#include "linux/container_of.h"
#include "linux/fs.h"
#include "linux/gfp_types.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "ouichefs.h"
#include "bitmap.h"


void set_backup_flags(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);

	if (!ci)
		return;

	ci->is_backup = true;

	// Mark the inode as dirty to ensure it gets written back to disk
	mark_inode_dirty(inode);

	if (S_ISDIR(inode->i_mode)) {
		struct buffer_head *dir_bh;
		struct ouichefs_dir_block *dir_block;
		int i;

		dir_bh = sb_bread(sb, ci->index_block);
		if (!dir_bh)
			return;

		dir_block = (struct ouichefs_dir_block *)dir_bh->b_data;

		for (i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
			struct inode *child_inode;

			if (dir_block->files[i].inode == 0)
				break;

			// Skip . and .. entries to avoid loops
			if (dir_block->files[i].inode == inode->i_ino ||
			    strcmp(dir_block->files[i].filename, ".") == 0 ||
			    strcmp(dir_block->files[i].filename, "..") == 0)
				continue;

			child_inode =
				ouichefs_iget(sb, dir_block->files[i].inode);
			pr_info("Setting backup flag for %s", dir_block-> files[i].filename);
			if (IS_ERR(child_inode))
				continue;

			set_backup_flags(child_inode);

			iput(child_inode);
		}

		brelse(dir_bh);
	}
}

struct snapshot_info *create_snapshot(struct super_block *sb)
{
	// TODO: USE A OUICHEFS BLOCK TO STORE SN_INFO
	struct snapshot_info *sn_info =
		kmalloc(sizeof(struct snapshot_info), GFP_KERNEL);

	if (sn_info == NULL) {
		return ERR_PTR(-ENOMEM);
	}

	struct inode *inode;
	struct ouichefs_inode_info *ci;
	struct ouichefs_sb_info *sbi;
	uint32_t ino, bno;
	int ret;

	/* Check if inodes are available */
	sbi = OUICHEFS_SB(sb);
	if (sbi->nr_free_inodes == 0 || sbi->nr_free_blocks == 0)
		return ERR_PTR(-ENOSPC);

	/* Get a new free inode */
	ino = get_free_inode(sbi);
	if (!ino)
		return ERR_PTR(-ENOSPC);
	inode = ouichefs_iget(sb, ino);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		goto put_ino;
	}
	ci = OUICHEFS_INODE(inode);

	/* Get a free block for this new inode's index */
	bno = get_free_block(sbi);
	if (!bno) {
		ret = -ENOSPC;
		goto put_inode;
	}
	ci->index_block = bno;

	/* Initialize inode */
	inode_init_owner(&nop_mnt_idmap, inode, NULL, S_IFDIR);
	inode->i_blocks = 1;
	inode->i_size = OUICHEFS_BLOCK_SIZE;
	inode->i_fop = &ouichefs_dir_ops;
	set_nlink(inode, 2);

	inode->i_ctime = inode->i_atime = inode->i_mtime = current_time(inode);

	// TODO ERROR HANDLING

	freeze_super(sb);

	struct inode *root_inode = ouichefs_iget(sb, 1);
	struct ouichefs_inode_info *root_ci;
	root_ci = OUICHEFS_INODE(root_inode);
	struct buffer_head *root_bh = sb_bread(sb, root_ci->index_block);
	struct ouichefs_file_index_block *root_index =
		(struct ouichefs_file_index_block *)root_bh->b_data;

	struct buffer_head *snapshot_bh = sb_bread(sb, ci->index_block);
	struct ouichefs_file_index_block *snapshot_index =
		(struct ouichefs_file_index_block *)snapshot_bh->b_data;

	memcpy(snapshot_index, root_index, OUICHEFS_BLOCK_SIZE);

	set_backup_flags(root_inode);

	thaw_super(sb);

	sn_info->inode = inode;
	sn_info->timestamp = current_time(inode).tv_sec;

	list_add_tail(&sn_info->list, &sbi->snapshot_list);

	if (sbi->snapshot_list.next == &sn_info->list) {
		sn_info->id = 0;
	} else {
		sn_info->id = container_of(sn_info->list.prev,
					   struct snapshot_info, list)
				      ->id +
			      1;
	}
	return sn_info;

put_inode:
	iput(inode);
put_ino:
	put_inode(sbi, ino);

	return ERR_PTR(ret);
}

