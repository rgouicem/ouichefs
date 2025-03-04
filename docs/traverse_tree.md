# Algorithm: Traverse Backup Tree

**Function**: `traverse_backup_tree(struct inode source, struct inode new_root)`

**Purpose**: Create a backup tree that follows nodes marked with `is_backup=1` in a direct line from source upward until reaching a non-backup node or the filesystem root. The `new_root` inode will become the topmost node of the backup tree, replacing the original root inode in the backup structure.

**Parameters**:
- `source`: The source inode to start traversal from
- `new_root`: An initialized inode structure that will serve as the root inode of the backup tree (not the original filesystem root)

**Algorithm**:
1. **Base case checks**:
   - If `source` is NULL, return success (0)
   - If `source->is_backup` is NOT set, return success (0) - we've reached a non-backup node
   - If `source` is the filesystem root (inode number 1), process it, set `new_root` as the backup tree's top node, and return success (0)

2. **Process current directory node**:
   - For each directory in the backup path:
     - Create a new inode in the backup tree under the appropriate parent to maintain hierarchy
     - Create a new index block (directory block)
     - **IMPORTANT**: Copy all entries from the original directory block
     - Most directory entries will continue to reference the original filesystem objects
     - Only entries in the direct backup line will be modified to point to their backup counterparts

3. **Handle direct backup line**:
   - When a directory contains a child that's part of the backup line:
     - Create a backup copy of that child inode
     - Update only that specific directory entry to point to the new backup inode
     - All other entries remain pointing to original filesystem objects

4. **Process current file node**:
   - For files in the backup path:
     - Create a new inode in the backup tree
     - The index block will reference the same data blocks as the original file
     - No file data is duplicated, only inode metadata

5. **Recursive processing**:
   - Find the parent directory using `find_parent_dir()`
   - If parent exists:
     - If parent has `is_backup=1`, recursively call `traverse_backup_tree(parent, new_root)`
     - After recursion returns, link the current backup node to its parent in the backup tree
   - If parent is the root:
     - Process the root node (if it has `is_backup=1`)
     - Set `new_root` to be the topmost node of the backup tree, replacing the original filesystem root
     - Return success (we've reached the end of the path)

**Key Characteristics**:
- Traverses from source upward, but builds the backup tree maintaining the original hierarchical structure
- The `new_root` parameter is an inode structure that will replace the original filesystem root in the backup tree
- When the algorithm completes, `new_root` points to the topmost node of the backup tree (not the original filesystem root)
- Stops traversal when either:
  - A node without the backup flag is encountered
  - The filesystem root is reached
- Creates a sparse backup tree with minimal duplication
- Directory blocks in the backup tree reference mostly original filesystem objects
- Only entries for direct backup path nodes are updated to point to backup copies
- File data blocks are always referenced, never duplicated
- Original directory structure is preserved in the backup path

This algorithm efficiently implements a backup strategy that only duplicates the minimum necessary structure while maintaining references to original content wherever possible. The algorithm ensures that `new_root` becomes the root inode of the backup tree, replacing the original filesystem root in the backup structure.