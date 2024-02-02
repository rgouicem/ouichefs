#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

struct mount_item {
	struct dentry *dentry;
	const char *name;
	struct list_head list;
};

struct mount_item first = {
	.dentry = NULL,
	.list = LIST_HEAD_INIT(first.list),
};

static int partitions_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Following partitions use ouiche_fs:\n");

	struct mount_item *item;
	struct list_head *head = &first.list;
	list_for_each_entry(item, head, list) {
		seq_printf(m, "%s\n", item->name);
	}

	return 0;
}

static int partitions_open(struct inode *inode, struct file *file)
{
	return single_open(file, partitions_show, NULL);
}

struct proc_ops partitions_proc_ops = {
	.proc_open = partitions_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
