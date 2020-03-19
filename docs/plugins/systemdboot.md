# systemdboot

Multiple kernels can be managed with this plugin and systemd-boot, but it will
require changing the mountpoint of the esp (EFI System Partition).

## Requirements

* Create configuration for currently active boot environment
* Copy kernels to new directory
* Create fstab entries

## Problem With Regular Mountpoint

Usually the `$esp` would get mounted at `/boot`, `/boot/efi`, or `/efi`. The
kernels would sit in the root of the `$esp`, and the configs for systemdboot
in `$esp/loader/`.

```
$esp 
. 
├── initramfs-linux-fallback.img 
├── initramfs-linux.img 
├── intel-ucode.img 
├── vmlinuz-linux 
│ 
└── loader/ 
    ├── entries/ 
    │   └── arch.conf 
    └── loader.conf 
```

The configs would then reference kernels in the root directory.

```
title           Arch Linux 
linux           vmlinuz-linux 
initrd          intel-ucode.img 
initrd          initramfs-linux.img 
options         zfs=bootfs rw 
```

The problem with this method, is multiple kernels cannot be kept at the same
time. Therefore this hierarchy is not conducive to boot environments.

## Alternate Mountpoint

First, remount the `$esp` to a new location, the default is `/efi`.

If you would like to explicitly specify the mountpoint used, you can set the
`systemdboot:efi` property on your current boot environment, and the plugin
will use the specified location: 

```shell script
$ zectl set systemdboot:efi=/efi
```

Don't forget to change the mount point in `/etc/fstab`.

```
UUID=9F8A-F566    /efi    vfat    rw,defaults,errors=remount-ro  0 2
```

Now, make a subdirectory `$esp/env`, kernels will be kept in a subdirectory
of this location.

Copy your current kernels to `$esp/env/org.zectl-<boot environment>/`

The bootloader configuration can now use a different path for each boot
environment.

So the 'default' boot environment config, located at
`$esp/loader/entries/org.zectl-default.conf`, would look something like:

```
title           Arch Linux
linux           /env/org.zectl-default/vmlinuz-linux
initrd          /env/org.zectl-default/intel-ucode.img
initrd          /env/org.zectl-default/initramfs-linux.img
options         zfs=zpool/ROOT/default rw
```

Create this configuration file.

To make the system happy when it looks for kernels at `/boot`, this directory
should be bindmounted to `/boot`.

If your system uses a different directory for kernels it can be set with:

```shell script
$ zectl set systemdboot:boot=<kernel directory>
```

Bindmount `$esp/env/org.zectl-default` to `/boot` in `/etc/fstab`.

```
/efi/env/org.zectl-default   /boot     none    rw,defaults,errors=remount-ro,bind    0 0
```

If this directory is not here, the kernels will not be updated when the system
rebuilds the kernel.

Once a system is set up in the proper configuration, `zectl` will update
the bootloader, and fstab when a new boot environment is created.

It will also copy the configuration described above, replacing the currently
activated boot environment's configuration with the new boot environments name.

