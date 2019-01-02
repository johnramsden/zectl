#!/bin/sh

# Travis setup script (16.04

# Install zfs requirements
add-apt-repository -y ppa:jonathonf/zfs && \
apt-get -q update && \
apt-get install -y linux-headers-$(uname -r) && \
apt-get install -y spl-dkms zfs-dkms \
                   zfsutils-linux libzfslinux-dev \
                   build-essential \
                   autoconf \
                   libtool \
                   gawk \
                   alien \
                   fakeroot \
                   zlib1g-dev \
                   uuid-dev \
                   libattr1-dev \
                   libblkid-dev \
                   libselinux-dev \
                   libudev-dev \
                   libdevmapper-dev \
                   || exit 1
