.. SPDX-License-Identifier: GPL-2.0

Original author: Aron Egill Fridriksson <aron.fridriksson@rwth-aachen.de>

# Snapshotting system implementation overview

## Introduction
This document is a step-by-step walkthrough of most of the features of a
snapshotting system. It concerns file creation, deletion and modification.
Snapshot creation, deletion and reverting are also considered.
This document is best viewed with monospace fonts.

## High-level overview caveats
The diagrams here are simplified as to not show index blocks. In reality every
inode has one associated index block, so they are implicitly part of the "Inode"
in the diagrams.
Further, it is assumed that each file can have only three data blocks and each
data block holds only one byte. This is WAY smaller than is practical but it
makes the diagrams and explainations easier.

## 1. Existing filesystem
The filesystem contains only one file:
file.txt

the fs structure at this point is:

+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x58 | 0xFC | NONE |
+-------+    +----------+    +--------------------+

Where we assume that the data contained in file.txt fits in two data blocks and
the third is not allocated.
The snapshot tree does not exist at this point since no snapshots have been
created yet. The file system behaves just as it did before the snapshotting was
implemented.

## 2. Snapshot 1 is created.
It's the first snapshot so the snapshot tree is created:

+----------+    +----------+
| Snapshot | <- | Shadow   |
| 1        |    | Snapshot |
+----------+    +----------+

Snapshot 1 contains no inodes since it represents the filesystem unmodified from
this state. The shadow snapshot is an everpresent snapshot that exists only in
RAM and tracks all changes made in the filesystem. In theory when a new snapshot
is created the shadow snapshot becomes and ACTUAL snapshot (and a new empty
shadow snapshot points to it).
It's also worth noting that the superblock field index_snapshot points to the
same snapshot that the shadow snapshot points to. This is also not shown in the
drawings but must be taken into consideration.

## 3. file.txt is modified.
Let's write 0x75 into the first block of file.txt. The state of the filesystem
tree and the snapshot tree then becomes:

+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+

+----------+    +----------+
| Snapshot | <- | Shadow   |
| 1        |    | Snapshot |
+----------+    +----------+
                    |
                    v
                +-------+    +----------+    +--------------------+
                | Root  | -> | file.txt | -> | Data Blocks        |
                | Inode |    | Inode    |    | 0x58 | NONE | NONE |
                +-------+    +----------+    +--------------------+

The original filesystem tree is modified and the shadow snapshot now tracks the
change, saving the old value in the relavant block. Note that only the modified
block is allocated here, the unmodified blocks remain unallocated.

## 4. Snapshot 2 created.
Now the Shadow snapshot graduates to an actual snapshot:

+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Shadow   |
| 1        |    | 2        |    | Snapshot |
+----------+    +----------+    +----------+
                    |
                    v
                +-------+    +----------+    +--------------------+
                | Root  | -> | file.txt | -> | Data Blocks        |
                | Inode |    | Inode    |    | 0x58 | NONE | NONE |
                +-------+    +----------+    +--------------------+

The original filesystem is unchanged, as it always will be when a snapshot is
made.

## 5. New Directory.
Now we create a directory dir/ in the root directory. This will change the
filesystem tree so:

+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+
       |
       |     +-------+
       +---> | dir/  |
             | Inode |
             +-------+

And the snapshot tree becomes:

+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Shadow   |
| 1        |    | 2        |    | Snapshot |
+----------+    +----------+    +----------+
                                    |
                                    v
                   +(N)----+    +-------+
                   | dir/  | <- | Root  |
                   | Inode |    | Inode |
                   +-------+    +-------+

Here the newly created Inode for dir/ is tagged with N to mark it as a newly
created Inode in this snapshot.
Note that Snapshot 2 retains the data created in step 4, it's just not drawn
here for brevity.

## 6. Nested File.
Now we create a file dir/nested_file.txt which is empty:
+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+
       |
       |     +-------+    +------------------+
       +---> | dir/  | -> | nested_file.txt  |
             | Inode |    | Inode            |
             +-------+    +------------------+

Which changes the snapshot tree thusly:
+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Shadow   |
| 1        |    | 2        |    | Snapshot |
+----------+    +----------+    +----------+
                                        |
                                        v
+(N)---------------+    +(N)----+    +-------+
| nested_file.txt  | <- | dir/  | <- | Root  |
| Inode            |    | Inode |    | Inode |
+------------------+    +-------+    +-------+

Like before we mark the snapshot inodes which were created in this snapshot with
an (N)

## 7. Snapshot 3 created
We create snapshot 3 similarly to previous times (data in snapshot 3 not shown):
+----------+    +----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot | <- | Shadow   |
| 1        |    | 2        |    | 3        |    | Snapshot |
+----------+    +----------+    +----------+    +----------+

## 8. Writing to a new data block
let's try writing 0x12 into block 3 of dir/nested_file.txt:
+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+
       |
       |     +-------+    +------------------+    +--------------------+
       +---> | dir/  | -> | nested_file.txt  | -> | Data Blocks        |
             | Inode |    | Inode            |    | NONE | NONE | 0x12 |
             +-------+    +------------------+    +--------------------+

The shadow snapshot reflects this change:
+----------+    +----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot | <- | Shadow   |
| 1        |    | 2        |    | 3        |    | Snapshot | ----+
+----------+    +----------+    +----------+    +----------+     |
                                                                 v
+--------------------+    +------------------+    +-------+    +-------+
| Data Blocks        | <- | nested_file.txt  | <- | dir/  | <- | Root  |
| NONE | NONE | 0xFF |    | Inode            |    | Inode |    | Inode |
+--------------------+    +------------------+    +-------+    +-------+

The value 0xFF written represents that the data block did not previously exist.
This could fact be embedded into the nested_file.txt index block.

## 9. Reverting back to snapshot 1

### 9.1 Destroying the shadow snapshot
We start by removing the data from the shadow snapshot if there was any data
written to it, which -- in this case -- there was. Reading the shadow snapshot
we can infer that it added one data block to dir/nested_file.txt. As we delete
the shadow snapshot we also need to remove that data block from the filesystem
tree:

+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+
       |
       |     +-------+    +------------------+
       +---> | dir/  | -> | nested_file.txt  |
             | Inode |    | Inode            |
             +-------+    +------------------+

+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
                                        |
                                        v
+------------------+    +-------+    +-------+
| nested_file.txt  | <- | dir/  | <- | Root  |
| Inode            |    | Inode |    | Inode |
+------------------+    +-------+    +-------+

### 9.2 Reverting from snapshot 3
We look at snapshot 3 and can infer that there's two inodes in the tree that
were created:
+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
                                        |
                                        v
+(N)---------------+    +(N)----+    +-------+
| nested_file.txt  | <- | dir/  | <- | Root  |
| Inode            |    | Inode |    | Inode |
+------------------+    +-------+    +-------+

We must then remove these from the filesystem tree:
+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+

### 9.3 Reverting from snapshot 2
Now we look at snapshot 2 and can infer that one data block changed, with its
previous value being 0x58:
+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
                    |
                    v
                +-------+    +----------+    +--------------------+
                | Root  | -> | file.txt | -> | Data Blocks        |
                | Inode |    | Inode    |    | 0x58 | NONE | NONE |
                +-------+    +----------+    +--------------------+

We revert this in the filesystem. But we also need to store the value that
replaced 0x58, namely 0x75, since we also need to be able to revert forward back
to snapshot 3. We use the same field to store the value:
+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
                    |
                    v
                +-------+    +----------+    +--------------------+
                | Root  | -> | file.txt | -> | Data Blocks        |
                | Inode |    | Inode    |    | 0x75 | NONE | NONE |
                +-------+    +----------+    +--------------------+

### 9.3 Arrived at snapshot 1
Now that the iteration has reached snapshot 1 we're done and the original file
system reflects the state of the partition when snapshot 1 was taken:

+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x58 | 0xFC | NONE |
+-------+    +----------+    +--------------------+

And the snapshot tree looks like this:

+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
    A
    |
+----------+
| Shadow   |
| Snapshot |
+----------+

## 10. Snapshot 4 branching from snapshot 1
For snapshot 4 we'll add a file "newfile.txt" with some data in it and delete
file.txt:

+-------+    +-------------+    +--------------------+
| Root  | -> | newfile.txt | -> | Data Blocks        |
| Inode |    | Inode       |    | NONE | 0x55 | NONE |
+-------+    +-------------+    +--------------------+

The snapshot tree then looks like this after we create the snapshot:

+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
    A
    |
+----------+    +-------+    +(D)-------+    +--------------------+
| Snapshot | -> | Root  | -> | file.txt | -> | Data Blocks        | 
| 4        |    | Inode |    | Inode    |    | 0x58 | 0xFC | NONE |
+----------+    +-------+    +----------+    +--------------------+
    A                  |
    |                  |     +(N)----------+
+----------+           +---> | newfile.txt |
| Shadow   |                 | Inode       |
| Snapshot |                 +-------------+
+----------+

Now the (D) tag represents a file that was deleted in this snapshot. We must
preserve what the file used to contain in case we revert back to the snapshot
before this one. The same data blocks that used to represent the file.txt in the
filesystem can be reused.
Inode for the file we just created, tagged (N), does not need to point to the 
data that was put into the file, we know that already from the information in
the file system tree. Though this inode pointer needs to be set when we revert
backwards through this snapshot in order to be able to revert forward (as we'll
see in the next step).

## 11. Reverting back to snapshot 3
Now for our final trick we'll go through the entire tree, reverting both
backwards and forwards in the process.

### 11.1 Resolving the path
The first problem, of course, is finding the path from snapshot 4 to snapshot 3.
This document will not go into detail on how to do that as the implementation of
that algorithm may be a large problem seperate from the details of this
snapshotting system. From here we'll take the leap of faith and assume the path
has already been resolved and all the necessary node pairs have been laid out in
a loop.

### 11.2 Reverting to snapshot 1
The first thing we need to do is look at snapshot 2 and see what we need to do
in order to revert to snapshot 1. We see that one file was created and one file
was destroyed. Then in the original filesystem we must create file "file.txt"
and fill it with the data saved in the snapshotting tree.
Now we must delete "newfile.txt" from the original file system but save its
content in the snapshot tree.
+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x58 | 0xFC | NONE |
+-------+    +----------+    +--------------------+

And the snapshot tree:

+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
    A
    |
+----------+    +-------+    +(D)-------+    +--------------------+
| Snapshot | -> | Root  | -> | file.txt | -> | Data Blocks        | 
| 4        |    | Inode |    | Inode    |    | 0x58 | 0xFC | NONE |
+----------+    +-------+    +----------+    +--------------------+
                       |
                       |     +(N)----------+    +--------------------+
                       +---> | newfile.txt | -> | Data Blocks        |
                             | Inode       |    | NONE | 0x55 | NONE |
                             +-------------+    +--------------------+
            
### 11.3 Reverting forward to snapshot 2
Now we read snapshot 2 and see what was added in that snapshot. We see that
file.txt was modified in that snapshot and the data blocks contain the new value
it was set to. Now we do similar to step 9.3 and swap the values so that we can
revert backwards if needed.
+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+

Snapshot tree:
+----------+    +----------+    +----------+
| Snapshot | <- | Snapshot | <- | Snapshot |
| 1        |    | 2        |    | 3        |
+----------+    +----------+    +----------+
    A             |
    |             v
+----------+    +-------+    +----------+    +--------------------+
| Snapshot |    | Root  | -> | file.txt | -> | Data Blocks        | 
| 4        |    | Inode |    | Inode    |    | 0x58 | NONE | NONE |
+----------+    +-------+    +----------+    +--------------------+

### 11.4 Reverting forward to snapshot 3
In snapshot 3 we read that a directory and a file with no data were created. So
we create them in the filesystem and leave the snapshot tree unmodified (but
create the shadow snapshot at the appropriate location).
+-------+    +----------+    +--------------------+
| Root  | -> | file.txt | -> | Data Blocks        |
| Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+-------+    +----------+    +--------------------+
       |
       |     +-------+    +-----------------+
       +---> | dir/  | -> | nested_file.txt |
             | Inode |    | Inode           |
             +-------+    +-----------------+

Snapshot tree:
+----------+    +----------+    +----------+    +----------+ 
| Snapshot | <- | Snapshot | <- | Snapshot | <- | Shadow   |
| 1        |    | 2        |    | 3        |    | Snapshot |
+----------+    +----------+    +----------+    +----------+
    A                            |
    |            +---------------+
+----------+     |
| Snapshot |     v
| 4        |    +-------+    +(N)----+    +(N)--------------+    
+----------+    | Root  | -> | dir/  | -> | nested_file.txt |    
                | Inode |    | Inode |    | Inode           |    
                +-------+    +-------+    +-----------------+    

The filesystem now represents the state at the time of snapshot 3.

## Deleting snapshot 4
Let's say we decide that snapshot 4 was a mistake alltogether and we'd like to
delete it entirely. Since snapshot 4 is a "leaf" (has no branches / children)
removing it is relatively trivial. Removing non-leaf snapshots is more
difficult.
Let's look at snapshot 4:

+----------+    +-------+    +(D)-------+    +--------------------+
| Snapshot | -> | Root  | -> | file.txt | -> | Data Blocks        | 
| 4        |    | Inode |    | Inode    |    | 0x75 | 0xFC | NONE |
+----------+    +-------+    +----------+    +--------------------+
                       |
                       |     +(N)----------+    +--------------------+
                       +---> | newfile.txt | -> | Data Blocks        |
                             | Inode       |    | NONE | 0x55 | NONE |
                             +-------------+    +--------------------+
 
It represents the life of one file and the death of another. In the case of the
inode tagged with (N) we can simply deallocate the data block and its inode. The
same would be true if it represented a modification.
Importantly, we CANNOT deallocate the data block that represents the file
deletion since that data block is still being used by the filesystem inode.
Indeed, if we look closer we see that the data block is actually "wrong" (the
first data block should be 0x86 instead of 0x75). This is actually fine, since
there is no way for the reverting algorithm to reach this snapshot while it's in
this incorrect state.
So. In the case of the inode tagged (D) we simply deallocate the inode and leave
its data block alone. The root inode and snapshot 4 are similarly removed.
Naturally, the snapshot tree now looks like this:

+----------+    +----------+    +----------+    +----------+ 
| Snapshot | <- | Snapshot | <- | Snapshot | <- | Shadow   |
| 1        |    | 2        |    | 3        |    | Snapshot |
+----------+    +----------+    +----------+    +----------+
 
And the filesystem remains unchanged.
