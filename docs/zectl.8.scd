zectl(8)

# NAME

zectl - ZFS Boot Environment manager

# DESCRIPTION

zectl is a ZFS Boot Environment manager for Linux.

# SYNOPSIS

*zectl version*

*zectl activate* <boot-environment>

*zectl create* [ -e <existing-dataset> | <existing-dataset@snapshot> ] [ -r ] <boot-environment>

*zectl destroy* [ -F ] <boot-environment>

*zectl get* [ -H ] [ property ]

*zectl list* [ -H ]

*zectl mount* <boot-environment>

*zectl rename* <boot-environment> <boot-environment-new>

*zectl set* <property>=<value>

*zectl snapshot* <boot-environment>@<snapshot>

*zectl unmount* <boot-environment>

# COMMANDS

*zectl version*
	Print zectl version.

*zectl activate* <boot-environment>
	Activate _boot-environment_.

*zectl create* [ -e <existing-dataset> | <existing-dataset@snapshot> ] [ -r ] <boot-environment>
	Create _boot-environment_.

	_-e_ will create the new boot environment from an existing boot environment,
	or snapshot of an existing boot environment - see *zectl snapshot*.

	_-r_ will create a recursive boot environment.

*zectl destroy* [ -F ] <boot-environment>
	Destroy _boot-environment_.

	_-F_ forcefully unmounts and destroys _boot-environment_.

*zectl get* [ -H ] [ property ]
	Get zfs properties associated with _zectl_.

	_-H_ outputs tab delimited data and removes headers.

	Unset properties will output their default settings.

	Specifying a property outputs only the requested property. Individual
	properties should be requested without the fully qualified prefix.

*zectl list* [ -H ]
	List boot environments.

	_-H_ outputs tab delimited data and removes headers.

	The _Active_ column displays an _N_ on the boot environment currently
	booted, and a _R_ on the activate boot environment.

*zectl mount* <boot-environment>
	Mount _boot-environment_ and output the mount location to _stdout_.

*zectl rename* <boot-environment> <boot-environment-new>
	Rename _boot-environment_ to _boot-environment-new_. Currently booted, or
	active boot environments cannot be renamed.

*zectl set* <property>=<value>
	Set a zfs property for _zectl_.

*zectl snapshot* <boot-environment>@<snapshot>
	Snapshot _boot-environment_.

	*zectl snapshot* should be used for correct restoration of a snapshot
	as opposed to using *zfs snapshot* directly. Some plug-ins may require extra
	steps during the *zectl snapshot* command to ensure correct restoration via
	*zectl create*.

*zectl unmount* <boot-environment>
	Unmount <boot-environment>. Currently booted boot environments cannot be
	unmounted.

# SEE ALSO

zfsprops(7), zfs-set(8), zfs(8)

