# zectl

[![MIT license](http://img.shields.io/badge/license-MIT-brightgreen.svg)](http://opensource.org/licenses/MIT)

[![Matrix Chat](https://img.shields.io/badge/chat-on%20%5Bmatrix%5D-brightgreen)](https://riot.im/app/#/room/#zectl:matrix.org "Matrix chat")

ZFS Boot Environment manager

---

`zectl` is a ZFS boot environment manager for Linux.

ZFS lets you create multiple filesystems which are known has datasets. Boot Environments take advantage of how low cost clones can be taken of a root dataset. These clones are effectively forking points where all prior data is shared between the origin dataset and the clone, and any new data will be stored separately.

After creating a boot environment, when it is ready to be used, it is activated. After activation, once the system is rebooted, all new data will be stored in the new boot environment's dataset. These boot environments can be integrated into the bootloader so that they can be selected at the boot menu, as if they are separate installs of an operating system.

## Bootloaders

`zectl` is written in a modular way that supports adding additional bootloaders with "plugins".

`zectl` currently only has a systemdboot plugin.

See [plugins](docs/plugins/README.md)

## Installing

`zectl` is available in on Arch linux:
* [zectl](https://aur.archlinux.org/packages/zectl/)
* [zectl-git](https://aur.archlinux.org/packages/zectl-git/)

To build manually see [BUILDING](docs/BUILDING.md)

## System Setup

See:
* [System Setup](docs/SYSTEM_SETUP.md)
* [plugins](docs/plugins/README.md)
