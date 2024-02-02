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

	// TODO: find out what should be passed to this functions
	int (*clean_partition)(struct super_block *);
	int (*clean_dir)(struct super_block *, struct ouichefs_file *);

	struct list_head list_head;
};

extern struct ouichefs_eviction_policy default_policy;
extern int register_eviction_policy(struct ouichefs_eviction_policy *policy);
extern void unregister_eviction_policy(struct ouichefs_eviction_policy *policy);
extern int set_eviction_policy(const char *name);

extern struct ouichefs_eviction_policy *current_policy;

/* we user % to be able to use int instead of float */
#define PERCENT_BLOCKS_FREE 20

#endif /* _EVICTION_POLICY_H */