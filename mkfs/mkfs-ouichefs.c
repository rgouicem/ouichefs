#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <endian.h>
#include <string.h>

#define OUICHEFS_MAGIC  0x48434957

#define OUICHEFS_SB_BLOCK_NR     0

#define OUICHEFS_BLOCK_SIZE       (1 << 12)  /* 4 KiB */
#define OUICHEFS_MAX_FILESIZE     (1 << 22)  /* 4 MiB */
#define OUICHEFS_FILENAME_LEN            28
#define OUICHEFS_MAX_SUBFILES           128


struct ouichefs_inode {
	mode_t   i_mode;	  /* File mode */
	uint32_t i_uid;           /* Owner id */
	uint32_t i_gid;		  /* Group id */
	uint32_t i_size;	  /* Size in bytes */
	uint32_t i_ctime;	  /* Inode change time */
	uint32_t i_atime;	  /* Access time */
	uint32_t i_mtime;	  /* Modification time */
	uint32_t i_blocks;	  /* Block count (subdir count for directories) */
	uint32_t i_nlink;	  /* Hard links count */
	uint32_t index_block;	  /* Block with list of blocks for this file */
};

#define OUICHEFS_INODES_PER_BLOCK (OUICHEFS_BLOCK_SIZE / sizeof(struct ouichefs_inode))

struct ouichefs_superblock {
	uint32_t magic;		  /* Magic number */

	uint32_t nr_blocks;	  /* Total number of blocks (incl sb & inodes) */
	uint32_t nr_inodes;       /* Total number of inodes */

	uint32_t nr_istore_blocks;/* Number of inode store blocks */
	uint32_t nr_ifree_blocks; /* Number of free inodes bitmask blocks */
	uint32_t nr_bfree_blocks; /* Number of free blocks bitmask blocks */

	uint32_t nr_free_inodes;  /* Number of free inodes */
	uint32_t nr_free_blocks;  /* Number of free blocks */

	char padding[4064];       /* Padding to match block size */
};

struct ouichefs_file_index_block {
	uint32_t blocks[OUICHEFS_BLOCK_SIZE >> 2];
};

struct ouichefs_dir_block {
	struct ouichefs_file {
		uint32_t inode;
		char filename[OUICHEFS_FILENAME_LEN];
	} files[OUICHEFS_MAX_SUBFILES];
};

static inline void usage(char *appname)
{
	fprintf(stderr,
		"Usage:\n"
		"%s disk\n",
		appname);
}

/* Returns ceil(a/b) */
static inline uint32_t idiv_ceil(uint32_t a, uint32_t b)
{
	uint32_t ret = a / b;
	if (a % b != 0)
		return ret + 1;
	return ret;
}

static struct ouichefs_superblock *write_superblock(int fd, struct stat *fstats)
{
	int ret;
	struct ouichefs_superblock *sb;
	uint32_t nr_inodes = 0, nr_blocks = 0, nr_ifree_blocks = 0;
	uint32_t nr_bfree_blocks = 0, nr_data_blocks = 0, nr_istore_blocks = 0;
	uint32_t mod;

	sb = malloc(sizeof(struct ouichefs_superblock));
	if (!sb)
		return NULL;

	nr_blocks = fstats->st_size / OUICHEFS_BLOCK_SIZE;
	nr_inodes = nr_blocks;
	mod = nr_inodes % OUICHEFS_INODES_PER_BLOCK;
	if (mod != 0)
		nr_inodes += mod;
	nr_istore_blocks = idiv_ceil(nr_inodes, OUICHEFS_INODES_PER_BLOCK);
	nr_ifree_blocks = idiv_ceil(nr_inodes, OUICHEFS_BLOCK_SIZE * 8);
	nr_bfree_blocks = idiv_ceil(nr_blocks, OUICHEFS_BLOCK_SIZE * 8);
	nr_data_blocks = nr_blocks - 1 - nr_istore_blocks - nr_ifree_blocks - nr_bfree_blocks;

	memset(sb, 0, sizeof(struct ouichefs_superblock));
	sb->magic = htole32(OUICHEFS_MAGIC);
	sb->nr_blocks = htole32(nr_blocks);
	sb->nr_inodes = htole32(nr_inodes);
	sb->nr_istore_blocks = htole32(nr_istore_blocks);
	sb->nr_ifree_blocks = htole32(nr_ifree_blocks);
	sb->nr_bfree_blocks = htole32(nr_bfree_blocks);
	sb->nr_free_inodes = htole32(nr_inodes - 1);
	sb->nr_free_blocks = htole32(nr_data_blocks - 1);

	ret = write(fd, sb, sizeof(struct ouichefs_superblock));
	if (ret != sizeof(struct ouichefs_superblock)) {
		free(sb);
		return NULL;
	}

	printf("Superblock: (%ld)\n"
	       "\tmagic=%#x\n"
	       "\tnr_blocks=%u\n"
	       "\tnr_inodes=%u (istore=%u blocks)\n"
	       "\tnr_ifree_blocks=%u\n"
	       "\tnr_bfree_blocks=%u\n"
	       "\tnr_free_inodes=%u\n"
	       "\tnr_free_blocks=%u\n",
	       sizeof(struct ouichefs_superblock),
	       sb->magic, sb->nr_blocks, sb->nr_inodes, sb->nr_istore_blocks,
	       sb->nr_ifree_blocks, sb->nr_bfree_blocks, sb->nr_free_inodes,
	       sb->nr_free_blocks);

	return sb;
}

static int write_inode_store(int fd, struct ouichefs_superblock *sb)
{
	int ret = 0;
	uint32_t i;
	struct ouichefs_inode *inode;
	char *block;
	uint32_t first_data_block;

	/* Allocate a zeroed block for inode store */
	block = malloc(OUICHEFS_BLOCK_SIZE);
	if (!block)
		return -1;
	memset(block, 0, OUICHEFS_BLOCK_SIZE);

	/* Root inode (inode 0) */
	inode = (struct ouichefs_inode *)block;
	first_data_block = 1 + le32toh(sb->nr_bfree_blocks) +
		le32toh(sb->nr_ifree_blocks) +
		le32toh(sb->nr_istore_blocks);
	inode->i_mode = htole32(S_IFDIR |
				S_IRUSR | S_IRGRP | S_IROTH |
				S_IWUSR | S_IWGRP |
				S_IXUSR | S_IXGRP | S_IXOTH);
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_size = htole32(OUICHEFS_BLOCK_SIZE);
	inode->i_ctime = inode->i_atime = inode->i_mtime = htole32(0);
	inode->i_blocks = htole32(1);
	inode->i_nlink = htole32(2);
	inode->index_block = htole32(first_data_block);

	ret = write(fd, block, OUICHEFS_BLOCK_SIZE);
	if (ret != OUICHEFS_BLOCK_SIZE) {
		ret = -1;
		goto end;
	}

	/* Reset inode store blocks to zero */
	memset(block, 0, OUICHEFS_BLOCK_SIZE);
	for (i = 1; i < sb->nr_istore_blocks; i++) {
		ret = write(fd, block, OUICHEFS_BLOCK_SIZE);
		if (ret != OUICHEFS_BLOCK_SIZE) {
			ret = -1;
			goto end;
		}
	}
	ret = 0;

	printf("Inode store: wrote %d blocks\n"
	       "\tinode size = %ld B\n",
	       i, sizeof(struct ouichefs_inode));

end:
	free(block);
	return ret;
}

static int write_ifree_blocks(int fd, struct ouichefs_superblock *sb)
{
	int ret = 0;
	uint32_t i;
	char *block;
	uint64_t *ifree;

	block = malloc(OUICHEFS_BLOCK_SIZE);
	if (!block)
		return -1;
	ifree = (uint64_t *) block;

	/* Set all bits to 1 */
	memset(ifree, 0xff, OUICHEFS_BLOCK_SIZE);

	/* First ifree block, containing first used inode */
	ifree[0] = htole64(0xfffffffffffffffe);
	ret = write(fd, ifree, OUICHEFS_BLOCK_SIZE);
	if (ret != OUICHEFS_BLOCK_SIZE) {
		ret = -1;
		goto end;
	}

	/* All ifree blocks except the one containing 2 first inodes */
	ifree[0] = 0xffffffffffffffff;
	for (i = 1; i < le32toh(sb->nr_ifree_blocks); i++) {
		ret = write(fd, ifree, OUICHEFS_BLOCK_SIZE);
		if (ret != OUICHEFS_BLOCK_SIZE) {
			ret = -1;
			goto end;
		}
	}
	ret = 0;

	printf("Ifree blocks: wrote %d blocks\n", i);

end:
	free(block);

	return ret;
}

static int write_bfree_blocks(int fd, struct ouichefs_superblock *sb)
{
	int ret = 0;
	uint32_t i;
	char *block;
	uint64_t *bfree, mask, line;
	uint32_t nr_used = le32toh(sb->nr_istore_blocks) +
		le32toh(sb->nr_ifree_blocks) +
		le32toh(sb->nr_bfree_blocks) + 2;

	block = malloc(OUICHEFS_BLOCK_SIZE);
	if (!block)
		return -1;
	bfree = (uint64_t *)block;

	/*
	 * First blocks (incl. sb + istore + ifree + bfree + 1 used block)
	 * we suppose it won't go further than the first block
	 */
	memset(bfree, 0xff, OUICHEFS_BLOCK_SIZE);
	i = 0;
	while (nr_used) {
		line = 0xffffffffffffffff;
		for (mask = 0x1; mask != 0x0; mask <<= 1) {
			line &= ~mask;
			nr_used--;
			if (!nr_used)
				break;
		}
		bfree[i] = htole64(line);
		i++;
	}
	ret = write(fd, bfree, OUICHEFS_BLOCK_SIZE);
	if (ret != OUICHEFS_BLOCK_SIZE) {
		ret = -1;
		goto end;
	}

	/* other blocks */
	memset(bfree, 0xff, OUICHEFS_BLOCK_SIZE);
	for (i = 1; i < le32toh(sb->nr_bfree_blocks); i++) {
		ret = write(fd, bfree, OUICHEFS_BLOCK_SIZE);
		if (ret != OUICHEFS_BLOCK_SIZE) {
			ret = -1;
			goto end;
		}
	}
	ret = 0;

	printf("Bfree blocks: wrote %d blocks\n", i);
end:
	free(block);

	return ret;
}

static int write_data_blocks(int fd, struct ouichefs_superblock *sb)
{
	int ret = 0;
	/* struct ouichefs_dir_block root_block; */
	/* struct ouichefs_file_index_block foo_block; */
	/* char *foo; */
	/* uint32_t first_block = le32toh(sb->nr_istore_blocks) + */
	/* 	le32toh(sb->nr_ifree_blocks) + le32toh(sb->nr_bfree_blocks) + 3; */

	/* foo = malloc(OUICHEFS_BLOCK_SIZE); */
	/* if (!foo) */
	/* 	return -1; */
	/* memset(foo, 0, OUICHEFS_BLOCK_SIZE); */

/* end: */
/* 	free(foo); */

	return ret;
}

int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS, fd;
	long int min_size;
	struct stat stat_buf;
	struct ouichefs_superblock *sb = NULL;
	long int blk_size = 0;

	if (argc != 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* Open disk image */
	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("open():");
		return EXIT_FAILURE;
	}

	/* Get image size */
	ret = fstat(fd, &stat_buf);
	if (ret != 0) {
		perror("fstat():");
		ret = EXIT_FAILURE;
		goto fclose;
	}

	/* Get block device size */
	if ((stat_buf.st_mode & S_IFMT) == S_IFBLK) {
		ret = ioctl(fd, BLKGETSIZE64, &blk_size);
		if (ret != 0) {
			perror("BLKGETSIZE64:");
			ret = EXIT_FAILURE;
			goto fclose;
		}
		stat_buf.st_size = blk_size;
	}

	/* Check if image is large enough */
	min_size = 100 * OUICHEFS_BLOCK_SIZE;
	if (stat_buf.st_size <= min_size) {
		fprintf(stderr,
			"File is not large enough (size=%ld, min size=%ld)\n",
			stat_buf.st_size,
			min_size);
		ret = EXIT_FAILURE;
		goto fclose;
	}

	/* Write superblock (block 0) */
	sb = write_superblock(fd, &stat_buf);
	if (!sb) {
		perror("write_superblock():");
		ret = EXIT_FAILURE;
		goto fclose;
	}

	/* Write inode store blocks (from block 1) */
	ret = write_inode_store(fd, sb);
	if (ret != 0) {
		perror("write_inode_store():");
		ret = EXIT_FAILURE;
		goto free_sb;
	}

	/* Write inode free bitmap blocks */
	ret = write_ifree_blocks(fd, sb);
	if (ret != 0) {
		perror("write_ifree_blocks()");
		ret = EXIT_FAILURE;
		goto free_sb;
	}

	/* Write block free bitmap blocks */
	ret = write_bfree_blocks(fd, sb);
	if (ret != 0) {
		perror("write_bfree_blocks()");
		ret = EXIT_FAILURE;
		goto free_sb;
	}

	/* Write data blocks */
	ret = write_data_blocks(fd, sb);
	if (ret != 0) {
		perror("write_data_blocks():");
		ret = EXIT_FAILURE;
		goto free_sb;
	}

free_sb:
	free(sb);
fclose:
	close(fd);

	return ret;
}
