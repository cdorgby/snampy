#!/usr/bin/env bash

# fix up permissions and add aditional packages into a combostick.

set -e
set -x

if [ `id -u` -ne 0 ]; then
    echo "Must run as root (or with sudo)"
    exit 1
fi

if [ $# -lt 2 ]; then
    echo "$0 <path to disk.img> <path to pkgs to include>"
    exit 1
fi

_img=$1
shift
_pkgs=$*

_loop=`losetup -f`

losetup -P $_loop $_img

if [ ! $? ]; then
    echo "Can't setup loop device to $1"
    exit 1
fi

set -e
trap "losetup --detach $_loop" ERR EXIT

_tmpdir=`mktemp -dq`
if [ ! $? ]; then
    echo "Can't get a temp directory"
    exit 1
fi

function cleanup
{
    umount $_tmpdir || true
    rm -fr $_tmpdir
    losetup --detach $_loop
}

function do_stuff
{
    for _i in $_pkgs; do
        cp $_i $_tmpdir
        chmod 664 $_tmpdir/`basename $_i`
    done

    chown root:root -R $_tmpdir
    chown root:2002 $_tmpdir/boot
    chown root:2002 $_tmpdir/boot/grub
    chown root:2002 $_tmpdir/boot/grub/grub.cfg
    chmod 775 $_tmpdir/boot/grub
    chmod 664 $_tmpdir/boot/grub/grub.cfg
}

trap cleanup ERR EXIT

mount "${_loop}p1" $_tmpdir
do_stuff

umount $_tmpdir
mount "${_loop}p2" $_tmpdir
do_stuff