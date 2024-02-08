#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/buffer_head.h>

#include "eviction_policy.h"

int clean_partition_placeholder(struct super_block *sb)
{
	pr_info("clean_partition_placeholder called\n");
	pr_info("got superblock: %s\n", sb->s_id);

	return 0;
}

int clean_dir_placeholder(struct super_block *sb, struct inode *parent,
			  struct ouichefs_file *files)
{
	pr_info("clean_dir_placeholder called\n");
	pr_info("got superblock: %s\n", sb->s_id);

	return 0;
}

struct ouichefs_eviction_policy default_policy = {
	.name = "default",
	.clean_partition = clean_partition_placeholder,
	.clean_dir = clean_dir_placeholder,
	.list_head = LIST_HEAD_INIT(default_policy.list_head),
};

struct ouichefs_eviction_policy *current_policy = &default_policy;

/*
 * Allows another module to register an eviction policy handler used by 
 * ouichefs to free up space on the disk.
 */
int register_eviction_policy(struct ouichefs_eviction_policy *policy)
{
	// bail out on NULL pointer
	if (!policy)
		return -EINVAL;

	// check if name has valid length
	if (strlen(policy->name) > POLICY_NAME_LEN) {
		pr_err("policy name too long\n");
		return -EINVAL;
	}

	// we could check if the policy is already registered / if policy with this name already exists

	list_add_tail(&policy->list_head, &default_policy.list_head);

	// change to the new policy after inserting (helpful mostly for development)
	current_policy = policy;

	pr_info("registered eviction policy '%s'\n", policy->name);

	return 0;
}
EXPORT_SYMBOL(register_eviction_policy);

/*
 * Allows another module to unregister an eviction policy handler used by 
 * ouichefs to free up space on the disk.
 */
void unregister_eviction_policy(struct ouichefs_eviction_policy *policy)
{
	// bail out on NULL pointer
	if (!policy)
		return;

	// we don't want to lose the default (and list head)
	if (policy == &default_policy) {
		pr_err("cannot unregister default eviction policy\n");
		return;
	}

	// if current policy is unregistered fallback to default
	if (current_policy == policy) {
		current_policy = &default_policy;
	}

	list_del(&policy->list_head);

	pr_info("unregistered eviction policy '%s'\n", policy->name);
}
EXPORT_SYMBOL(unregister_eviction_policy);

int set_eviction_policy(const char *name)
{
	struct ouichefs_eviction_policy *policy;

	// bail out on NULL pointer
	if (!name)
		return -EINVAL;

	// find policy by name
	list_for_each_entry(policy, &default_policy.list_head, list_head) {
		if (strcmp(policy->name, name) == 0) {
			current_policy = policy;
			pr_info("set eviction policy to '%s'\n", name);
			return 0;
		}
	}

	pr_err("eviction policy '%s' not found\n", name);

	return -EINVAL;
}

void traverse_dir(struct super_block *sb, struct ouichefs_dir_block *dir,
		  struct traverse_node *dir_node,
		  void (*node_action)(struct traverse_node *parent, void *data),
		  void (*leaf_action)(struct traverse_node *parent,
				      struct traverse_node *child, void *data),
		  void *data)
{
	struct ouichefs_file *dir_file = dir_node->file;
	struct inode *dir_inode = dir_node->inode;
	struct ouichefs_file *f = NULL;
	struct inode *inode = NULL;
	struct ouichefs_dir_block *subdir = NULL;

	int indent = 0;

	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &dir->files[i];

		if (!f->inode)
			break;

		inode = ouichefs_iget(sb, f->inode);

		if (S_ISDIR(inode->i_mode)) {
			pr_info("%*s%s/\n", indent, "", f->filename);

			struct buffer_head *bh = sb_bread(
				sb, OUICHEFS_INODE(inode)->index_block);
			if (!bh)
				return;

			subdir = (struct ouichefs_dir_block *)bh->b_data;
			struct traverse_node subdir_node = {
				.file = f,
				.inode = inode,
			};
			traverse_dir(sb, subdir, &subdir_node, node_action,
				     leaf_action, data);

			brelse(bh);
		} else {
			if (leaf_action) {
				struct traverse_node parent = {
					.file = dir_file,
					.inode = dir_inode,
				};
				struct traverse_node child = {
					.file = f,
					.inode = inode,
				};
				leaf_action(&parent, &child, data);
			}
		}
	}
}
EXPORT_SYMBOL(traverse_dir);