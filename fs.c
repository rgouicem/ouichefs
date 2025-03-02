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
#include <linux/dcache.h>

#include "ouichefs.h"

static struct kset *ouichefs_kset;

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

	// Create the sysfs dir for the partition
	char dirname[sizeof(dev_name) + 1];
	const char *slash = strrchr(dev_name, '/');

    if (slash != NULL) {
        strncpy(dirname, slash + 1, sizeof(dirname) - 1);
    } else {
		strncpy(dirname, dev_name, sizeof(dirname) - 1);
    }
	dirname[sizeof(dirname) - 1] = '\0';

	if (!create_snapshot_store_obj(dirname, ouichefs_kset)) {
		dentry = ERR_PTR(-ENOMEM);
		pr_err("'%s' mount failure: snapshot store failure\n", dev_name);
	}

	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	kill_block_super(sb);

	// Delete the sysfs dir for the partition
	destroy_snapshot_store_obj(sb, ouichefs_kset);

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

	/* Create ouichefs dir under /sys/fs */
	ouichefs_kset = kset_create_and_add("ouichefs", NULL, fs_kobj);
	if (!ouichefs_kset) {
		pr_err("ouichefs sysfs creation failed\n");
		goto err_sysfs;
	}

	pr_info("module loaded\n");
	return 0;

err_sysfs:
	ouichefs_destroy_inode_cache();
	return -ENOMEM;

err_inode:
	ouichefs_destroy_inode_cache();
err:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	ouichefs_destroy_inode_cache();
	kset_unregister(ouichefs_kset);

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@rwth-aachen.de>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
