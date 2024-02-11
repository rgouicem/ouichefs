#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

struct partition {
	const char *name;
	struct super_block *sb;
	struct list_head list;
};

static struct partition first_partition = {
	.name = NULL,
	.sb = NULL,
	.list = LIST_HEAD_INIT(first_partition.list),
};

void remember_partition(struct super_block *sb, const char *name)
{
	struct partition *item = kmalloc(sizeof(struct partition), GFP_KERNEL);
	item->sb = sb;
	item->name = name;
	list_add(&item->list, &first_partition.list);
}

void forget_partition(struct super_block *sb)
{
	struct partition *item;
	struct list_head *head = &first_partition.list;
	list_for_each_entry(item, head, list) {
		if (item->sb == sb) {
			list_del(&item->list);
			kfree(item);
			break;
		}
	}
}

// MARK: - procfs

static int partitions_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Following partitions use ouiche_fs:\n");

	struct partition *item;
	struct list_head *head = &first_partition.list;
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
