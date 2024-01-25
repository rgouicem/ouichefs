/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eviction policy for the ouiche_fs
 *
 * Copyright (C) 2023 mastermakrela & rico_stanosek
 */
#ifndef _EVICTION_POLICY_H
#define _EVICTION_POLICY_H

#include <linux/list.h>

#define POLICY_NAME_LEN 32

struct ouichefs_eviction_policy {
	char name[POLICY_NAME_LEN];

	// TODO: find out what should be passed to this functions
	void (*clean_partition)(void);
	void (*clean_dir)(void);

	struct list_head list_head;
};

extern struct ouichefs_eviction_policy default_policy;
extern int register_eviction_policy(struct ouichefs_eviction_policy *policy);
extern void unregister_eviction_policy(struct ouichefs_eviction_policy *policy);
extern int set_eviction_policy(const char *name);

#endif /* _EVICTION_POLICY_H */