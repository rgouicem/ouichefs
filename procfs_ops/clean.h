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

	struct mount_item *mount_item;
	struct list_head *head = &first.list;

	list_for_each_entry(mount_item, head, list) {
		// pr_info("Mount name: %s\n", mount_item->name);
		// pr_info("same? %d\n", strcmp(mount_item->name, mount_name));

		if (strcmp(mount_item->name, mount_name) == 0) {
			break;
		}
	}

	if (mount_item == NULL || mount_item->dentry == NULL) {
		pr_err("No such mount found\n");
		return -EINVAL;
	}

	// pr_info("Mount found %s\n", mount_item->name);

	struct super_block *sb = mount_item->dentry->d_sb;

	if (sb == NULL) {
		pr_err("No superblock found\n");
		return -EINVAL;
	}

	// check if the superblock is of the ouichefs type
	if (sb->s_magic != OUICHEFS_MAGIC) {
		pr_err("Superblock is not of ouichefs type\n");
		return -EINVAL;
	}

	// pr_info("Superblock found %s\n", sb->s_id);

	current_policy->clean_partition(sb);

	return size;
}

struct proc_ops clean_proc_ops = {
	.proc_write = clean_proc_write,
};
