#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "../eviction_policy/eviction_policy.h"

static int evictions_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Following eviction policies are available:\n");

	struct ouichefs_eviction_policy *policy;

	if (current_policy == &default_policy) {
		seq_printf(m, "default (does nothing)\t[ACTIVE]\n");
	} else {
		seq_printf(m, "default (does nothing)\n");
	}

	list_for_each_entry(policy, &default_policy.list_head, list_head) {
		if (policy == current_policy)
			seq_printf(m, "%s\t[ACTIVE]\n", policy->name);
		else
			seq_printf(m, "%s\n", policy->name);
	}

	return 0;
}

static int evictions_open(struct inode *inode, struct file *file)
{
	return single_open(file, evictions_show, NULL);
}

static ssize_t evictions_proc_write(struct file *s, const char __user *buf,
				    size_t size, loff_t *ppos)
{
	// we expect a policy name, so we know the maximum size
	char input_buffer[POLICY_NAME_LEN];

	if (size > POLICY_NAME_LEN) {
		pr_err("Policy name too long. Maximum length is %d\n",
		       POLICY_NAME_LEN);
		return -EINVAL;
	}

	if (copy_from_user(input_buffer, buf, size))
		return -EFAULT;

	pr_info("Received policy name: %s\n", input_buffer);

	if (set_eviction_policy(input_buffer))
		return -EINVAL;

	return size;
}

struct proc_ops evictions_proc_ops = {
	.proc_open = evictions_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = evictions_proc_write,
};
