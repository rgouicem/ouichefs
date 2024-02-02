#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#include "ouichefs.h"
#include "eviction_policy/eviction_policy.h"

int no_op1(struct super_block *sb)
{
	return 0;
}

int no_op2(struct super_block *sb, struct ouichefs_file *file)
{
	return 0;
}

struct ouichefs_eviction_policy wich_default_policy = {
	.name = "wich_default_policy",
	.clean_dir = no_op2,
	.clean_partition = no_op1,
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
