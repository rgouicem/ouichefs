#!/bin/bash
# Script to create two ouichefs disk images and launch QEMU with them
# This script should be run from within the /home/bytemouse/Code/lkp/share/linux-stable/ouichefs directory

# Move to the main lkp directory where QEMU is expected to run from
cd ../../../../

# Define paths
OUICHEFS_DIR="share/linux-stable/ouichefs"
CURRENT_DIR=$(pwd)

# Size of each disk image in MB
DISK_SIZE=50

# Names for the disk images
OUICHE_DISK1="${CURRENT_DIR}/${OUICHEFS_DIR}/ouichefs_disk1.img"
OUICHE_DISK2="${CURRENT_DIR}/${OUICHEFS_DIR}/ouichefs_disk2.img"

# Create disk images
create_ouichefs_disk() {
    local disk_name=$1
    local size_mb=$2
    
    echo "Creating $disk_name ($size_mb MB)..."
    
    # Create an empty disk image
    dd if=/dev/zero of=$disk_name bs=1M count=$size_mb status=progress
    
    # Format it with ouichefs - make sure mkfs.ouichefs is in the path
    # or provide the full path to it
    ${CURRENT_DIR}/${OUICHEFS_DIR}/mkfs/mkfs.ouichefs $disk_name
    
    echo "$disk_name created successfully"
}

# Always create both disk images, overwriting existing ones
echo "Note: This will overwrite any existing disk images"

# Create first disk image
create_ouichefs_disk "$OUICHE_DISK1" "$DISK_SIZE"

# Create second disk image
create_ouichefs_disk "$OUICHE_DISK2" "$DISK_SIZE"

echo "Disk images created. Starting QEMU..."

# Fix the paths if necessary
HDA="-drive file=lkp-arch.img,format=raw"
HDB="-drive file=myHome.img,format=raw"
HDC="-drive file=${OUICHE_DISK1},format=raw"
HDD="-drive file=${OUICHE_DISK2},format=raw"
SHARED="./share"
KERNEL="./share/linux-stable/arch/x86/boot/bzImage"

if [ -z ${KDB} ]; then
    CMDLINE='root=/dev/sda1 rw console=ttyS0 kgdboc=ttyS1'
else
    CMDLINE='root=/dev/sda1 rw console=ttyS0 kgdboc=ttyS1 kgdbwait'
fi

FLAGS="--enable-kvm "
VIRTFS+=" --virtfs local,path=${SHARED},mount_tag=share,security_model=mapped-xattr,id=share "

echo "Once in the VM, mount the ouichefs disks with:"
echo "  sudo mkdir -p /mnt/ouichefs1 /mnt/ouichefs2"
echo "  sudo mount /dev/sdc /mnt/ouichefs1 -t ouichefs"
echo "  sudo mount /dev/sdd /mnt/ouichefs2 -t ouichefs"
echo ""
echo "Starting QEMU now..."

# Launch QEMU directly
exec qemu-system-x86_64 ${FLAGS} \
     ${HDA} ${HDB} ${HDC} ${HDD} \
     ${VIRTFS} \
     -net user -net nic \
     -serial mon:stdio -serial tcp::1234,server,nowait \
     -boot c -m 1G \
     -kernel "${KERNEL}" \
     -append "${CMDLINE}"
