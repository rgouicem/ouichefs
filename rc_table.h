#ifndef _OUICHEFS_RC_TABLE_H
#define _OUICHEFS_TC_TABLE_H

#include "ouichefs.h"

// Increment the reference counter of the block `bno` in `rc_table`.
static inline void inc_ref_count(struct ouichefs_ref_counter *rc_table, uint32_t bno)
{
	if (rc_table[bno].block != bno) {
		pr_err("Bad block number\n");
	}

	rc_table[bno].ref_count++;
}

// Decrement the reference counter of the block `bno` in `rc_table`.
// The caller should be careful not to call this function on a reference
// counter already at zero. If it is the case, this function will do
// nothing and print an error.
static inline void dec_ref_count(struct ouichefs_ref_counter *rc_table, uint32_t bno)
{
	if (rc_table[bno].block != bno) {
		pr_err("Bad block number\n");
	}

	if (rc_table[bno].ref_count >= 1) {
		rc_table[bno].ref_count--;
	} else {
		pr_err("Reference counter of %u already at zero!\n", bno);
	}
}

#endif
