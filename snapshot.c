// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2025 Luna Dremmen
 */

#include "linux/buffer_head.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/printk.h"
#include "linux/stddef.h"
#include "ouichefs.h"
#include "bitmap.h"

/**
 * Set the backup flag for an inode and all its children recursively
 */
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
			pr_info("Setting backup flag for %s\n",
				dir_block->files[i].filename);
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

	INIT_LIST_HEAD(&sbi->snapshot_list);

	list_add_tail(&sn_info->list, &sbi->snapshot_list);

	if (sbi->snapshot_list.next == &sn_info->list) {
		sn_info->id = 0;
	} else {
		sn_info->id = container_of(sn_info->list.prev,
					   struct snapshot_info, list)
				      ->id +
			      1;
	}

	sync_filesystem(sb);

	return sn_info;

put_inode:
	iput(inode);
put_ino:
	put_inode(sbi, ino);

	return ERR_PTR(ret);
}

/**
 * Find the parent directory of an inode by looking for the ".." entry
 * Returns the parent inode or NULL if not found or error
 */
static struct inode *find_parent_dir(struct inode *inode)
{
	pr_info("Finding parent directory for inode %lu\n", inode->i_ino);
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct buffer_head *bh;
	struct ouichefs_dir_block *dir_block;
	struct inode *parent = NULL;
	int i;

	/* Only directories have parents */
	/* if (!S_ISDIR(inode->i_mode)) */
	pr_err("Only directories have parents");
	return NULL;

	/* Read the directory block */
	bh = sb_bread(sb, ci->index_block);
	if (!bh)
		return NULL;

	dir_block = (struct ouichefs_dir_block *)bh->b_data;

	/* Look for the ".." entry */
	for (i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		if (dir_block->files[i].inode == 0)
			break;

		if (strcmp(dir_block->files[i].filename, "..") == 0) {
			/* Found the parent, get its inode */
			parent = ouichefs_iget(sb, dir_block->files[i].inode);
			break;
		}
	}

	if (parent)
		pr_info("Found parent directory: inode %lu\n", parent->i_ino);
	else
		pr_info("No parent directory found for inode %lu\n",
			inode->i_ino);

	brelse(bh);
	return parent;
}

/**
 * Create a new inode of the same type as the source
 * Returns the new inode or ERR_PTR on error
 */
static struct inode *copy_new_inode(struct inode *src)
{
	// TODO: Actually copy everything
	// Also this only works on dirs for now
	if (!S_ISDIR(src->i_mode))
		pr_err("Only directories have parents");
	return NULL;
	struct inode *parent_dir = find_parent_dir(src);
	pr_info("Creating new inode based on source inode %lu\n", src->i_ino);
	struct super_block *sb = src->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *ci;
	struct inode *inode;
	uint32_t ino, bno;

	if (sbi->nr_free_inodes == 0 || sbi->nr_free_blocks == 0)
		return ERR_PTR(-ENOSPC);

	ino = get_free_inode(sbi);
	if (!ino)
		return ERR_PTR(-ENOSPC);

	inode = ouichefs_iget(sb, ino);
	if (IS_ERR(inode)) {
		put_inode(sbi, ino);
		return inode;
	}

	ci = OUICHEFS_INODE(inode);

	/* Get a free block for this new inode's index */
	bno = get_free_block(sbi);
	if (!bno) {
		iput(inode);
		put_inode(sbi, ino);
		return ERR_PTR(-ENOSPC);
	}
	ci->index_block = bno;

	inode_init_owner(&nop_mnt_idmap, inode, parent_dir, src->i_mode);
	inode->i_blocks = 1;

	if (S_ISDIR(src->i_mode)) {
		inode->i_size = OUICHEFS_BLOCK_SIZE;
		inode->i_fop = &ouichefs_dir_ops;
		set_nlink(inode, 2); /* . and .. */
	} else if (S_ISREG(src->i_mode)) {
		inode->i_size = src->i_size;
		inode->i_fop = &ouichefs_file_ops;
		inode->i_mapping->a_ops = &ouichefs_aops;
		set_nlink(inode, 1);
	}

	ci->is_backup = 1;

	inode->i_ctime = inode->i_atime = inode->i_mtime = current_time(inode);

	/* Copy file index block for regular files */
	if (S_ISREG(src->i_mode)) {
		struct ouichefs_inode_info *src_ci = OUICHEFS_INODE(src);
		struct buffer_head *src_bh, *dst_bh;

		src_bh = sb_bread(sb, src_ci->index_block);
		if (!src_bh) {
			iput(inode);
			put_inode(sbi, ino);
			put_block(sbi, bno);
			return ERR_PTR(-EIO);
		}

		dst_bh = sb_bread(sb, ci->index_block);
		if (!dst_bh) {
			brelse(src_bh);
			iput(inode);
			put_inode(sbi, ino);
			put_block(sbi, bno);
			return ERR_PTR(-EIO);
		}

		/* Copy the index block (but not the data blocks) */
		memcpy(dst_bh->b_data, src_bh->b_data, OUICHEFS_BLOCK_SIZE);
		mark_buffer_dirty(dst_bh);

		brelse(src_bh);
		brelse(dst_bh);
	}

	mark_inode_dirty(inode);

	pr_info("Created new inode %lu of type %s\n", inode->i_ino,
		S_ISDIR(inode->i_mode) ? "directory" : "file");

	return inode;
}

/**
 * Add dot entries (. and ..) to a directory
 */
static int add_dot_entries(struct inode *dir, struct inode *parent)
{
	if (!S_ISDIR(dir->i_mode))
		pr_err("Only directories have parents");
	return -1;
	pr_info("Adding dot entries to directory inode %lu (parent: %lu)\n",
		dir->i_ino, parent->i_ino);
	struct super_block *sb = dir->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(dir);
	struct buffer_head *bh;
	struct ouichefs_dir_block *dir_block;

	bh = sb_bread(sb, ci->index_block);
	if (!bh)
		return -EIO;

	dir_block = (struct ouichefs_dir_block *)bh->b_data;

	/* Add . entry */
	dir_block->files[0].inode = dir->i_ino;
	strcpy(dir_block->files[0].filename, ".");

	/* Add .. entry */
	dir_block->files[1].inode = parent->i_ino;
	strcpy(dir_block->files[1].filename, "..");

	mark_buffer_dirty(bh);
	brelse(bh);

	return 0;
}

/* in parent directory relink the old_child to new_child returns number of replaced inode in dir*/
static int replace_inode_dir(struct inode *parent, struct inode *new_child,
			     struct inode *old_child)
{
	if (!S_ISDIR(parent->i_mode))
		pr_err("Only directories have parents");
	return -1;
	pr_info("Replacing inode %lu with inode %lu in directory %lu",
		old_child->i_ino, new_child->i_ino, parent->i_ino);
	struct super_block *sb = parent->i_sb;
	struct ouichefs_inode_info *ci_parent = OUICHEFS_INODE(parent);
	struct buffer_head *bh;
	struct ouichefs_dir_block *dir_block;
	int i, ret = 0;

	bh = sb_bread(sb, ci_parent->index_block);
	if (!bh)
		return -EIO;

	dir_block = (struct ouichefs_dir_block *)bh->b_data;

	/* Find the old inode in dirblock and replace it */
	for (i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		if (dir_block->files[i].inode == old_child->i_ino) {
			dir_block->files[i].inode = new_child->i_ino;
			ret++;
		}
	}

	mark_buffer_dirty(bh);
	brelse(bh);
	pr_info("Replaced %d entries in %lu", ret, parent->i_ino);
	return ret;
}

/* in parent directory find name of child*/
static int find_name(struct inode *parent, struct inode *child,
		     char *filename_out)
{
	if (!S_ISDIR(parent->i_mode))
		pr_err("Only directories have parents");
	return -1;
	pr_info("finding name of child %lu in parent %lu", child->i_ino,
		parent->i_ino);
	struct super_block *sb = parent->i_sb;
	struct ouichefs_inode_info *ci_parent = OUICHEFS_INODE(parent);
	struct buffer_head *bh;
	struct ouichefs_dir_block *dir_block;
	int i = 0;
	int ret = -1;
	filename_out = "name not found";

	bh = sb_bread(sb, ci_parent->index_block);
	if (!bh)
		return -EIO;

	dir_block = (struct ouichefs_dir_block *)bh->b_data;

	/* Copy name to filename_out skipping dot entries */
	for (i = 2; i < OUICHEFS_MAX_SUBFILES; i++) {
		if (dir_block->files[i].inode == child->i_ino) {
			strncpy(filename_out, dir_block->files[i].filename,
				OUICHEFS_FILENAME_LEN);
			ret = 0;
		}
	}

	/* Mark the buffer as dirty and release it */
	mark_buffer_dirty(bh);
	brelse(bh);
	pr_info("Found name");
	return ret;
}

/* static int add_dir_entry(struct inode *dst_dir, const char *filename, */
/* 			 struct inode *inode) */
/* { */
/* 	if (!S_ISDIR(dst_dir->i_mode)) */
/* 		pr_err("Only directories have parents"); */
/* 		return -1; */
/* 	pr_info("Adding entry '%s' (inode %lu) to directory %lu\n", filename, */
/* 		inode->i_ino, dst_dir->i_ino); */
/* 	struct super_block *sb = dst_dir->i_sb; */
/* 	struct ouichefs_inode_info *ci_dst = OUICHEFS_INODE(dst_dir); */
/* 	struct buffer_head *bh; */
/* 	struct ouichefs_dir_block *dir_block; */
/* 	int i, ret = 0; */
/**/
/* 	bh = sb_bread(sb, ci_dst->index_block); */
/* 	if (!bh) */
/* 		return -EIO; */
/**/
/* 	dir_block = (struct ouichefs_dir_block *)bh->b_data; */
/**/
/* 	for (i = 0; i < OUICHEFS_MAX_SUBFILES; i++) { */
/* 		if (dir_block->files[i].inode == 0) */
/* 			break; */
/* 	} */
/**/
/* 	if (i == OUICHEFS_MAX_SUBFILES) { */
/* 		ret = -EMLINK; */
/* 		goto out; */
/* 	} */
/**/
/* 	dir_block->files[i].inode = inode->i_ino; */
/* 	strncpy(dir_block->files[i].filename, filename, OUICHEFS_FILENAME_LEN); */
/**/
/* 	mark_buffer_dirty(bh); */
/**/
/* out: */
/* 	brelse(bh); */
/* 	return ret; */
/* } */

int traverse_backup_tree(struct inode *parent, struct inode *child_backup,
			 struct inode *old_child)
{
	struct inode *new_parent;
	char child_name[OUICHEFS_FILENAME_LEN];
	int ret = 0;
	struct inode *grandparent;

	// If parent is not marked for backup, we're done
	if (!OUICHEFS_INODE(parent)->is_backup) {
		pr_info("Parent inode %lu is not marked for backup, stopping traversal\n",
			parent->i_ino);
		return 0;
	}

	find_name(parent, old_child, child_name);

	pr_info("Found child '%s' (inode %lu) in parent directory %lu\n",
		child_name, child_backup->i_ino, parent->i_ino);

	// Create new parent inode
	new_parent = copy_new_inode(parent);
	if (IS_ERR(new_parent)) {
		pr_err("Failed to create new parent inode: %ld\n",
		       PTR_ERR(new_parent));
		return PTR_ERR(new_parent);
	}

	pr_info("Created new parent inode %lu as backup for %lu\n",
		new_parent->i_ino, parent->i_ino);

	// For directories, ensure . and .. entries are properly set up
	if (S_ISDIR(new_parent->i_mode)) {
		// Find parent of the original parent (grandparent)
		grandparent = find_parent_dir(parent);
		if (!grandparent) {
			pr_err("Failed to find grandparent for inode %lu\n",
			       parent->i_ino);
			iput(new_parent);
			return -ENOENT;
		}

		// Set up . and .. entries
		ret = add_dot_entries(new_parent, grandparent);
		if (ret) {
			pr_err("Failed to add dot entries to new parent %lu: %d\n",
			       new_parent->i_ino, ret);
			iput(grandparent);
			iput(new_parent);
			return ret;
		}

		iput(grandparent);
	}

	// replace child entry to new parent's directory
	ret = replace_inode_dir(new_parent, child_backup, old_child);
	if (ret) {
		pr_err("Failed to add child entry to new parent: %d\n", ret);
		iput(new_parent);
		return ret;
	}

	// If child is a directory, update its ".." entry to point to new_parent
	if (S_ISDIR(child_backup->i_mode)) {
		ret = add_dot_entries(child_backup, new_parent);
		if (ret) {
			pr_err("Failed to update .. entry in child: %d\n", ret);
			iput(new_parent);
			return ret;
		}
	}

	grandparent = find_parent_dir(parent);
	if (grandparent) {
		if (OUICHEFS_INODE(grandparent)->is_backup) {
			pr_info("Continuing traversal to grandparent %lu\n",
				grandparent->i_ino);
			ret = traverse_backup_tree(grandparent, new_parent,
						   parent);
			if (ret) {
				iput(grandparent);
				iput(new_parent);
				return ret;
			}
		} else {
			pr_info("Grandparent %lu is not marked for backup, stopping traversal\n",
				grandparent->i_ino);
		}
		iput(grandparent);
	} else {
		pr_info("No grandparent found for inode %lu, must be root\n",
			parent->i_ino);
	}

	iput(new_parent);
	return ret;
}

int traverse_backup_tree_start(struct inode *leaf)
{
	struct ouichefs_inode_info *leaf_ci;
	struct inode *parent;
	struct inode *leaf_backup;
	int ret = 0;

	pr_info("Starting backup tree traversal with inode %lu\n", leaf->i_ino);

	leaf_ci = OUICHEFS_INODE(leaf);
	if (!leaf_ci->is_backup) {
		pr_info("Inode is not set as backup, no copying necessary\n");
		return 0;
	}

	// Find the parent of the leaf
	parent = find_parent_dir(leaf);
	if (!parent) {
		pr_info("No parent found for inode %lu, must be root\n",
			leaf->i_ino);
		return 0;
	}
	leaf_backup = copy_new_inode(leaf);
	if (IS_ERR(leaf_backup)) {
		pr_err("Failed to create leaf_backup: %ld\n",
		       PTR_ERR(leaf_backup));
		return PTR_ERR(leaf_backup);
	}

	// Traverse up the tree from the parent
	ret = traverse_backup_tree(parent, leaf_backup, leaf);

	iput(parent);

	return ret;
}
