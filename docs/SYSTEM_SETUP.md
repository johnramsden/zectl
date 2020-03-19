# System Setup

In order to use boot environments your system needs to be set up in a certain
manner. 

The main dataset that is used for your root file system, can also be thought
of as your boot environment. Anything in this dataset, or in a dataset under
it, is what constitutes a boot environment. 

## Dataset Configuration 

To put your system in a compatible configuration, your boot environments for
your system should be kept in a 'Boot Environment root'. In most configurations
this would be `<pool>/ROOT`. However its location is not important, and it
can be located anywhere within a pool. What's important is that it does not
have any child datasets that are not in a boot environment.

The common practice is to start with a 'default' boot environment. This would
be the dataset `<pool>/ROOT/default`. If a system is setup in this
manner, it would be the most basic boot environment compatible system.

Make sure the boot environment is set as the bootfs:

```shell script
zpool set bootfs="<pool>/ROOT/default" "<pool>"
```

If you have any existing boot environments, make sure they have the property
`canmount=noauto` set.

```shell script
zfs set canmount=noauto "<pool>/ROOT/<be>"
```