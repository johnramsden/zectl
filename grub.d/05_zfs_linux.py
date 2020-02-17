
# encoding: utf-8
# cython: language_level=3, c_string_type=unicode, c_string_encoding=default

import sys

import os
import platform
# import pyzfscmds.system.agnostic
# import pyzfscmds.utility
import re
import subprocess
import functools
from distutils.version import LooseVersion

from ctypes import *
cimport libze

# import zedenv.lib.be
# import zedenv.lib.check
# import zedenv.lib.configure

# import zedenv_grub.grub

from typing import List, Optional


def source(file: str):
    """
    'sources' a file and manipulates environment variables
    """

    env_command = ['sh', '-c', f'set -a && . {file} && env']

    try:
        env_output = subprocess.check_output(
            env_command, universal_newlines=True, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to source{file}.\n{e}\n.")

    for line in env_output.splitlines():
        (key, _, value) = line.partition("=")
        os.environ[key] = value


def normalize_string(str_input: str):
    """
    Given a string, remove all non alphanumerics, and replace spaces with underscores
    """
    str_list = []
    for c in str_input.split(" "):
        san = [lo.lower() for lo in c if lo.isalnum()]
        str_list.append("".join(san))

    return "_".join(str_list)


def grub_command(command: str, call_args: List[str] = None, stderr=subprocess.PIPE):
    cmd_call = [command]
    if call_args:
        cmd_call.extend(call_args)

    try:
        cmd_output = subprocess.check_output(
            cmd_call, universal_newlines=True, stderr=stderr)
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to run {command}.\n{e}.")

    return cmd_output.splitlines()


class GrubLinuxEntry:

    def __init__(self, linux: str,
                 grub_os: str,
                 be_root: str,
                 rpool: str,
                 genkernel_arch: str,
                 boot_environment_kernels: dict,
                 grub_cmdline_linux: str,
                 grub_cmdline_linux_default: str,
                 grub_devices: Optional[List[str]],
                 default: str,
                 grub_boot_on_zfs: bool,
                 grub_device_boot: str):

        self.grub_cmdline_linux = grub_cmdline_linux
        self.grub_cmdline_linux_default = grub_cmdline_linux_default
        self.grub_devices = grub_devices
        self.grub_device_boot = grub_device_boot

        self.grub_boot_on_zfs = grub_boot_on_zfs

        self.linux = linux
        self.grub_os = grub_os
        self.genkernel_arch = genkernel_arch

        self.basename = os.path.basename(linux)
        self.dirname = os.path.dirname(linux)

        self.boot_environment_kernels = boot_environment_kernels

        try:
            self.rel_dirname = grub_command("grub-mkrelpath", [self.dirname])[0]
        except RuntimeError as e:
            sys.exit(e)
        self.version = self.get_linux_version()

        self.rpool = rpool
        self.be_root = be_root
        self.boot_environment = self.get_boot_environment()

        # Root dataset will double as device ID
        self.linux_root_dataset = os.path.join(
            self.be_root, self.boot_environment)
        self.linux_root_device = f"ZFS={self.linux_root_dataset}"
        self.boot_device_id = self.linux_root_dataset

        self.initrd_early = self.get_initrd_early()
        self.initrd_real = self.get_initrd_real()

        self.kernel_config = self.get_kernel_config()

        self.initramfs = self.get_from_config(r'CONFIG_INITRAMFS_SOURCE=(.*)$')

        self.grub_default_entry = None
        if "GRUB_ACTUAL_DEFAULT" in os.environ:
            self.grub_default_entry = os.environ['GRUB_ACTUAL_DEFAULT']

        self.grub_save_default = None
        if "GRUB_SAVEDEFAULT" in os.environ:
            self.grub_save_default = True if os.environ['GRUB_SAVEDEFAULT'] == "true" else False

        self.grub_gfxpayload_linux = None
        if "GRUB_GFXPAYLOAD_LINUX" in os.environ:
            self.grub_gfxpayload_linux = os.environ['GRUB_GFXPAYLOAD_LINUX']

        self.grub_enable_cryptodisk = None
        if "GRUB_ENABLE_CRYPTODISK" in os.environ:
            if os.environ['GRUB_ENABLE_CRYPTODISK'] == 'y':
                self.grub_enable_cryptodisk = True
            else:
                self.grub_enable_cryptodisk = False

        self.default = default

        self.grub_entries = []

    @staticmethod
    def entry_line(entry_line: str, submenu_indent: int = 0):
        return ("\t" * submenu_indent) + entry_line

    def prepare_grub_to_access_device(self) -> Optional[List[str]]:
        """
        Get device modules to load, replicates function from grub-mkconfig_lib
        """
        lines = []

        # Below not needed since not running on net/open{bsd}
        """
          old_ifs="$IFS"
          IFS='
        '
          partmap="`"${grub_probe}" --device $@ --target=partmap`"
          for module in ${partmap} ; do
            case "${module}" in
              netbsd | openbsd)
                echo "insmod part_bsd";;
              *)
                echo "insmod part_${module}";;
            esac
          done
        """

        """
          # Abstraction modules aren't auto-loaded.
          abstraction="`"${grub_probe}" --device $@ --target=abstraction`"
        """

        if self.grub_boot_on_zfs and not zedenv.lib.be.extra_bpool():
            devices = self.grub_devices
        else:
            devices = self.grub_device_boot

        try:
            abstraction = grub_command("grub-probe",
                                       ['--device', *devices, '--target=abstraction'])
        except RuntimeError:
            pass
        else:
            lines.extend([f"insmod {m}" for m in abstraction if m.strip() != ''])

        """
          fs="`"${grub_probe}" --device $@ --target=fs`"
        """
        try:
            fs = grub_command("grub-probe",
                              ['--device', *devices, '--target=fs'])
        except RuntimeError:
            pass
        else:
            lines.extend([f"insmod {f}" for f in fs if f.strip() != ''])

        """
        if [ x$GRUB_ENABLE_CRYPTODISK = xy ]; then
          for uuid in `"${grub_probe}" --device $@ --target=cryptodisk_uuid`; do
          echo "cryptomount -u $uuid"
          done
        fi
        """
        if self.grub_enable_cryptodisk:
            try:
                crypt_uuids = grub_command("grub-probe",
                                           ['--device', *devices,
                                            '--target=cryptodisk_uuid'])
            except RuntimeError:
                pass
            else:
                lines.extend(
                    [f"cryptomount -u {uuid}" for uuid in crypt_uuids if uuid.strip() != ''])

        """
          # If there's a filesystem UUID that GRUB is capable of identifying, use it;
          # otherwise set root as per value in device.map.
          fs_hint="`"${grub_probe}" --device $@ --target=compatibility_hint`"
          if [ "x$fs_hint" != x ]; then
            echo "set root='$fs_hint'"
          fi
        """
        try:
            fs_hint = grub_command("grub-probe",
                                   ['--device',
                                    *devices,
                                    '--target=compatibility_hint'])
        except RuntimeError:
            pass
        else:
            if fs_hint and fs_hint[0].strip() != '':
                hint = ''.join(fs_hint).strip()
                lines.append(f"set root='{hint}'")
        r"""
          if fs_uuid="`"${grub_probe}" --device $@ --target=fs_uuid 2> /dev/null`" ; then
            hints="`"${grub_probe}" --device $@ --target=hints_string 2> /dev/null`" || hints=
            echo "if [ x\$feature_platform_search_hint = xy ]; then"
            echo "  search --no-floppy --fs-uuid --set=root ${hints} ${fs_uuid}"
            echo "else"
            echo "  search --no-floppy --fs-uuid --set=root ${fs_uuid}"
            echo "fi"
          fi
        """
        try:
            fs_uuid = grub_command("grub-probe",
                                   ['--device', *devices, '--target=fs_uuid'],
                                   stderr=subprocess.DEVNULL)
        except RuntimeError:
            pass
        else:
            try:
                hints_string = grub_command("grub-probe",
                                            ['--device', *devices,
                                             '--target=hints_string'],
                                            stderr=subprocess.DEVNULL)
            except RuntimeError:
                hints_string = None

            both_fs_string = fs_uuid[0]
            if hints_string:
                hints_string_joined = ''.join(hints_string).strip()
                if hints_string_joined != '':
                    both_fs_string = f"{both_fs_string} {hints_string[0]}"

            lines.extend(
                [
                    "if [ x$feature_platform_search_hint = xy ]; then",
                    f"  search --no-floppy --fs-uuid --set=root {both_fs_string}",
                    "else",
                    f"  search --no-floppy --fs-uuid --set=root {fs_uuid[0]}",
                    "fi"
                ]
            )

        return lines

    def generate_entry(self, grub_class, grub_args, entry_type,
                       entry_indentation: int = 0) -> List[str]:

        entry = []

        if entry_type != "simple":
            title_prefix = f"{self.grub_os} BE [{self.boot_environment}] with Linux {self.version}"
            if entry_type == "recovery":
                title = f"{title_prefix} (recovery mode)"
            else:
                title = title_prefix

            # TODO: If matches default...
            r"""
            if [ x"$title" = x"$GRUB_ACTUAL_DEFAULT" ] || \
                        [ x"Previous Linux versions>$title" = x"$GRUB_ACTUAL_DEFAULT" ]; then

                replacement_title="$(echo "Advanced options for ${OS}" | \
                    sed 's,>,>>,g')>$(echo "$title" | sed 's,>,>>,g')"

                quoted="$(echo "$GRUB_ACTUAL_DEFAULT" | grub_quote)"

                # NO ACTUAL NEWLINE MID COMMAND in original
                title_correction_code="${title_correction_code}
                    if [ \"x\$default\" = '$quoted' ]; then
                        default='$(echo "$replacement_title" | grub_quote)';
                    fi;"
            fi
            """
            """
            if self.grub_default_entry == (title or f"Previous Linux versions>{title}"):
                title_prefix = f"Advanced options for {self.grub_os}".replace('>', '>>') + ">"
                replacement_title = title_prefix + title.replace('>', '>>')
                # Not quite sure why above replacing '>' is necessary, based on above shell code
            """

            entry.append(
                self.entry_line(
                    f"menuentry '{title}' {grub_class} $menuentry_id_option "
                    f"'gnulinux-{self.version}-{entry_type}-{self.boot_device_id}' {{",
                    submenu_indent=entry_indentation))
        else:
            entry.append(self.entry_line(
                f"menuentry '{self.grub_os} BE [{self.boot_environment}]' "
                f"{grub_class} $menuentry_id_option 'gnulinux-simple-{self.boot_device_id}' {{",
                submenu_indent=entry_indentation))

        # Graphics section
        entry.append(self.entry_line("load_video", submenu_indent=entry_indentation + 1))
        if not self.grub_gfxpayload_linux:
            fb_efi = self.get_from_config(r'(CONFIG_FB_EFI=y)')
            vt_hw_console_binding = self.get_from_config(r'(CONFIG_VT_HW_CONSOLE_BINDING=y)')

            if fb_efi and vt_hw_console_binding:
                entry.append(
                    self.entry_line('set gfxpayload=keep', submenu_indent=entry_indentation + 1))
        else:
            entry.append(self.entry_line(f"set gfxpayload={self.grub_gfxpayload_linux}",
                                         submenu_indent=entry_indentation + 1))

        entry.append(self.entry_line(f"insmod gzio", submenu_indent=entry_indentation + 1))

        for module in self.prepare_grub_to_access_device():
            entry.append(self.entry_line(module, submenu_indent=entry_indentation + 1))

        entry.append(self.entry_line(f"echo 'Loading Linux {self.version} ...'",
                                     submenu_indent=entry_indentation + 1))
        rel_linux = os.path.join(self.rel_dirname, self.basename)
        entry.append(
            self.entry_line(f"linux {rel_linux} root={self.linux_root_device} rw {grub_args}",
                            submenu_indent=entry_indentation + 1))

        initrd = self.get_initrd()

        if initrd:
            entry.append(self.entry_line(f"echo 'Loading initial ramdisk ...'",
                                         submenu_indent=entry_indentation + 1))
            entry.append(self.entry_line(f"initrd {' '.join(initrd)}",
                                         submenu_indent=entry_indentation + 1))

        entry.append(self.entry_line("}", entry_indentation))

        return entry

    def get_from_config(self, pattern) -> Optional[str]:
        """
        Check kernel_config for initramfs setting
        """
        config_match = None
        if self.kernel_config:
            reg = re.compile(pattern)

            with open(self.kernel_config) as f:
                config = f.read().splitlines()

            config_match = next((reg.match(lm).group(1) for lm in config if reg.match(lm)), None)

        return config_match

    def get_kernel_config(self) -> Optional[str]:
        configs = [f"{self.dirname}/config-{self.version}",
                   f"/etc/kernels/kernel-config-{self.version}"]
        return next((c for c in configs if os.path.isfile(c)), None)

    def get_initrd(self) -> list:
        initrd = []
        if self.initrd_real:
            initrd.append(os.path.join(self.rel_dirname, self.initrd_real))

        if self.initrd_early:
            initrd.extend([os.path.join(self.rel_dirname, ie) for ie in self.initrd_early])

        return initrd

    def get_initrd_early(self) -> list:
        """
        Get microcode images
        https://www.mail-archive.com/grub-devel@gnu.org/msg26775.html
        See grub-mkconfig for code
        GRUB_EARLY_INITRD_LINUX_STOCK is distro provided microcode, ie:
          intel-uc.img intel-ucode.img amd-uc.img amd-ucode.img
          early_ucode.cpio microcode.cpio"
        GRUB_EARLY_INITRD_LINUX_CUSTOM is for your custom created images
        """
        early_initrd = []
        if "GRUB_EARLY_INITRD_LINUX_STOCK" in os.environ:
            early_initrd.extend(os.environ['GRUB_EARLY_INITRD_LINUX_STOCK'].split())

        if "GRUB_EARLY_INITRD_LINUX_CUSTOM" in os.environ:
            early_initrd.extend(os.environ['GRUB_EARLY_INITRD_LINUX_CUSTOM'].split())

        return [i for i in early_initrd if os.path.isfile(os.path.join(self.dirname, i))]

    def get_initrd_real(self) -> Optional[str]:
        initrd_list = [f"initrd.img-{self.version}",
                       f"initrd-{self.version}.img",
                       f"initrd-{self.version}.gz",
                       f"initrd-{self.version}",
                       f"initramfs-{self.version}",
                       f"initramfs-{self.version}.img",
                       f"initramfs-genkernel-{self.version}",
                       f"initramfs-genkernel-{self.genkernel_arch}-{self.version}"]

        initrd_real = next(
            (i for i in initrd_list if os.path.isfile(os.path.join(self.dirname, i))), None)

        return initrd_real

    def get_boot_environment(self):
        """
        Get name of BE from kernel directory
        """
        if self.grub_boot_on_zfs:
            if not zedenv.lib.be.extra_bpool():
                if self.dirname == "/boot":
                    target = re.search(
                        r'.*/(.*)@/boot$', grub_command("grub-mkrelpath", [self.dirname])[0])
                    return target.group(1) if target else None

                target = re.search(r'zedenv-(.*)/boot/*$', self.dirname)
            else:
                target = re.search(r'zedenv-([a-zA-Z0-9_\-\.]+)@?/*$',
                                   grub_command("grub-mkrelpath", [self.dirname])[0])
        else:
            target = re.search(r'zedenv-(.*)/*$', self.dirname)

        return target.group(1) if target else None

    def get_linux_version(self):
        """
        Gets the version after kernel, if there is one
        Example:
             vmlinuz-4.16.12_1 gives 4.16.12_1
        """

        target = re.search(r'^[^0-9\-]*-(.*)$', self.basename)
        if target:
            return target.group(1)
        return ""


class Generator:

    def __init__(self):

        self.prefix = "/usr"
        self.exec_prefix = "/usr"
        self.data_root_dir = "/usr/share"

        if "pkgdatadir" in os.environ:
            self.pkgdatadir = os.environ['pkgdatadir']
        else:
            self.pkgdatadir = "/usr/share/grub"

        self.text_domain = "grub"
        self.text_domain_dir = f"{self.data_root_dir}/locale"

        self.entry_type = "advanced"

        # Update environment variables by sourcing grub defaults
        source("/etc/default/grub")

        grub_class = "--class gnu-linux --class gnu --class os"

        os.environ['ZPOOL_VDEV_NAME_PATH'] = '1'

        if "GRUB_DISTRIBUTOR" in os.environ:
            grub_distributor = os.environ['GRUB_DISTRIBUTOR']
            self.grub_os = f"{grub_distributor} GNU/Linux"
            self.grub_class = f"--class {normalize_string(grub_distributor)} {grub_class}"
        else:
            self.grub_os = "GNU/Linux"
            self.grub_class = grub_class

        # Default to true in order to maintain compatibility with older kernels.
        self.grub_disable_linux_partuuid = True
        if "GRUB_DISABLE_LINUX_PARTUUID" in os.environ:
            if os.environ['GRUB_DISABLE_LINUX_PARTUUID'] in ("false", "False", "0"):
                self.grub_disable_linux_partuuid = False

        if "GRUB_CMDLINE_LINUX" in os.environ:
            self.grub_cmdline_linux = os.environ['GRUB_CMDLINE_LINUX']
        else:
            self.grub_cmdline_linux = ""

        if "GRUB_CMDLINE_LINUX_DEFAULT" in os.environ:
            self.grub_cmdline_linux_default = os.environ['GRUB_CMDLINE_LINUX_DEFAULT']
        else:
            self.grub_cmdline_linux_default = ""

        if "GRUB_DISABLE_SUBMENU" in os.environ and os.environ['GRUB_DISABLE_SUBMENU'] == "y":
            self.grub_disable_submenu = True
        else:
            self.grub_disable_submenu = False

        self.grub_disable_recovery = None
        if "GRUB_DISABLE_RECOVERY" in os.environ:
            if os.environ['GRUB_DISABLE_RECOVERY'] == "true":
                self.grub_disable_recovery = True
            else:
                self.grub_disable_recovery = False

        self.root_dataset = pyzfscmds.system.agnostic.mountpoint_dataset("/")
        self.be_root = zedenv.lib.be.root()

        # in GRUB terms, bootfs is everything after pool
        self.bootfs = "/" + self.root_dataset.split("/", 1)[1]
        self.rpool = self.root_dataset.split("/")[0]
        self.linux_root_device = f"ZFS={self.rpool}{self.bootfs}"

        self.active_boot_environment = zedenv.lib.be.bootfs_for_pool(self.rpool)

        self.machine = platform.machine()

        self.invalid_filenames = ["readme"]  # Normalized to lowercase
        self.invalid_extensions = [".dpkg", ".rpmsave", ".rpmnew", ".pacsave", ".pacnew"]

        self.genkernel_arch = self.get_genkernel_arch()

        self.linux_entries = []

        self.grub_boot = zedenv.lib.be.get_property(self.root_dataset, 'org.zedenv.grub:boot')
        if not self.grub_boot or self.grub_boot == "-":
            self.grub_boot = "/mnt/boot"

        # Get boot device
        try:
            self.grub_boot_device = grub_command("grub-probe",
                                                 ['--target=device', self.grub_boot])
        except RuntimeError as err1:
            sys.exit(f"Failed to probe boot device.\n{err1}")

        grub_boot_on_zfs = zedenv.lib.be.get_property(
            self.root_dataset, 'org.zedenv.grub:bootonzfs')
        if grub_boot_on_zfs.lower() in ("1", "yes"):
            self.grub_boot_on_zfs = True
        else:
            try:
                fs_type = grub_command("grub-probe",
                                       ['--device', *self.grub_boot_device,
                                        '--target=fs'])
            except RuntimeError:
                fs_type = None

            if fs_type and ''.join(fs_type).strip() == "zfs":
                self.grub_boot_on_zfs = True
            else:
                self.grub_boot_on_zfs = False

        simpleentries_set = zedenv.lib.be.get_property(
            self.root_dataset, "org.zedenv.grub:simpleentries")

        self.simpleentries = True
        if simpleentries_set and simpleentries_set.lower() in ("n", "no", "0"):
            self.simpleentries = False

        boot_env_dir = "zfsenv" if self.grub_boot_on_zfs else "env"
        self.boot_env_kernels = os.path.join(self.grub_boot, boot_env_dir)

        # /usr/bin/grub-probe --target=device /
        try:
            grub_device_temp = grub_command("grub-probe", ['--target=device', "/"])
        except RuntimeError as err0:
            sys.exit(f"Failed to probe root device.\n{err0}")

        if grub_device_temp:
            self.grub_devices: list = grub_device_temp

        self.default = ""

        self.boot_list = self.get_boot_environments_boot_list()

    def file_valid(self, file_path):
        """
        Run equivalent checks to grub_file_is_not_garbage() from grub-mkconfig_lib
        Check file is valid and not one of:
        *.dpkg - debian dpkg
        *.rpmsave | *.rpmnew
        README* | */README* - documentation
        """
        if not os.path.isfile(file_path):
            return False

        file = os.path.basename(file_path)
        _, ext = os.path.splitext(file)

        if ext in self.invalid_extensions:
            return False

        if next((True for f in self.invalid_filenames if f.lower() in file.lower()), False):
            return False

        return True

    def create_entry(self, kernel_dir: str, search_regex) -> Optional[dict]:
        be_boot_dir = kernel_dir
        if self.grub_boot_on_zfs and not kernel_dir == "/boot" and not zedenv.lib.be.extra_bpool():
            be_boot_dir = os.path.join(kernel_dir, "boot")

        boot_dir = os.path.join(self.boot_env_kernels, be_boot_dir)

        boot_files = os.listdir(boot_dir)
        kernel_matches = [i for i in boot_files
                          if search_regex.match(i) and self.file_valid(os.path.join(boot_dir, i))]

        return {
            "directory": boot_dir,
            "files": boot_files,
            "kernels": kernel_matches
        }

    def get_boot_environments_boot_list(self) -> List[Optional[dict]]:
        """
        Get a list of dicts containing all BE kernels
        """

        vmlinuz = r'(vmlinuz-.*)'
        vmlinux = r'(vmlinux-.*)'
        kernel = r'(kernel-.*)'

        boot_search = f"{vmlinuz}|{kernel}"

        if re.search(r'(i[36]86)|x86_64', self.machine):
            boot_regex = re.compile(boot_search)
        else:
            boot_search = f"{boot_search}|{vmlinux}"
            boot_regex = re.compile(boot_search)

        boot_entries = [self.create_entry(e, boot_regex)
                        for e in os.listdir(self.boot_env_kernels)]

        # Do not use `/boot` if an extra ZFS boot pool is used.
        if self.grub_boot_on_zfs and os.path.exists("/boot") and not zedenv.lib.be.extra_bpool():
            boot_entries.append(self.create_entry("/boot", boot_regex))

        return boot_entries

    def get_genkernel_arch(self):

        if re.search(r'i[36]86', self.machine):
            return "x86"

        if re.search(r'mips|mips64', self.machine):
            return "mips"

        if re.search(r'mipsel|mips64el', self.machine):
            return "mipsel"

        if re.search(r'arm.*', self.machine):
            return "arm"

        return self.machine

    def generate_grub_entries(self):
        indent = 0
        is_top_level = True

        entries = []

        for i in self.boot_list:
            entry_position = 0
            kernels_sorted = sorted(i['kernels'], reverse=True,
                                    key=functools.cmp_to_key(Generator.kernel_comparator))

            for j in kernels_sorted:
                grub_entry = GrubLinuxEntry(
                    os.path.join(i['directory'], j), self.grub_os, self.be_root, self.rpool,
                    self.genkernel_arch, i, self.grub_cmdline_linux,
                    self.grub_cmdline_linux_default, self.grub_devices, self.default,
                    self.grub_boot_on_zfs, self.grub_boot_device)

                ds = os.path.join(self.be_root, grub_entry.boot_environment)
                if ds == self.active_boot_environment:
                    # To keep ordering, put matching entries ordered based on position
                    self.linux_entries.insert(entry_position, grub_entry)
                    entry_position += 1
                else:
                    self.linux_entries.append(grub_entry)

        for boot_entry in self.linux_entries:
            # First few in linux_entries are active, others in submenu
            if is_top_level and os.path.join(
                    self.be_root, boot_entry.boot_environment
            ) != self.active_boot_environment and not self.grub_disable_submenu:
                is_top_level = False
                indent = 1

                # Submenu title
                entries.append(
                    [(f"submenu 'Boot Environments ({self.grub_os})' $menuentry_id_option "
                      f"'gnulinux-advanced-be-{self.active_boot_environment}' {{")])

            if is_top_level and self.simpleentries:
                # Simple entry
                entries.append(
                    boot_entry.generate_entry(
                        self.grub_class,
                        f"{self.grub_cmdline_linux} {self.grub_cmdline_linux_default}",
                        "simple", entry_indentation=indent))

            # Advanced entry
            entries.append(
                boot_entry.generate_entry(
                    self.grub_class,
                    f"{self.grub_cmdline_linux} {self.grub_cmdline_linux_default}",
                    "advanced", entry_indentation=indent))

            # Recovery entry
            if self.grub_disable_recovery:
                entries.append(
                    boot_entry.generate_entry(
                        self.grub_class,
                        f"single {self.grub_cmdline_linux}",
                        "recovery", entry_indentation=indent))

        if not is_top_level:
            entries.append("}")

        return entries

    @staticmethod
    def kernel_comparator(kernel0: str, kernel1: str) -> int:
        """
        Rather than using key based compare, it is simpler to use a comparator in this situation.
        """

        regex = re.compile(r'-([0-9]+([\.|\-][0-9]+)*)-')

        version0 = regex.search(kernel0)
        version1 = regex.search(kernel1)

        def ext_cmp(k0: str, k1: str) -> int:
            """
            Check if the kernels and in an extension,
            if one of them does consider it less than the other
            """
            exts = ('bak', '.old')
            if k0.endswith(exts) or k1.endswith(exts):
                if k0.endswith(exts) and k1.endswith(exts):
                    return 0

                if k0.endswith(exts):
                    return -1
                return 1

            return 0

        # Compare versions
        if version0 or version1:
            if version0 and version1:
                sv0 = -1
                sv1 = -1
                try:
                    sv0 = LooseVersion(version0.group(1))
                except ValueError:
                    pass
                try:
                    sv1 = LooseVersion(version1.group(1))
                except ValueError:
                    pass

                try:
                    if sv0 < sv1:
                        return -1
                    if sv0 == sv1:
                        return ext_cmp(kernel0, kernel1)
                    return 1
                except AttributeError:
                    return ext_cmp(kernel0, kernel1)

            if version0:
                return 1
            return -1

        # No version
        return ext_cmp(kernel0, kernel1)


if __name__ == "__main__":

    ran_activate = False
    bootloader_plugin = None

    # if pyzfscmds.system.agnostic.check_valid_system():

        # Only execute if run by 'zedenv activate'
        # if not zedenv.lib.check.Pidfile()._check():
        if True:

            # Determine bootloader plugin of current installation
            # TODO

            # boot_environment_root = zedenv.lib.be.root()
            cdef libze.libze_handle* lzeh

            print(f"Hello World\n")



        #     bootloader_set = libze_be_prop_get(lzeh, plugin, "bootloader", ZE_PROP_NAMESPACE)

        #     # bootloader_set = zedenv.lib.be.get_property(
        #     #     boot_environment_root, "org.zedenv:bootloader")

        #     if bootloader_set:
        #         bootloader = bootloader_set if bootloader_set != '-' else None
        #     else:
        #         sys.exit(0)

        #     root_dataset = pyzfscmds.system.agnostic.mountpoint_dataset("/")
        #     # zpool = zedenv.lib.be.dataset_pool(root_dataset)

        #     current_be = None
        #     # try:
        #     #     current_be = pyzfscmds.utility.dataset_child_name(zedenv.lib.be.bootfs_for_pool(zpool))
        #     # except RuntimeError:
        #     #     sys.exit(0)

        #     bootloader_plugin = zedenv_grub.grub.GRUB({
        #         'boot_environment': current_be,
        #         'old_boot_environment': current_be,
        #         'bootloader': "grub",
        #         'verbose': False,
        #         'noconfirm': False,
        #         'noop': False,
        #         'boot_environment_root': boot_environment_root
        #     }, skip_update=True, skip_cleanup=True)

        #     if not bootloader_plugin.bootloader == "grub":
        #         sys.exit(0)

        #     try:
        #         bootloader_plugin.post_activate()
        #     except (RuntimeWarning, RuntimeError, AttributeError) as err:
        #         sys.exit(0)
        #     else:
        #         ran_activate = True

        # for en in Generator().generate_grub_entries():
        #     for l in en:
        #         print(l)

        # if ran_activate and bootloader_plugin:
        #     bootloader_plugin.teardown_boot_env_tree()
