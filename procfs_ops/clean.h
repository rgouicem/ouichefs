#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static ssize_t clean_proc_write(struct file *s, const char __user *buf,
				size_t size, loff_t *ppos)
{
	char mount_name[128];

	// get the superblock id from the input
	if (copy_from_user(mount_name, buf, size))
		return -EFAULT;

	// if name is empty print usage
	if (mount_name[0] == '\0') {
		//TODO: figure out why it doesn't work
		pr_info("Usage: provide name of the mount to clean");
		return -EINVAL;
	}

	// pr_info("Received mount name: %s\n", mount_name);

	struct partition *partition;
	struct list_head *head = &first_partition.list;

	list_for_each_entry(partition, head, list) {
		if (strcmp(partition->name, mount_name) == 0) {
			break;
		}
	}

	if (partition == NULL) {
		pr_err("No such partition found\n");
		return -EINVAL;
	}

	struct super_block *sb = partition->sb;

	if (sb == NULL) {
		pr_err("Partition without superblock - this should not happen ¯\\_(ツ)_/¯ \n");
		return -EINVAL;
	}

	if (sb->s_magic != OUICHEFS_MAGIC) {
		pr_err("Partition is not ouichefs - cannot clean\n");
		return -EINVAL;
	}

	current_policy->clean_partition(sb);

	return size;
}

struct proc_ops clean_proc_ops = {
	.proc_write = clean_proc_write,
};
