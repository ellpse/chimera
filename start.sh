#!/bin/bash
set -e
#some dude from stackoverflows code**
KERNEL_DIR="$HOME/linux"
KERNEL_IMG="$KERNEL_DIR/arch/x86/boot/bzImage"
INITRAMFS_SRC="$KERNEL_DIR/initramfs"
INITRAMFS_OUT="$KERNEL_DIR/initramfs.cpio.gz"
DISK_IMG="virtual_disk.img"

if [ ! -d "$INITRAMFS_SRC" ] || [ ! -f "$INITRAMFS_SRC/init" ]; then
    echo "Error: 'init' binary not found in $INITRAMFS_SRC"
    exit 1
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "Error: Kernel image not found at $KERNEL_IMG"
    exit 1
fi

if [ ! -f "$DISK_IMG" ]; then
    echo "Creating a 10GB virtual storage disk..."
    dd if=/dev/zero of="$DISK_IMG" bs=1M count=10240
    mkfs.ext4 -F "$DISK_IMG"
fi

echo "Packaging changes to $INITRAMFS_SRC..."
(
    cd "$INITRAMFS_SRC"
    find . -print0 | cpio --null --create --format=newc | gzip -9 > "$INITRAMFS_OUT"
)

echo "Checking system capabilities..."
QEMU_ARGS=(
    -kernel "$KERNEL_IMG"
    -initrd "$INITRAMFS_OUT"
    -drive file="$DISK_IMG",format=raw,if=ide
    -append "root=/dev/sda rdinit=/init console=ttyS0 quiet ip=dhcp"
    -nographic
    -m 2G
    -netdev user,id=net0
    -device virtio-net-pci,netdev=net0
)

if [ -e /dev/kvm ] && [ -w /dev/kvm ]; then
    echo "KVM hardware acceleration is available."
    QEMU_ARGS+=(-smp 2 -cpu host -enable-kvm)
else
    echo "KVM not available. Falling back to software emulation."
    QEMU_ARGS+=(-smp 2)
fi

echo "Booting QEMU in the current shell..."
echo "--------------------------------------------------------"
exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
