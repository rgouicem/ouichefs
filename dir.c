// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */
#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"

/*
 * Iterate over the files contained in dir and commit them in ctx.
 * This function is called by the VFS while ctx->pos changes.
 * Return 0 on success.
 */
static int ouichefs_iterate(struct file *dir, struct dir_context *ctx)
{
	struct inode *inode = file_inode(dir);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL;
	struct ouichefs_dir_block *dblock = NULL;
	struct ouichefs_file *f = NULL;
	int i;

	/* Check that dir is a directory */
	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	/*
	 * Check that ctx->pos is not bigger than what we can handle (including
	 * . and ..)
	 */
	if (ctx->pos > OUICHEFS_MAX_SUBFILES + 2)
		return 0;

	/* Commit . and .. to ctx */
	if (!dir_emit_dots(dir, ctx))
		return 0;

	/* Read the directory index block on disk */
	bh = sb_bread(sb, ci->index_block);
	if (!bh)
		return -EIO;
	dblock = (struct ouichefs_dir_block *)bh->b_data;

	/* Iterate over the index block and commit subfiles */
	for (i = ctx->pos - 2; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &dblock->files[i];
		if (!f->inode)
			break;
		if (!dir_emit(ctx, f->filename, OUICHEFS_FILENAME_LEN,
				le32_to_cpu(f->inode), DT_UNKNOWN))
			break;
		ctx->pos++;
	}

	brelse(bh);

	return 0;
}

const struct file_operations ouichefs_dir_ops = {
	.owner = THIS_MODULE,
	.iterate_shared = ouichefs_iterate,
	.fsync = generic_file_fsync,
};
