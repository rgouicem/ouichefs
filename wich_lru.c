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

static int clean_dir(struct super_block *sb, struct inode *parent,
		     struct ouichefs_file *files)
{
	struct inode *child = NULL;
	struct ouichefs_file *child_f = NULL;

	struct inode *inode = NULL;
	struct ouichefs_file *f = NULL;

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
