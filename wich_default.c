#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"
#include "eviction_policy/eviction_policy.h"

struct print_data {
	int indent;
};

void node_action(struct traverse_node *parent, void *data)
{
	struct print_data *pd = (struct print_data *)data;
	pd->indent += 2;
}

void leaf_action(struct traverse_node *parent, struct traverse_node *child,
		 void *data)
{
	struct print_data *pd = (struct print_data *)data;
	pr_info("%*s%s\n", pd->indent, "", child->file->filename);
}

int clean_partition(struct super_block *sb)
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

	struct print_data pd = {
		.indent = 0,
	};

	struct traverse_node root_node = { .file = NULL, .inode = NULL };

	traverse_dir(sb, dblock, &root_node, node_action, leaf_action, &pd);

	return 0;
}

int clean_dir(struct super_block *sb, struct inode *parent,
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
			continue;

		if (!child) {
			child = inode;
			child_f = f;
			continue;
		}

		if (child->i_size < inode->i_size) {
			child = inode;
			child_f = f;
		}
	}

	if (!child) {
		pr_err("No files in directory. Can't free space\n");
		return -1;
	}

	pr_info("Removing file: %s in directory: %s\n", child_f->filename,
		parent->i_sb->s_id);

	return 0;
}

struct ouichefs_eviction_policy wich_default_policy = {
	.name = "wich_default_policy",
	.clean_dir = clean_dir,
	.clean_partition = clean_partition,
	.list_head = LIST_HEAD_INIT(wich_default_policy.list_head),
};

static int __init my_module_init(void)
{
	printk(KERN_INFO "Hello from my_module!\n");

	if (register_eviction_policy(&wich_default_policy)) {
		pr_err("register_eviction_policy failed\n");
		return -1;
	}

	return 0;
}

static void __exit my_module_exit(void)
{
	unregister_eviction_policy(&wich_default_policy);

	printk(KERN_INFO "Goodbye from my_module!\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mastermakrela & rico_stanosek");
MODULE_DESCRIPTION("A simple kernel module");
