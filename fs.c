// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "ouichefs.h"
#include "eviction_policy/eviction_policy.h"

// procfs operations
#include "procfs_ops/evictions.h"
#include "procfs_ops/partitions.h"
#include "procfs_ops/clean.h"

// MARK: - procfs

static struct proc_dir_entry *dir;

// MARK: - Filesystem operations

/*
 * Mount a ouiche_fs partition
 */
struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	struct dentry *dentry = NULL;

	dentry =
		mount_bdev(fs_type, flags, dev_name, data, ouichefs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	remember_partition(dentry->d_sb, dev_name);

	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	forget_partition(sb);

	kill_block_super(sb);

	pr_info("unmounted disk\n");
}

static struct file_system_type ouichefs_file_system_type = {
	.owner = THIS_MODULE,
	.name = "ouichefs",
	.mount = ouichefs_mount,
	.kill_sb = ouichefs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
	.next = NULL,
};

// MARK: - Module init and exit

static int __init ouichefs_init(void)
{
	int ret;

	ret = ouichefs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto err;
	}

	ret = register_filesystem(&ouichefs_file_system_type);
	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto err_inode;
	}

	dir = proc_mkdir("ouiche", NULL);

	if (!dir) {
		pr_err("Failed to create ouiche procfs directory\n");
		goto err_inode;
	}

	if (!proc_create("eviction", 0, dir, &evictions_proc_ops)) {
		pr_err("Failed to create eviction procfs entry\n");
		goto err_procfs;
	}

	if (!proc_create("partitions", 0, dir, &partitions_proc_ops)) {
		pr_err("Failed to create partitions procfs entry\n");
		goto err_procfs;
	}

	if (!proc_create("clean", 0, dir, &clean_proc_ops)) {
		pr_err("Failed to create clean procfs entry\n");
		goto err_procfs;
	}

	pr_info("module loaded\n");
	return 0;

err_procfs:
	proc_remove(dir);

err_inode:
	ouichefs_destroy_inode_cache();
err:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;

	if (dir)
		proc_remove(dir);

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	ouichefs_destroy_inode_cache();

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mastermakrela & rico_stanosek");
MODULE_DESCRIPTION(
	"ouichefs, a simple educational filesystem for Linux [WS 2023/24 edition]");
