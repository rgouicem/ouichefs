#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"
#include "eviction_policy/eviction_policy.h"

struct print_data {
	int indent;
};

static void node_action_before(struct traverse_node *parent, void *data)
{
	struct print_data *pd = (struct print_data *)data;

	pr_info("%*s%s\n", pd->indent, "", parent->file->filename);

	pd->indent += 4;
}

static void node_action_after(struct traverse_node *parent, void *data)
{
	struct print_data *pd = (struct print_data *)data;
	pd->indent -= 4;
}

static void leaf_action(struct traverse_node *parent,
			struct traverse_node *child, void *data)
{
	struct print_data *pd = (struct print_data *)data;
	pr_info("%*s%s\n", pd->indent, "", child->file->filename);
}

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

	struct print_data pd = {
		.indent = 0,
	};

	struct traverse_node root_node = { .file = NULL, .inode = NULL };

	traverse_dir(sb, dblock, &root_node, node_action_before,
		     node_action_after, leaf_action, &pd);

	brelse(bh);

	return 0;
}

static int clean_dir(struct super_block *sb, struct inode *parent,
		     struct ouichefs_file *files)
{
	pr_info("Contents of the directory\n");

	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		if (!files[i].inode)
			break;

		pr_info("    %s\n", files[i].filename);
	}

	return 0;
}

static struct ouichefs_eviction_policy wich_print_policy = {
	.name = "wich_print",
	.clean_dir = clean_dir,
	.clean_partition = clean_partition,
	.list_head = LIST_HEAD_INIT(wich_print_policy.list_head),
};

static int __init my_module_init(void)
{
	printk(KERN_INFO "Hello from my_module!\n");

	if (register_eviction_policy(&wich_print_policy)) {
		pr_err("register_eviction_policy failed\n");
		return -1;
	}

	return 0;
}
module_init(my_module_init);

static void __exit my_module_exit(void)
{
	unregister_eviction_policy(&wich_print_policy);

	printk(KERN_INFO "Goodbye from my_module!\n");
}
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mastermakrela & rico_stanosek");
MODULE_DESCRIPTION("A simple kernel module");
