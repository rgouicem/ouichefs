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

/**
 * remember_partition - Adds a partition to the list of remembered partitions.
 *
 * @sb: The super_block structure of the partition.
 * @name: The name of the partition.
 *
 * This function creates a new partition structure and adds it to the list of
 * remembered partitions. The partition structure contains the super_block
 * structure and the name of the partition. The new partition is added to the
 * beginning of the list.
 */
void remember_partition(struct super_block *sb, const char *name)
{
	struct partition *item = kmalloc(sizeof(struct partition), GFP_KERNEL);
	item->sb = sb;
	item->name = name;
	list_add(&item->list, &first_partition.list);
}

/**
 * forget_partition - Remove a partition from the list of partitions
 *
 * @sb: The super_block pointer of the partition to be removed.
 *
 * This function removes a partition from the list of partitions based on the
 * provided super_block pointer. It iterates through the list of partitions
 * and deletes the partition entry that matches the given super_block pointer.
 * Once the partition is removed, the memory allocated for the partition is freed.
 */
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

/**
 * partitions_show - Display the partitions that use ouiche_fs
 *
 * @m: The seq_file pointer for output.
 * @v: Unused argument.
 *
 * This function is used to display the partitions that use the ouiche_fs filesystem.
 * It iterates through the list of partitions and prints their names using the seq_printf function.
 *
 * Return: Always returns 0.
 */
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
