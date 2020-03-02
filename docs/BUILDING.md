## Requirements

* ZFS
* `cmake` 3.10+
* `check` library (if running tests)

## Building

The `PLUGINS_DIRECTORY` can be used to set the location for plugins.

To setup the build and set `PLUGINS_DIRECTORY`, starting from the source root:

```shell script
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DPLUGINS_DIRECTORY=/usr/share/zectl/libze_plugin
```

To run tests add `-DBUILD_TESTING=1`.

To compile and install to a subdirectory `release`:

```shell script
make
make DESTDIR="release/" install
```

The files can now be copied to the system locations.
