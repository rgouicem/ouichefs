/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eviction policy for the ouiche_fs
 *
 * Copyright (C) 2023 mastermakrela & rico_stanosek
 */
#ifndef _EVICTION_POLICY_H
#define _EVICTION_POLICY_H

#include <linux/list.h>
#include "../ouichefs.h"

#define POLICY_NAME_LEN 32

struct ouichefs_eviction_policy {
	char name[POLICY_NAME_LEN];

	/**
	 * This function should go through the whole partition and free some 
	 * blocks based on it's policy.
	 * It gets the super block of the partition to clean, because there can
	 * be multiple partitions, using this filesystem, mounted.
	 */
	int (*clean_partition)(struct super_block *);

	/**
	 * This function is called when during file/directory creation there is
	 * not more place lest in the directory.
	 * It should try to free place in given directory, but can return error
	 * if it's not possible.
	 */
	int (*clean_dir)(struct super_block *sb, struct inode *parent,
			 struct ouichefs_file *files);

	struct list_head list_head;
};

extern struct ouichefs_eviction_policy default_policy;
int register_eviction_policy(struct ouichefs_eviction_policy *policy);
void unregister_eviction_policy(struct ouichefs_eviction_policy *policy);
int set_eviction_policy(const char *name);

extern struct ouichefs_eviction_policy *current_policy;

// helpers for traversing the filesystem

struct traverse_node {
	struct ouichefs_file *file;
	struct inode *inode;
};

void traverse_dir(struct super_block *sb, struct ouichefs_dir_block *dir,
		  struct traverse_node *dir_node,
		  void (*node_action_before)(struct traverse_node *parent,
					     void *data),
		  void (*node_action_after)(struct traverse_node *parent,
					    void *data),
		  void (*leaf_action)(struct traverse_node *parent,
				      struct traverse_node *child, void *data),
		  void *data);

/* we user % to be able to use int instead of float */
#define PERCENT_BLOCKS_FREE 20

#endif /* _EVICTION_POLICY_H */