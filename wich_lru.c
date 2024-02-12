#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"
#include "eviction_policy/eviction_policy.h"

#define ACCESS 1
#define MODIFICATION 2
#define CHANGE 3

static int mode = CHANGE;
module_param(mode, int, 0);
MODULE_PARM_DESC(mode, "Eviction policy mode");

struct lru_data {
	struct inode *parent;
	struct inode *child;
};

/**
 * is_older - Compares the timestamps of two inodes to determine which one is older.
 * @inode1: Pointer to the first inode.
 * @inode2: Pointer to the second inode.
 *
 * This function compares the timestamps of two inodes based on the specified mode
 * (access, modification, or change) and returns a value indicating which inode is older.
 *
 * Return: A negative value if inode1 is older, a positive value if inode2 is older,
 *         and zero if the timestamps are equal.
 */
static int is_older(struct inode *inode1, struct inode *inode2)
{
	switch (mode) {
	case ACCESS:
		return timespec64_compare(&inode1->i_atime, &inode2->i_atime);
	case MODIFICATION:
		return timespec64_compare(&inode1->i_mtime, &inode2->i_mtime);
	case CHANGE:
	default:
		return timespec64_compare(&inode1->i_ctime, &inode2->i_ctime);
	};
}

/**
 * leaf_action - Perform an action on a leaf node during traversal
 * 
 * @parent: The parent node of the leaf node.
 * @child: The leaf node to perform the action on.
 * @data: The data associated with the traversal.
 *
 * This function is called during traversal of a tree structure to perform an
 * action on a leaf node. It updates the oldest file information in the lru_data
 * structure based on the child node's inode. If the child node is older than the
 * current oldest file, the parent and child inodes are updated and a log message
 * is printed.
 */
static void leaf_action(struct traverse_node *parent,
			struct traverse_node *child, void *data)
{
	struct lru_data *d = (struct lru_data *)data;

	if (d->child == NULL) {
		d->parent = parent->inode;
		d->child = child->inode;
		pr_info("New oldest file is: %s in directory: %s\n",
			child->file->filename, parent->inode->i_sb->s_id);
		return;
	}

	if (is_older(child->inode, d->child)) {
		d->parent = parent->inode;
		d->child = child->inode;

		pr_info("New oldest file is: %s in directory: %s\n",
			child->file->filename, parent->inode->i_sb->s_id);
	}
}

/**
 * clean_partition - Cleans the partition by removing a file from a directory.
 * 
 * @sb: The super_block structure pointer.
 *
 * This function is responsible for cleaning the partition by removing a file 
 * from a directory. It reads the directory index block on disk, traverses the 
 * directory structure, and removes the specified file.
 *
 * Return: 0 on success, -EIO on failure.
 */
static int clean_partition(struct super_block *sb)
{
	struct buffer_head *bh = NULL;
	struct ouichefs_dir_block *dblock = NULL;

	struct ouichefs_inode *root_inode = get_root_inode(sb);

	if (!root_inode->index_block)
		return -EIO;

	/* Read the directory index block on disk */
	bh = sb_bread(sb, root_inode->index_block);
	if (!bh)
		return -EIO;
	dblock = (struct ouichefs_dir_block *)bh->b_data;

	struct lru_data d = { .child = NULL, .parent = NULL };

	struct traverse_node root_node = { .file = NULL, .inode = NULL };

	traverse_dir(sb, dblock, &root_node, NULL, NULL, leaf_action, &d);

	brelse(bh);

	pr_info("Removing file: %s in directory: %s\n", d.child->i_sb->s_id,
		d.parent->i_sb->s_id);

	ouichefs_remove(d.parent, d.child);

	return 0;
}
/**
 * clean_dir - Clean a directory by removing the oldest file
 * 
 * @sb: The super block of the file system.
 * @parent: The parent inode of the directory.
 * @files: Array of ouichefs_file structures representing the files in the directory.
 *
 * This function cleans a directory by removing the oldest file in the directory.
 * It iterates through the files in the directory, finds the oldest file, and removes it.
 * If there are no files in the directory, an error message is printed and -1 is returned.
 *
 * Return: 0 on success, -1 if there are no files in the directory
 */
static int clean_dir(struct super_block *sb, struct inode *parent,
		     struct ouichefs_file *files)
{
	struct inode *child = NULL;
	struct ouichefs_file *child_f = NULL;

	struct inode *inode = NULL;
	struct ouichefs_file *f = NULL;

	/* Find oldest file */
	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &(files[i]);

		if (!f)
			break;

		inode = ouichefs_iget(sb, f->inode);

		if (S_ISDIR(inode->i_mode))
			goto cont;

		if (!child) {
			child = inode;
			child_f = f;
			goto cont;
		}

		if (is_older(inode, child)) {
			child = inode;
			child_f = f;
		}

cont:
		iput(inode);
		continue;
	}

	if (!child) {
		pr_err("No files in directory. Can't free space\n");
		return -1;
	}

	pr_info("Removing file: %s in directory: %s\n", child_f->filename,
		parent->i_sb->s_id);

	ouichefs_remove(parent, child);

	return 0;
}

static struct ouichefs_eviction_policy wich_lru_policy = {
	.name = "wich_lru",
	.clean_dir = clean_dir,
	.clean_partition = clean_partition,
	.list_head = LIST_HEAD_INIT(wich_lru_policy.list_head),
};

static int __init my_module_init(void)
{
	pr_info("Registering LRU eviction policy!\n");
	pr_info("Comparing by: %s\n", mode == ACCESS ? "access time" :
				      mode == MODIFICATION ?
						       "modification time" :
						       "change time");
	pr_info("if you want to change the mode, reinsert the module with the new mode\n");

	if (register_eviction_policy(&wich_lru_policy)) {
		pr_err("register_eviction_policy failed\n");
		return -1;
	}

	return 0;
}
module_init(my_module_init);

static void __exit my_module_exit(void)
{
	unregister_eviction_policy(&wich_lru_policy);

	printk(KERN_INFO "Unregistered LRU eviction policy\n");
}
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mastermakrela & rico_stanosek");
MODULE_DESCRIPTION("LRU eviction policy for ouichefs");
