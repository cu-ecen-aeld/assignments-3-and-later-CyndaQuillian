#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

# Define kernel version, architecture, and BusyBox version
KERNEL_VERSION=v5.15
KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
BUSYBOX_VERSION=1_33_1

# Set the FINDER_APP_DIR to the correct path 
FINDER_APP_DIR=$(readlink -f "$(dirname "$0")")
echo "Using finder-app directory at ${FINDER_APP_DIR}"

# Default directory if not passed as argument
DEFAULT_OUTDIR="/tmp/aeld"

# Use the passed argument for OUTDIR or the default one
OUTDIR=${1:-$DEFAULT_OUTDIR}
OUTDIR=$(realpath "$OUTDIR")  # Get the absolute path

echo "Using directory ${OUTDIR} for output"

# Create OUTDIR if it doesn't exist, fail if it can't be created
if [ ! -d "$OUTDIR" ]; then
    echo "Directory ${OUTDIR} does not exist. Creating it..."
    mkdir -p "$OUTDIR" || { echo "Failed to create directory ${OUTDIR}"; exit 1; }
else
    echo "Directory ${OUTDIR} already exists."
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    # Clone only if the repository does not exist.
    echo "Cloning Linux kernel source into ${OUTDIR}/linux"
    git clone --depth 1 --single-branch --branch ${KERNEL_VERSION} ${KERNEL_REPO} linux-stable
fi

# Navigate to the kernel source directory
cd "${OUTDIR}/linux-stable"
echo "Checking out version ${KERNEL_VERSION}"
git checkout ${KERNEL_VERSION}

# Build the Kernel Image
echo "Building Kernel Image"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)

# Copy resulting Image to outdir
echo "Copying Kernel Image to ${OUTDIR}"
cp arch/${ARCH}/boot/Image ${OUTDIR}/

# Create the root filesystem in outdir/rootfs
echo "Creating the staging directory for the root filesystem"
if [ -d "${OUTDIR}/rootfs" ]; then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi
mkdir -p ${OUTDIR}/rootfs

# Build the root filesystem
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]; then
    echo "Cloning BusyBox"
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    echo "Configuring BusyBox"
    make defconfig
else
    cd busybox
fi

echo "Building BusyBox"

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

# Copy the necessary files to the root filesystem
echo "Copying files to root filesystem"
cd "$OUTDIR/rootfs"
mkdir -p home/ bin/ sbin/ etc/ proc/ sys/ dev/ lib/ lib64/ usr/bin/ usr/sbin/ home/conf/
cp -r "$FINDER_APP_DIR/finder.sh" home/
cp -r "$FINDER_APP_DIR/conf/username.txt" home/conf/username.txt
cp -r "$FINDER_APP_DIR/conf/assignment.txt" home/conf/assignment.txt
cp -r "$FINDER_APP_DIR/finder-test.sh" home/
cp -r "$FINDER_APP_DIR/writer.sh" home/
cp -r "$FINDER_APP_DIR/writer.c" home/
cp -r "$FINDER_APP_DIR/makefile" home/
sed -i 's|../conf/assignment.txt|/home/assignment.txt|' home/finder-test.sh
sed -i 's|./writer.sh|./writer|' home/finder-test.sh

# Copy library dependencies
echo "Copying library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"
ARCH_SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp ${ARCH_SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/.
cp ${ARCH_SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/.
cp ${ARCH_SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/.
cp ${ARCH_SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/.

# Cross compile and copy the writer application from Assignment 2
echo "Cross-compiling and copying the writer application"
cd "$FINDER_APP_DIR"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} writer
cp "$FINDER_APP_DIR/writer" "$OUTDIR/rootfs/home/"

# Copy the autorun-qemu.sh script
echo "Copying autorun-qemu.sh"
cp "$FINDER_APP_DIR/autorun-qemu.sh" "$OUTDIR/rootfs/home/"
chmod +x "$OUTDIR/rootfs/home/autorun-qemu.sh"

cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# Create initramfs.cpio.gz based on the root filesystem and nodes
echo "Creating initramfs"
cd "$OUTDIR/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1
sudo mknod -m 666 dev/tty c 5 0
sudo mknod -m 666 dev/tty0 c 4 0
sudo mknod -m 666 dev/tty1 c 4 1
sudo mknod -m 666 dev/tty2 c 4 2
sudo mknod -m 666 dev/tty3 c 4 3
sudo mknod -m 666 dev/tty4 c 4 4

find . | cpio -H newc -ov --owner root:root | gzip > ${OUTDIR}/initramfs.cpio.gz

echo "Kernel image and initramfs created successfully."
