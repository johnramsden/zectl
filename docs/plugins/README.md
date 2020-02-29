Plugins
========

As of now plugins for the following bootloaders exist:

* [systemdboot](systemdboot.md)

In order to integrate with a bootloader, set the `org.zectl:bootloader` property
on your boot environments, and the bootloader plugin will be used.

```shell script
$ zectl set bootloader=<plugin>
```
