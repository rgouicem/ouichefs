#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "../eviction_policy/eviction_policy.h"

/**
 * evictions_show - Display the available eviction policies
 *
 * @m: Pointer to the seq_file structure.
 * @v: Unused parameter.
 *
 * This function is used to display the available eviction policies.
 * It prints the names of the eviction policies, indicating whether
 * they are active or not.
 *
 * Return: Always returns 0
 */
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

/**
 * evictions_proc_write - Write callback function for the evictions_proc file
 *
 * @s: Pointer to the file structure.
 * @buf: Pointer to the user buffer containing the data to be written.
 * @size: Size of the data to be written.
 * @ppos: Pointer to the file position.
 *
 * This function is the write callback for the evictions_proc file. It expects a
 * policy name as input and sets the eviction policy accordingly. The maximum
 * allowed size for the policy name is defined by POLICY_NAME_LEN. If the input
 * policy name exceeds this limit, an error is returned. The function copies the
 * policy name from the user buffer to the input_buffer and then calls the
 * set_eviction_policy() function to set the eviction policy. If the policy name
 * is successfully set, the function returns the size of the data written.
 *
 * Return: On success, the size of the data written. On failure, a negative error code.
 */
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
