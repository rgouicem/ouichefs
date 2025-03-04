int traverse_backup_tree(struct inode *source, struct inode *destination)
{
	struct super_block *sb;
	struct ouichefs_inode_info *ci;
	struct inode *parent, *new_inode = NULL, *parent_backup = NULL;
	char filename[OUICHEFS_FILENAME_LEN];
	int ret = 0;

	pr_info("traverse_backup_tree: Starting with inode %lu\n",
		source->i_ino);

	/* Base case checks */
	if (!source) {
		pr_info("traverse_backup_tree: Source is NULL, returning success\n");
		return 0;
	}

	ci = OUICHEFS_INODE(source);
	sb = source->i_sb;

	/* Check if the source has backup flag set */
	if (!ci->is_backup) {
		pr_info("traverse_backup_tree: Inode %lu not marked for backup, stopping\n",
			source->i_ino);
		return 0;
	}

	/* Check if we're at the root inode */
	if (source->i_ino == 1) {
		pr_info("traverse_backup_tree: Reached filesystem root\n");
		/* Create backup of root directly with destination as parent */
		new_inode = create_new_inode(sb, source, destination);
		if (IS_ERR(new_inode)) {
			ret = PTR_ERR(new_inode);
			pr_err("traverse_backup_tree: Failed to create backup of root: %d\n",
			       ret);
			return ret;
		}

		/* Initialize directory structure if needed */
		if (S_ISDIR(source->i_mode)) {
			ret = add_dot_entries(new_inode, destination);
			if (ret) {
				pr_err("traverse_backup_tree: Failed to add dot entries: %d\n",
				       ret);
				iput(new_inode);
				return ret;
			}
		}

		/* Add root to destination directory */
		ret = add_dir_entry(destination, "root", new_inode);
		if (ret) {
			pr_err("traverse_backup_tree: Failed to add root dir entry: %d\n",
			       ret);
			iput(new_inode);
			return ret;
		}

		iput(new_inode);
		return 0;
	}

	/* Find the parent directory */
	parent = find_parent_dir(source);
	if (!parent) {
		pr_err("traverse_backup_tree: Failed to find parent for inode %lu\n",
		       source->i_ino);
		return -ENOENT;
	}

	/* Get the filename of the source in its parent directory */
	if (!get_filename_in_parent(source, filename)) {
		pr_err("traverse_backup_tree: Failed to get filename for inode %lu\n",
		       source->i_ino);
		iput(parent);
		return -ENOENT;
	}

	pr_info("traverse_backup_tree: Processing inode %lu (%s) with parent %lu\n",
		source->i_ino, filename, parent->i_ino);

	/* Check if parent has backup flag */
	if (OUICHEFS_INODE(parent)->is_backup) {
		/* Process parent first to establish backup path from top down */
		ret = traverse_backup_tree(parent, destination);
		if (ret) {
			pr_err("traverse_backup_tree: Failed to process parent: %d\n",
			       ret);
			iput(parent);
			return ret;
		}

		/* Find the newly created backup copy of the parent */
		parent_backup = NULL;
		struct buffer_head *bh = NULL;
		struct ouichefs_dir_block *dir_block = NULL;

		/* Search for the backup parent in the destination directory */
		bh = sb_bread(sb, OUICHEFS_INODE(destination)->index_block);
		if (!bh) {
			pr_err("traverse_backup_tree: Failed to read destination directory\n");
			iput(parent);
			return -EIO;
		}

		dir_block = (struct ouichefs_dir_block *)bh->b_data;
		int i, found = 0;

		for (i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
			if (dir_block->files[i].inode == 0)
				break;

			/* Get inode and check if it's a backup of our parent */
			struct inode *tmp =
				ouichefs_iget(sb, dir_block->files[i].inode);
			if (IS_ERR(tmp))
				continue;

			/* Check if this is the backup copy of our parent */
			if (S_ISDIR(tmp->i_mode)) {
				/* Check if this directory contains our parent's backup */
				struct buffer_head *child_bh = sb_bread(
					sb, OUICHEFS_INODE(tmp)->index_block);
				if (child_bh) {
					struct ouichefs_dir_block *child_dir =
						(struct ouichefs_dir_block *)
							child_bh->b_data;

					for (int j = 0;
					     j < OUICHEFS_MAX_SUBFILES; j++) {
						char parent_name
							[OUICHEFS_FILENAME_LEN];
						if (child_dir->files[j].inode ==
						    0)
							break;

						if (get_filename_in_parent(
							    parent,
							    parent_name) &&
						    strncmp(child_dir->files[j]
								    .filename,
							    parent_name,
							    OUICHEFS_FILENAME_LEN) ==
							    0) {
							parent_backup = tmp;
							found = 1;
							brelse(child_bh);
							break;
						}
					}
					brelse(child_bh);
				}
			}

			if (found)
				break;

			iput(tmp);
		}

		brelse(bh);

		if (!parent_backup) {
			pr_err("traverse_backup_tree: Failed to find backup parent\n");
			iput(parent);
			return -ENOENT;
		}
	} else {
		/* If parent doesn't have backup flag, use destination as parent */
		parent_backup = destination;
		ihold(destination);
	}

	/* Create a backup copy of the current node */
	new_inode = create_new_inode(sb, source, parent_backup);
	if (IS_ERR(new_inode)) {
		ret = PTR_ERR(new_inode);
		pr_err("traverse_backup_tree: Failed to create new inode: %d\n",
		       ret);
		iput(parent_backup);
		iput(parent);
		return ret;
	}

	/* If it's a directory, initialize with . and .. entries */
	if (S_ISDIR(source->i_mode)) {
		ret = add_dot_entries(new_inode, parent_backup);
		if (ret) {
			pr_err("traverse_backup_tree: Failed to add dot entries: %d\n",
			       ret);
			iput(new_inode);
			iput(parent_backup);
			iput(parent);
			return ret;
		}
	}

	/* Link the new inode to its parent in the backup tree */
	ret = add_dir_entry(parent_backup, filename, new_inode);
	if (ret) {
		pr_err("traverse_backup_tree: Failed to add dir entry: %d\n",
		       ret);
		iput(new_inode);
		iput(parent_backup);
		iput(parent);
		return ret;
	}

	pr_info("traverse_backup_tree: Successfully processed inode %lu\n",
		source->i_ino);

	/* Clean up */
	iput(new_inode);
	iput(parent_backup);
	iput(parent);

	return 0;
}
