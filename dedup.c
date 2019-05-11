#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>

#include "ouichefs.h"

static int ouichefs_dedup_scan_directory(struct inode *inode, struct ouichefs_dedup_info *dedup_info);
static int ouichefs_dedup_scan_file(struct inode *inode, struct ouichefs_dedup_info *dedup_info);
static int are_eq_blocks(struct super_block *sb, uint32_t b1, uint32_t b2, loff_t size1, loff_t size2);

int ouichefs_dedup_scan(struct super_block *sb)
{
    int err;

    pr_info("scanning for duplicated blocks\n");

    err = ouichefs_dedup_scan_directory(sb->s_root->d_inode, NULL);
    if (!err)
        return err;

    return 0;
}

static int ouichefs_dedup_scan_directory(struct inode *inode, struct ouichefs_dedup_info *dedup_info)
{
    int i = 0;

    struct ouichefs_dir_block *dir = NULL;
    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh = NULL;

    bh = sb_bread(sb, ci->index_block);
    if (!bh)
        goto io_fail;

    dir = (struct ouichefs_dir_block *)bh->b_data;

    for (i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
        if (dir->files[i].filename[0] != '\0') {
            struct inode *inode = ouichefs_iget(sb, dir->files[i].inode);
            if (IS_ERR(inode))
                goto fail_brelse;

            if (S_ISDIR(inode->i_mode)) {
                if (dedup_info == NULL)
                    pr_info("scanning '%s' subdirectory\n", dir->files[i].filename);

                if (ouichefs_dedup_scan_directory(inode, dedup_info) < 0) {
                    iput(inode);
                    goto fail_brelse;
                }
            } else if (S_ISREG(inode->i_mode)) {
                if (dedup_info == NULL)
                    pr_info("scanning '%s' file\n", dir->files[i].filename);

                if (ouichefs_dedup_scan_file(inode, dedup_info) < 0) {
                    iput(inode);
                    goto fail_brelse;
                }
            } else {
                pr_err("'%s' has an unknown type\n", dir->files[i].filename);
            }

            iput(inode);
        }
    }

    // mark_buffer_dirty(bh);
    // sync_dirty_buffer(bh);
    brelse(bh);

    return 0;

    fail_brelse:
    brelse(bh);
    io_fail:
    pr_err("IO error\n");
    return -EIO;
}

static int ouichefs_dedup_scan_file(struct inode *inode, struct ouichefs_dedup_info *dedup_info)
{
    int i = 0;

    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct ouichefs_file_index_block *index;
    struct buffer_head *bh_index = NULL;

    loff_t remaining_size = inode->i_size;
    loff_t block_size = 0;

    bh_index = sb_bread(sb, ci->index_block);
    if (!bh_index)
        goto io_fail;

    index = (struct ouichefs_file_index_block *)bh_index->b_data;

    for (i = 0; i < (OUICHEFS_BLOCK_SIZE >> 2); i++) {
        uint32_t current_block = index->blocks[i];

        if (current_block != 0) {
            if (remaining_size >= OUICHEFS_BLOCK_SIZE) {
                block_size = OUICHEFS_BLOCK_SIZE;
                remaining_size -= OUICHEFS_BLOCK_SIZE;
            } else {
                block_size = remaining_size;
                remaining_size = 0;
            }

            if (dedup_info == NULL) {
                struct ouichefs_dedup_info di;

                di.bh_index = bh_index;
                di.block = current_block;
                di.block_size = block_size;

                pr_info("composed of the block %d of size %lld\n", di.block, di.block_size);

                if (ouichefs_dedup_scan_directory(sb->s_root->d_inode, &di) < 0)
                    goto fail_brelse;
            } else {
                if ((dedup_info->block != current_block) && are_eq_blocks(sb, dedup_info->block, current_block, dedup_info->block_size, block_size)) {
                    pr_info("block %d and %d are equals\n", dedup_info->block, current_block);
                }
            }
        }
    }

    // mark_buffer_dirty(bh);
    // sync_dirty_buffer(bh);
    brelse(bh_index);

    return 0;

    fail_brelse:
    brelse(bh_index);
    io_fail:
    pr_err("IO error\n");
    return -EIO;
}

static int are_eq_blocks(struct super_block *sb, uint32_t b1, uint32_t b2, loff_t size1, loff_t size2)
{
    struct buffer_head *bh1 = NULL, *bh2 = NULL;

    if (size1 != size2)
        return 0;

    if (b1 == b2)
        return 1;

    bh1 = sb_bread(sb, b1);
    if (!bh1)
        goto fail_bread1;

    bh2 = sb_bread(sb, b2);
    if (!bh2)
        goto fail_bread2;

    if (memcmp(bh1->b_data, bh2->b_data, size1) != 0) {
        brelse(bh2);
        brelse(bh1);

        return 0;
    }

    brelse(bh2);
    brelse(bh1);

    return 1;

    fail_bread2:
    brelse(bh1);
    fail_bread1:
    pr_err("IO error\n");
    return 0;
}