#include <linux/module.h>
#include <linux/kernel.h>

#include "eviction_policy.h"

int clean_dir_placeholder(struct super_block *sb, struct ouichefs_file *files)
{
	pr_info("clean_dir_placeholder called\n");
	pr_info("got superblock: %s\n", sb->s_id);

	return 0;
}

int clean_partition_placeholder(struct super_block *sb)
{
	pr_info("clean_partition_placeholder called\n");
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
