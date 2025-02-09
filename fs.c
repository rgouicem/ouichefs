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
#include "linux/container_of.h"
#include "linux/kobject.h"

#include "ouichefs.h"

/* 
 * Attributes and their functions.
 */
static ssize_t empty_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return 0;
}

static ssize_t list_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	return sysfs_emit(buf, "Hello World!\n");
}

static ssize_t create_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	return 0;
}

static ssize_t destroy_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	return 0;
}

static ssize_t restore_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	return 0;
}

static ssize_t empty_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	return 0;
}

static struct kobj_attribute create_attr =
	__ATTR(create, 0664, empty_show, create_store);

static struct kobj_attribute destroy_attribute =
	__ATTR(destroy, 0664, empty_show, destroy_store);

static struct kobj_attribute list_attribute =
	__ATTR(list, 0664, list_show, empty_store);

static struct kobj_attribute restore_attribute =
	__ATTR(restore, 0664, empty_show, restore_store);

static struct attribute *ouichefs_default_attrs[] = {
	&create_attr.attr,
	&destroy_attribute.attr,
	&list_attribute.attr,
	&restore_attribute.attr,
	NULL, /* need to NULL terminate the list of attributes */
};
ATTRIBUTE_GROUPS(ouichefs_default);

/*
 * The default show function that must be passed to sysfs.  This will be
 * called by sysfs for whenever a show function is called by the user on a
 * sysfs file associated with the kobjects we have registered.
 */
static ssize_t default_attr_show(struct kobject *kobj, struct attribute *attr,
				 char *buf)
{
	struct kobj_attribute *attribute;

	attribute = container_of(attr, struct kobj_attribute, attr);

	return attribute->show(kobj, attribute, buf);
}

/*
 * Just like the default show function above, but this one is for when the
 * sysfs "store" is requested (when a value is written to a file.)
 */
static ssize_t default_attr_store(struct kobject *kobj, struct attribute *attr,
				  const char *buf, size_t len)
{
	struct kobj_attribute *attribute;

	attribute = container_of(attr, struct kobj_attribute, attr);

	return attribute->store(kobj, attribute, buf, len);
}

/* 
 * Default sysfs_ops that just "unwrap" the kobj_attribute definition.
 * Needed for ouichefs_ktype.
 */
static const struct sysfs_ops default_sysfs_ops = {
	.show = default_attr_show,
	.store = default_attr_store,
};

static void default_release(struct kobject *kobj)
{
	kfree(kobj);
}

static const struct kobj_type ouichefs_ktype = {
	.sysfs_ops = &default_sysfs_ops,
	.release = default_release,
	.default_groups = ouichefs_default_groups,
};

static struct kset *ouichefs_set;

/*
 * Adds a kobject with the provided name.  Implicitly creates
 * the kobj_attributes needed for each partition.
 */
static struct kobject *create_partition_obj(const char *name)
{
	struct kobject *kobj;
	int retval;

	kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);
	if (!kobj)
		return NULL;

	kobj->kset = ouichefs_set;

	/*
	 * Initialize and add the kobject to /sys/fs/ouichefs.
   * We use name + 5 to remove the '/dev/' prefix in the name.
	 */
	retval = kobject_init_and_add(kobj, &ouichefs_ktype, NULL, "%s",
				      name + 5);
	if (retval) {
		kobject_put(kobj);
		return NULL;
	}

	/*
	 * We are always responsible for sending the uevent that the kobject
	 * was added to the system.
	 */
	kobject_uevent(kobj, KOBJ_ADD);

	return kobj;
}

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

	// Create /sys/fs/ouichefs entry
	create_partition_obj(dev_name);

	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	// Find kobj that correlates with the partition we want to unmount.
	// Might fail if anything got renamed!
	struct kobject *kobj = kset_find_obj(ouichefs_set, sb->s_id);
	if (kobj == NULL)
		pr_err("Could not find kobject to delete partition %s",
		       sb->s_id);

	// Unregsiter from hierarchy and delete
	kobject_del(kobj);
	kobject_put(kobj);

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

	// Register /sys/fs/ouichefs/ as a kset
	ouichefs_set = kset_create_and_add("ouichefs", NULL, fs_kobj);
	if (!ouichefs_set)
		goto err_kset;

	pr_info("module loaded\n");
	return 0;

err_kset:
	ret = -ENOMEM;
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

	// Clean up /sys/fs/ouichefs/
	kset_unregister(ouichefs_set);

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@rwth-aachen.de>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
