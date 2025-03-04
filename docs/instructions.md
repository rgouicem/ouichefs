# Snapshotting system for Ouichefs
## ouichefs – The Most Classy Filesystem in the World

*Authors:*
- Redha Gouicem, RWTH Aachen University
- Jérôme Coquisart, RWTH Aachen University

In this project, you will have to implement new features on top of an existing filesystem, **`ouichefs`**, in the form of a kernel module for Linux 6.5.7.

As usual, you are strongly encouraged to use a cross-reference tool to navigate the Linux kernel source code, be it integrated in your development environment or online like [Bootlin's Elixir](https://elixir.bootlin.com/linux/v6.5.7/source). You are also strongly encouraged to read the documentation available in `Documentation/filesystem/vfs.rst` or [online](https://docs.kernel.org/filesystems/vfs.html) to better understand how the virtual filesystem (VFS) layer works and how a filesystem is implemented.

> **Important**
>
> **Please read the assignment in its entirety before starting your implementation!!!**
>
> If you know everything you need to do beforehand, you will make better design decisions, and you won't have to change things back and forth often!

Before starting, you need to download the source code of the **`ouichefs`** filesystem from GitHub at the following address: [https://github.com/rgouicem/ouichefs](https://github.com/rgouicem/ouichefs). Be careful to use the branch named `v6.5.7`!

In addition to the source code, you will find documentation explaining the design of this simple filesystem as well as the relationship between various structures in the kernel. Don't hesitate to read the source code (it is documented), and to play around with the vanilla filesystem in your virtual machine to better understand how it works, its limitations, etc.

> **Objectives of the project**
>
> In this project, you will extend the features of ouichefs with a **snapshotting system**, allowing users to manually create, restore, list, and delete snapshots.

## Snapshotting system

Snapshots allow the user to save the current state of the partition on the disk. A snapshot is essentially a frozen state of the filesystem at a given moment. This includes file data and file metadata. The main use of snapshots is the ability to roll back to a previous version of the filesystem. This can help recover lost data, or restore previous versions of a file. Modern filesystems, including ZFS and BTRFS, are capable of creating and managing snapshots.

You are free to design the feature however you like. However, here are some considerations to keep in mind to develop a smart and efficient solution.

### Data

A naive implementation would be to copy all the metadata and data blocks somewhere on the disk. Restoring a snapshot would then be as easy as copying everything back. This option has limitations, especially in terms of storage. Each snapshot would effectively double the disk usage, and you would quickly run out of storage space.

Instead of copying all the blocks, we want to duplicate them only if they have been modified since the last snapshot. Unmodified blocks do not have to be duplicated after a snapshot, and modified blocks have to be copied to ensure the previous snapshots remain unaltered. By doing the duplication that way, we don't have to copy the whole partition when taking a snapshot, and we ensure the data associated with past snapshots is never modified. This method is called **copy-on-write** and is widely used by modern filesystems.

You are required to design your module efficiently to get a good grade on this project. Do not implement the naive solution!

### Metadata

To manage metadata, there are multiple design choices possible (again). The straightforward implementation is to save all metadata, e.g., inodes, for each snapshot. Since the size of metadata is not that big compared to file data, it's acceptable to limit your implementation to a simple copy. However, it is also possible to apply the copy-on-write technique mentioned earlier on metadata. This design is more complicated to implement, but it is much more space efficient.

## Guidelines for the interface

> **Warning**
>
> Carefully follow these instructions; this API must be respected for your submission.

To manage snapshots, you will develop a **sysfs interface**.

For each mounted partition of ouichefs, create a sysfs directory under `/sys/fs/ouichefs/` named after the partition. For instance, after mounting the partition `/dev/sda`, the directory `/sys/fs/ouichefs/sda` should appear. This directory will contain the files `create`, `destroy`, `restore`, and `list`. Multiple partitions can be mounted at the same time, so make sure each of them gets its own directory.

```bash
/sys/fs/ouichefs
|-- sda
|   |-- create
|   |-- destroy
|   |-- list
|   `-- restore
`-- sdb
    |-- create
    |-- destroy
    |-- list
    `-- restore
```

The `create` file should assign a unique ID to the snapshot and save the current state of the partition. To use it, simply write to the file associated with the partition.

```bash
$ echo 1 > /sys/fs/ouichefs/sda/create
```

The `list` file should list all the snapshots previously saved, printing the unique ID and the creation date. You can print any additional information, but make sure you are printing the snapshot ID first. The format is `ID: dd.mm.yy HH:MM:SS`

```bash
$ cat /sys/fs/ouichefs/sda/list
1: 25.12.24 19:12:37
2: 29.12.24 21:18:01
3: 31.12.24 08:59:47
4: 02.01.25 23:28:31
5: 07.01.25 08:34:21
```

Write a snapshot ID to the `destroy` file to permanently delete a snapshot from the list. This function should reclaim any data and metadata that can be reclaimed.

```bash
$ echo 3 > /sys/fs/ouichefs/sda/destroy
$ cat /sys/fs/ouichefs/sda/list
1: 25.12.24 19:12:37
2: 29.12.24 21:18:01
4: 02.01.25 23:28:31
5: 07.01.25 08:34:21
```

Finally, write a snapshot ID to the `restore` file to restore a snapshot, effectively rolling back the partition to the state it was at the time of the snapshot.

If snapshot 1 contains:

```bash
snapshot.c users.yml
```

And snapshot 4 contains:

```bash
Makefile a.out snapshot.c users.yml
```

Then here is an example of how to use restore:

```bash
$ echo 1 > /sys/fs/ouichefs/sda/restore
$ ls
snapshot.c users.yml
$ echo 4 > /sys/fs/ouichefs/sda/restore
$ ls
Makefile a.out snapshot.c users.yml
$ cat /sys/fs/ouichefs/sda/list # restore shouldn't delete other snapshots
1: 25.12.24 19:12:37
2: 29.12.24 21:18:01
3: 31.12.24 08:59:47
4: 02.01.25 23:28:31
5: 07.01.25 08:34:21
```

For an easier management of your sysfs interface, it's highly recommended to use `ksets`, as described in the [Kernel Documentation](https://www.kernel.org/doc/Documentation/kobject.txt).

## Submission

> **Important: Submission rules**
>
> Do **not** share or make your code available before you have received your final grade for this course. You must keep your code private.

The project is a group work, with groups consisting of 3 students. We will set up a Moodle activity to help you find teammates if needed.

**Submission process**  
Projects must be submitted as patches via email, in the same fashion as labs, to `lkp-maintainers@os.rwth-aachen.de`, with the subject `[PATCH X/Y] project: ...` (these are the same instructions as those provided during the lab, but replace *lab0X: task0Y* by *project*).

The submission deadline is **March 9th at 23:59**.

This time, you will submit only the patches applied to the ouichefs module, not patches made to the Linux kernel. We will apply your patches against the latest ouichefs code, and compile your module. Also, you don't have to modify the Makefile, except for the line `ouichefs-objs := fs.o super.o inode.o file.o dir.o ...` if you are adding files to the module.

When submitting, scripts will execute basic tests, make sure you pass them throughout the development of your module. As with the labs, you can submit patches as many times as you want before the deadline. Only the last submission will be used for grading. Passing the tests is not a requirement but is highly encouraged to make sure you are respecting the lab's instructions. Note that not every member of the group needs to submit.

The reference evaluation script for the project can be found [here](https://teaching.os.rwth-aachen.de/LKP/scripts/project.sh).

> **Tip**
>
> You can CC the group member when submitting a patchset, that way, every member of your group will receive the results of the test.

You will be graded on the following points:

- Meaningful patch descriptions
- Code clarity and compliance with the kernel coding style
- Design choices
- Presentation quality
- Compliance with the project instructions
- Code correctness

Bonus points can be obtained if you find a bug in the original repository and if you are able to fix it. To claim you bonus points, make a pull request on [https://github.com/rgouicem/ouichefs](https://github.com/rgouicem/ouichefs) **after the deadline** and before your final presentation.

**Final presentation:**  
You will need to present your work in a short in-person 10' presentation. During this presentation, you will:

1. Present the features you implemented
2. Explain and defend your implementation choices
3. Perform an interactive demonstration (on the examiner's laptop)

The final presentations will be held during **the week of March 10th**. We will provide an appointment booking system later.
