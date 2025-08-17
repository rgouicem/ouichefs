#!/bin/bash -e
modulename=ouichefs

sysfs="/sys/fs/ouichefs"
# sysfs="/sys/ouichefs"

exit_fail() {
	echo "test failed: $1"
	exit 1
}

# this script requires two drives to be attached to the VM
# under /dev/vda and /dev/vdb
mnta=/tmp/mnt1
mntb=/tmp/mnt2
mkdir -p $mnta
mkdir -p $mntb

check_ouichefs_basic_behavior() {
	set -x
	modprobe $modulename

	mount /dev/vda $mnta
	cd $mnta
	rm -fr $mnta/*

	mkdir dir
	echo "some data" > dir/file
	echo "some data" > file
	echo "some data" > file2
	mv file2 dir
	echo "some more data" > file

	ls -Ril > /dev/null
	rm file
	rm -fr dir
	ls -Ril > /dev/null

	rm -fr $mnta/*
	touch file{1..65}
	mkdir dir2
	mv file{1..65} dir2
	rm -fr $mnta/*

	echo "data" > file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	echo "data" >> file
	rm file

	cd
	umount $mnta
	rmmod $modulename
}

check_ouichefs_fio() {
	set -x
	modprobe $modulename

	mount /dev/vda $mnta
	cd $mnta
	rm -fr $mnta/*

	echo "[global]
nrfiles=10
directory=$mnta
size=16M
runtime=20s
ioengine=sync
time_based
bs=4k
verify=sha1
verify_fatal=1

group_reporting

[rw]
rw=randrw
numjobs=1

[rwappend]
file_append=1
rw=randrw
numjobs=1" > /tmp/workload.fio

	fio -f /tmp/workload.fio > /dev/null
	cd
	rm -fr $mnta/*
	umount $mnta
	rmmod $modulename
}

check_sysfs_structure() {
	set -x
	modprobe $modulename

	mount /dev/vda $mnta
	mount /dev/vdb $mntb

	for dev in vda vdb; do

		if ! [ -d "$sysfs/$dev" ]; then
			exit_fail "$sysfs/$dev not created!"
		fi

		for file in free_blocks used_blocks sliced_blocks total_free_slices files small_files total_data_size total_used_size efficiency ; do
			variable="$sysfs/$dev/$file"
			if ! [ -f "$variable" ]; then
				exit_fail "$variable not created!"
			fi
			if ! cat "$variable" > /dev/null; then
				exit_fail "error while reading ${variable}!"
			fi
		done
	done

	umount $mnta
	umount $mntb

	for dev in vda vdb; do
		if [ -d "$sysfs/$dev" ]; then
			exit_fail "$sysfs/$dev still present after unmount!"
		fi
	done

	rmmod $modulename
}

check_simple_write() {
	set -x
	modprobe $modulename

	mount /dev/vda $mnta
	mount /dev/vdb $mntb

	variable_used="$sysfs/vda/used_blocks"
	variable_small_files="$sysfs/vda/small_files"
	variable_free_slices="$sysfs/vda/total_free_slices"
	variable_slices="$sysfs/vda/sliced_blocks"

	# make sure there's at least 1 filled block allocated
	dd if=/dev/urandom of=$mnta/file0 bs=48 count=1 2>/dev/null

	if ! used_before=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! free_slices_before=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! small_files_before=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi

	# allocate a new file
	dd if=/dev/urandom of="$mnta/file1" bs=4k count=1 2>/dev/null

	if ! used_after=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! [ "$((used_before + 2))" -eq "$used_after"  ]; then
		exit_fail ">error while counting $variable_used: $used_before $used_after"
	fi

	if ! used_before=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi

	dd if=/dev/urandom of=$mnta/file2 bs=18 count=1 2>/dev/null
	dd if=/dev/urandom of=$mnta/file3 bs=48 count=1 2>/dev/null
	dd if=/dev/urandom of=$mnta/file4 bs=127 count=1 2>/dev/null

	if ! used_after=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_after=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_after=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi
	if ! [ "$((free_slices_before - 3))" -eq "$free_slices_after"  ]; then
		exit_fail ">>error while counting $variable_free_slices: $free_slices_before $free_slices_after"
	fi
	if ! [ "$used_before" -eq "$used_after"  ]; then
		exit_fail ">>error while counting $variable_used: $used_before $used_after"
	fi
	if ! [ "$((small_files_before + 3))" -eq "$small_files_after"  ]; then
		exit_fail ">>error while counting $variable_small_files: $small_files_before $small_files_after"
	fi
	if ! [ "$((slices_after))" -eq "$slices_before"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi

	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	for i in $(seq 45); do
		dd if=/dev/urandom of="$mnta/_file$i" bs=128 count=1 2>/dev/null
	done
	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! [ "$((slices_after))" -eq "$((slices_before + 1))"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi

	umount $mnta
	umount $mntb

	for dev in vda vdb; do
		if [ -d "$sysfs/$dev" ]; then
			exit_fail "$sysfs/$dev still present after unmount!"
		fi
	done

	rmmod $modulename
}

check_remove() {
	set -x
	modprobe $modulename

	mount /dev/vda $mnta
	mount /dev/vdb $mntb

	variable_used="$sysfs/vda/used_blocks"
	variable_small_files="$sysfs/vda/small_files"
	variable_free_slices="$sysfs/vda/total_free_slices"
	variable_slices="$sysfs/vda/sliced_blocks"

	if ! used_before=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_before=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_before=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi

	rm "$mnta/"file{0,2,3,4}

	if ! used_after=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_after=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_after=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi

	if ! [ "$used_before" -eq "$used_after"  ]; then
		exit_fail ">>error while counting $variable_used: $used_before $used_after"
	fi
	if ! [ "$((small_files_before - 4))" -eq "$small_files_after"  ]; then
		exit_fail ">>error while counting $variable_small_files: $small_files_before $small_files_after"
	fi
	if ! [ "$((slices_after))" -eq "$slices_before"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi
	if ! [ "$((free_slices_before + 4))" -eq "$free_slices_after"  ]; then
		exit_fail ">>error while counting $variable_free_slices: $free_slices_before $free_slices_after"
	fi

	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	for i in $(seq 1 44); do
		rm "$mnta/_file$i"
	done
	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! [ "$((slices_after))" -eq "$((slices_before - 1))"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi


	umount $mnta
	umount $mntb

	for dev in vda vdb; do
		if [ -d "$sysfs/$dev" ]; then
			exit_fail "$sysfs/$dev still present after unmount!"
		fi
	done

	rmmod $modulename
}

check_coexistence_trad_fs() {
	set -x
	modprobe $modulename

	mount /dev/vda $mnta
	mount /dev/vdb $mntb

	dd if=/dev/urandom of=$mntb/file1 bs=128 count=1 2>/dev/null

	variable_used="$sysfs/vdb/used_blocks"
	variable_small_files="$sysfs/vdb/small_files"
	variable_free_slices="$sysfs/vdb/total_free_slices"
	variable_slices="$sysfs/vdb/sliced_blocks"

	if ! used_before=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_before=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_before=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi

	dd if=/dev/urandom of=$mntb/file3 bs=128 count=1 2>/dev/null

	if ! used_after=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_after=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_after=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi
	if ! [ "$((free_slices_before - 1))" -eq "$free_slices_after"  ]; then
		exit_fail ">>error while counting $variable_free_slices: $free_slices_before $free_slices_after"
	fi
	if ! [ "$used_before" -eq "$used_after"  ]; then
		exit_fail ">>error while counting $variable_used: $used_before $used_after"
	fi
	if ! [ "$((small_files_before + 1))" -eq "$small_files_after"  ]; then
		exit_fail ">>error while counting $variable_small_files: $small_files_before $small_files_after"
	fi
	if ! [ "$((slices_after))" -eq "$slices_before"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi


	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_before=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_before=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi

	dd if=/dev/urandom of=$mntb/file3 bs=4k count=1 2>/dev/null

	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_after=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_after=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi
	if ! [ "$((small_files_before - 1))" -eq "$small_files_after"  ]; then
		exit_fail ">>error while counting $variable_small_files: $small_files_before $small_files_after"
	fi
	if ! [ "$((free_slices_before + 1))" -eq "$free_slices_after"  ]; then
		exit_fail ">>error while counting $variable_free_slices: $free_slices_before $free_slices_after"
	fi
	if ! [ "$((slices_after))" -eq "$slices_before"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi

	umount $mnta
	umount $mntb

	for dev in vda vdb; do
		if [ -d "$sysfs/$dev" ]; then
			exit_fail "$sysfs/$dev still present after unmount!"
		fi
	done

	rmmod $modulename
}

check_spanning_multiple_slices() {
	set -x
	modprobe $modulename

	mount /dev/vda $mnta
	mount /dev/vdb $mntb

	variable_used="$sysfs/vdb/used_blocks"
	variable_small_files="$sysfs/vdb/small_files"
	variable_free_slices="$sysfs/vdb/total_free_slices"
	variable_slices="$sysfs/vdb/sliced_blocks"

	dd if=/dev/urandom of=$mntb/file1 bs=100 count=1 2>/dev/null

	if ! used_before=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_before=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_before=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi

	dd if=/dev/urandom of=$mntb/file2 bs=100 count=1 oflag=append conv=notrunc 2>/dev/null
	dd if=/dev/urandom of=$mntb/file2 bs=128 count=1 oflag=append conv=notrunc 2>/dev/null

	if ! used_after=$(cat $variable_used); then
		exit_fail "error while reading ${variable_used}!"
	fi
	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_after=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_after=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi
	if ! [ "$((free_slices_before - 2))" -eq "$free_slices_after"  ]; then
		exit_fail ">>error while counting $variable_free_slices: $free_slices_before $free_slices_after"
	fi
	if ! [ "$used_before" -eq "$used_after"  ]; then
		exit_fail ">>error while counting $variable_used: $used_before $used_after"
	fi
	if ! [ "$((small_files_before + 1))" -eq "$small_files_after"  ]; then
		exit_fail ">>error while counting $variable_small_files: $small_files_before $small_files_after"
	fi
	if ! [ "$((slices_after))" -eq "$slices_before"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi

	if ! slices_before=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_before=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_before=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi

	dd if=/dev/urandom of=$mntb/file2 bs=128 count=1 oflag=append conv=notrunc 2>/dev/null
	dd if=/dev/urandom of=$mntb/file2 bs=128 count=1 oflag=append conv=notrunc 2>/dev/null
	dd if=/dev/urandom of=$mntb/file2 bs=128 count=1 oflag=append conv=notrunc 2>/dev/null

	if ! slices_after=$(cat $variable_slices); then
		exit_fail "error while reading ${variable_slices}!"
	fi
	if ! free_slices_after=$(cat $variable_free_slices); then
		exit_fail "error while reading ${variable_free_slices}!"
	fi
	if ! small_files_after=$(cat $variable_small_files); then
		exit_fail "error while reading ${variable_small_files}!"
	fi
	if ! [ "$((small_files_before))" -eq "$small_files_after"  ]; then
		exit_fail ">>error while counting $variable_small_files: $small_files_before $small_files_after"
	fi
	if ! [ "$((free_slices_before - 3))" -eq "$free_slices_after"  ]; then
		exit_fail ">>error while counting $variable_free_slices: $free_slices_before $free_slices_after"
	fi
	if ! [ "$((slices_after))" -eq "$slices_before"  ]; then
		exit_fail ">>error while counting $variable_slices: $slices_before $slices_after"
	fi

	umount $mnta
	umount $mntb

	for dev in vda vdb; do
		if [ -d "$sysfs/$dev" ]; then
			exit_fail "$sysfs/$dev still present after unmount!"
		fi
	done

	rmmod $modulename

}
check_memory_leak() {
	set -x
	check_ouichefs_basic_behavior
	check_ouichefs_fio
	check_sysfs_structure
	check_simple_write
	check_remove
	check_coexistence_trad_fs
	check_spanning_multiple_slices
	check_ouichefs_basic_behavior

	echo scan > /sys/kernel/debug/kmemleak
	sleep 60
	if [ -s /sys/kernel/debug/kmemleak ]; then
		cat /sys/kernel/debug/kmemleak
		exit 1
	fi
}
