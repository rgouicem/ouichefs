// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kobject.h>

#include "ouichefs.h"

/*
 * struct related to all snapshots for a particular partition
 * kobject: the directory
 * other params: potential snapshot attributes, e.g. tree struct of snapshot(?)
 */
struct ouichefs_snapshot_store_obj {
	struct kobject kobj;
	uuid_t id;
	/* Todo */
};
#define to_ouichefs_snapshot_store_obj(x) \
	container_of(x, struct ouichefs_snapshot_store_obj, kobj)

struct ouichefs_snapshot_store_attribute {
	struct attribute attr;
	ssize_t (*show)(struct ouichefs_snapshot_store_obj *obj,
		struct ouichefs_snapshot_store_attribute *attr, char *buf);
	ssize_t (*store)(struct ouichefs_snapshot_store_obj *onj,
		struct ouichefs_snapshot_store_attribute *attr, const char *buf, size_t count);
};
#define to_ouichefs_snapshot_store_attr(x) \
	container_of(x, struct ouichefs_snapshot_store_attribute, attr)

static ssize_t ouichefs_snapshot_store_attr_show(
	struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct ouichefs_snapshot_store_attribute *attribute;
	struct ouichefs_snapshot_store_obj *snapshot_store;

	attribute = to_ouichefs_snapshot_store_attr(attr);
	snapshot_store = to_ouichefs_snapshot_store_obj(kobj);

	if (!attribute->show) {
		pr_err("read not supported!\n");
		return 0;
	}

	return attribute->show(snapshot_store, attribute, buf);
}

static ssize_t ouichefs_snapshot_store_attr_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t len)
{
	struct ouichefs_snapshot_store_attribute *attribute;
	struct ouichefs_snapshot_store_obj *snapshot_store;

	attribute = to_ouichefs_snapshot_store_attr(attr);
	snapshot_store = to_ouichefs_snapshot_store_obj(kobj);

	if (!attribute->store) {
		pr_err("write not supported!\n");
		return 0;
	}

	return attribute->store(snapshot_store, attribute, buf, len);
}

static const struct sysfs_ops ouichefs_snapshot_store_sysfs_ops = {
	.show = ouichefs_snapshot_store_attr_show,
	.store = ouichefs_snapshot_store_attr_store,
};

static void ouichefs_snapshot_store_release(struct kobject *kobj)
{
	struct ouichefs_snapshot_store_obj *snapshot_store;

	snapshot_store = to_ouichefs_snapshot_store_obj(kobj);
	kfree(snapshot_store);
}

/*
* Assign a unique ID to the snapshot and save the current state of the partition
*/
static ssize_t create_store(struct ouichefs_snapshot_store_obj *obj,
	struct ouichefs_snapshot_store_attribute *attr, const char *buf, size_t count)
{
	/* Todo */
	return count;
}

/*
* Permanently delete a snapshot from the list
*/
static ssize_t destroy_store(struct ouichefs_snapshot_store_obj *obj,
	struct ouichefs_snapshot_store_attribute *attr, const char *buf, size_t count)
{
	/* Todo */
	return count;
}

/*
* List all the snapshots previously saved
* Print the unique ID and the creation date
*/
static ssize_t list_show(struct ouichefs_snapshot_store_obj *obj,
	struct ouichefs_snapshot_store_attribute *attr, char *buf)
{
	/* Todo */
	char *test_msg = "List works!";

	return sysfs_emit(buf, "%s\n", test_msg);
}

/*
* Roll back the partition to the state it was at the time of the snapshot
*/
static ssize_t restore_store(struct ouichefs_snapshot_store_obj *obj,
	struct ouichefs_snapshot_store_attribute *attr, const char *buf, size_t count)
{
	/* Todo */
	return count;
}

static struct ouichefs_snapshot_store_attribute create_attribute = __ATTR_WO(create);
static struct ouichefs_snapshot_store_attribute destroy_attribute = __ATTR_WO(destroy);
static struct ouichefs_snapshot_store_attribute list_attribute = __ATTR_RO(list);
static struct ouichefs_snapshot_store_attribute restore_attribute = __ATTR_WO(restore);

static struct attribute *ouichefs_snapshot_store_default_attrs[] = {
	&create_attribute.attr,
	&destroy_attribute.attr,
	&list_attribute.attr,
	&restore_attribute.attr,
	NULL
};
ATTRIBUTE_GROUPS(ouichefs_snapshot_store_default);

static const struct kobj_type ouichefs_snapshot_store_ktype = {
	.sysfs_ops = &ouichefs_snapshot_store_sysfs_ops,
	.release = ouichefs_snapshot_store_release,
	.default_groups = ouichefs_snapshot_store_default_groups,
};

/*
* Create a directory to manage snapshots for a specific partition
*/
struct ouichefs_snapshot_store_obj *create_snapshot_store_obj(
	const char *name, struct kset *ouichefs_kset, uuid_t id)
{
	struct ouichefs_snapshot_store_obj *snapshot_store;
	int retval;

	snapshot_store = kzalloc(sizeof(struct ouichefs_snapshot_store_obj), GFP_KERNEL);
	if (!snapshot_store)
		return NULL;

	snapshot_store->kobj.kset = ouichefs_kset;
	snapshot_store->id = id;
	/* Todo: link additional attributes */

	retval = kobject_init_and_add(&snapshot_store->kobj,
		&ouichefs_snapshot_store_ktype, NULL, "%s", name);
	if (retval) {
		kobject_put(&snapshot_store->kobj);
		return NULL;
	}

	kobject_uevent(&snapshot_store->kobj, KOBJ_ADD);

	return snapshot_store;
}

/*
* Destroy the directory for a specific partition
*/
void destroy_snapshot_store_obj(struct super_block *sb,
	struct kset *ouichefs_kset)
{
	struct kobject *kobj;
	struct ouichefs_snapshot_store_obj *snapshot_store;

	list_for_each_entry(kobj, &ouichefs_kset->list, entry) {
		snapshot_store = to_ouichefs_snapshot_store_obj(kobj);
		if (memcmp(sb->s_uuid.b, snapshot_store->id.b, UUID_SIZE) == 0) {
			pr_info("Matched snapshot fount");
			kobject_put(&snapshot_store->kobj);
			break;
		}
	}
}