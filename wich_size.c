#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"
#include "eviction_policy/eviction_policy.h"

struct size_data {
	struct inode *parent;
	struct inode *child;
};

/**
 * leaf_action - Perform an action on a leaf node during traversal
 *
 * @parent: The parent node of the leaf node.
 * @child: The leaf node to perform the action on.
 * @data: The data associated with the traversal.
 *
 * This function is called during traversal of a file system tree on a leaf node.
 * It performs an action on the leaf node, such as updating the biggest file information.
 */
static void leaf_action(struct traverse_node *parent,
			struct traverse_node *child, void *data)
{
	struct size_data *d = (struct size_data *)data;

	if (d->child == NULL) {
		d->parent = parent->inode;
		d->child = child->inode;
		pr_info("New biggest file is: %s in directory: %s\n",
			child->file->filename, parent->inode->i_sb->s_id);
		return;
	}

	if (child->inode->i_size < d->child->i_size) {
		d->parent = parent->inode;
		d->child = child->inode;

		pr_info("New biggest file is: %s in directory: %s\n",
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

	struct size_data d = { .child = NULL, .parent = NULL };

	struct traverse_node root_node = { .file = NULL, .inode = NULL };

	traverse_dir(sb, dblock, &root_node, NULL, NULL, leaf_action, &d);

	brelse(bh);

	pr_info("Removing file: %s in directory: %s\n", d.child->i_sb->s_id,
		d.parent->i_sb->s_id);

	ouichefs_remove(d.parent, d.child);

	return 0;
}

/**
 * clean_dir - Clean a directory by removing the biggest file
 *
 * @sb: The super block of the file system.
 * @parent: The parent inode of the directory.
 * @files: Array of ouichefs_file structures representing the files in the directory.
 *
 * This function cleans a directory by removing the file with the biggest size.
 * It iterates through the files in the directory, finds the file with the biggest size,
 * and removes it from the file system.
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

	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &(files[i]);

		if (!f->inode)
			break;

		inode = ouichefs_iget(sb, f->inode);

		if (S_ISDIR(inode->i_mode))
			goto cont;

		if (!child) {
			child = inode;
			child_f = f;
			goto cont;
		}

		if (inode->i_size < child->i_size) {
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

static struct ouichefs_eviction_policy wich_size_policy = {
	.name = "wich_size",
	.clean_dir = clean_dir,
	.clean_partition = clean_partition,
	.list_head = LIST_HEAD_INIT(wich_size_policy.list_head),
};

static int __init my_module_init(void)
{
	pr_info("Registering size based eviction policy!\n");

	if (register_eviction_policy(&wich_size_policy)) {
		pr_err("register_eviction_policy failed\n");
		return -1;
	}

	return 0;
}
module_init(my_module_init);

static void __exit my_module_exit(void)
{
	unregister_eviction_policy(&wich_size_policy);

	printk(KERN_INFO "Unregistered size based eviction policy\n");
}
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mastermakrela & rico_stanosek");
MODULE_DESCRIPTION("Size based eviction policy for ouichefs");
