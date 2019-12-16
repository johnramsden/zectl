#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <stdlib.h>

#include "libze_plugin/libze_plugin_systemdboot.h"
#include "libze/libze_util.h"

#define REGEX_BUFLEN 512

#define NUM_SYSTEMDBOOT_PROPERTY_VALUES 2
#define NUM_SYSTEMDBOOT_PROPERTIES 2
const char *systemdboot_properties[NUM_SYSTEMDBOOT_PROPERTIES][NUM_SYSTEMDBOOT_PROPERTY_VALUES] = {
        { "efi", "/efi" },
        { "boot", "/boot" }
};

libze_error
libze_plugin_systemdboot_defaults(libze_handle *lzeh, nvlist_t **default_properties) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *properties = NULL;

    properties = fnvlist_alloc();
    if (properties == NULL) {
        return libze_error_nomem(lzeh);
    }
    char namespace_buf[ZFS_MAXPROPLEN];
    if (libze_plugin_form_namespace(PLUGIN_SYSTEMDBOOT, namespace_buf)
        != LIBZE_ERROR_SUCCESS) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Exceeded max property name length.\n");
        goto err;
    }
    for (int i = 0; i < NUM_SYSTEMDBOOT_PROPERTIES; i++) {
        if (libze_default_prop_add(&properties, systemdboot_properties[i][0],
                systemdboot_properties[i][1], namespace_buf) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to add %s property to systemdboot nvlist.\n", systemdboot_properties[i][0]);
            goto err;
        }
    }

    *default_properties = properties;
    return ret;
err:
    (void) libze_list_free(properties);
    return ret;
}

static libze_error
add_default_properties(libze_handle *lzeh) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    nvlist_t *defaults_nvl = NULL;
    if ((ret = libze_plugin_systemdboot_defaults(lzeh, &defaults_nvl)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    // Add defaults
    nvpair_t *default_pair = NULL;
    for (default_pair = nvlist_next_nvpair(defaults_nvl, NULL); default_pair != NULL;
         default_pair = nvlist_next_nvpair(defaults_nvl, default_pair)) {
        const char *default_nvp_name = nvpair_name(default_pair);

        // If nvlist already in properties, don't add default
        if (nvlist_exists(lzeh->ze_props, default_nvp_name)) {
            continue;
        }

        nvlist_t *nvl = NULL;
        if (nvpair_value_nvlist(default_pair, &nvl) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed to access nvlist %s.\n", default_nvp_name);
            goto err;
        }
        nvlist_t *nvl_copy = NULL;
        if (nvlist_dup(nvl, &nvl_copy, 0) != 0) {
            ret = libze_error_nomem(lzeh);
            goto err;
        }
        if (nvlist_add_nvlist(lzeh->ze_props, default_nvp_name, nvl_copy) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                    "Failed adding default property %s.\n", default_nvp_name);
            goto err;
        }
    }

err:
    if (defaults_nvl != NULL) {
        (void) libze_list_free(defaults_nvl);
    }
    return ret;
}

/**
 * @brief
 * @param lzeh
 * @pre lzeh->ze_props is allocated
 * @return
 */
libze_error
libze_plugin_systemdboot_init(libze_handle *lzeh) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    if ((ret = add_default_properties(lzeh)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    return ret;
}

libze_error
libze_plugin_systemdboot_pre_activate(libze_handle *lzeh) {
    return 0;
}

/**
 * @brief Using boot mountpoint create a .mount unit string
 * @param lzeh               libze handle
 * @param boot_mountpoint    Mountpoint of boot partition
 * @param unit_buf           Buffer for .mount unit string
 * @return                   @p LIBZE_ERROR_UNKNOWN Boot mountpoint is not set
 *                           @p LIBZE_ERROR_MAXPATHLEN Max path length exceeded
 *                           @p LIBZE_ERROR_SUCCESS On success
 */
static libze_error
form_boot_mountpoint_unit(libze_handle *lzeh, char boot_mountpoint[LIBZE_MAX_PATH_LEN],
                          char unit_buf[LIBZE_MAX_PATH_LEN]) {

    if (strlen(boot_mountpoint) <= 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Boot mountpoint is not set.\n");
    }
    // Remove leading '/' for path name
    char tmp_boot_mountpoint_buf[ZFS_MAXPROPLEN];
    if (strlcpy(tmp_boot_mountpoint_buf, boot_mountpoint+1, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Boot mountpoint exceeds max path length.\n");
    }

    if ((strlcat(unit_buf, SYSTEMD_SYSTEM_UNIT_PATH, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(unit_buf, "/", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(unit_buf, tmp_boot_mountpoint_buf, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(unit_buf, ".mount", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Boot mountpoint unit path exceeds max path length.\n");
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
form_unit_regex(char reg_buf[REGEX_BUFLEN],
               char prefix[LIBZE_MAX_PATH_LEN],
               char suffix[ZFS_MAX_DATASET_NAME_LEN]) {
    //  "^[\\t ]*What=\\/efi\\/env\\/default[\\t ]*$"
    int ret = snprintf(reg_buf, REGEX_BUFLEN,
                       "^[\t ]*%s[\t ]*=[\t ]*%s[\t\n ]*$",
                       prefix, suffix);
    if (ret >= REGEX_BUFLEN) {
        return LIBZE_ERROR_MAXPATHLEN;
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Copy binary file into new binary file
 * @param lzeh
 * @param file Original file (rb)
 * @param new_file New file (wb)
 * @return
 */
static libze_error
do_copy_file(libze_handle *lzeh, FILE *file, FILE *new_file)
{
    assert(file != NULL);
    assert(new_file != NULL);

    while(B_TRUE) {
        int c = fgetc(file);
        if(feof(file) != 0) {
            return LIBZE_ERROR_SUCCESS;
        }
        fputc(c, new_file);

        if (ferror (new_file) != 0) {
            return LIBZE_ERROR_UNKNOWN;
        }

        fflush(new_file);
    }
}
/**
 * @brief Copy binary file into new file
 * @param lzeh
 * @param file Original filename
 * @param new_file New filename
 * @return @p LIBZE_ERROR_SUCCESS on success
 */
static libze_error
backup_file(libze_handle *lzeh, const char *filename)
{
    FILE *file = NULL;
    FILE *new_file = NULL;

    libze_error ret = LIBZE_ERROR_SUCCESS;

    char new_filename[LIBZE_MAX_PATH_LEN];

    int err = libze_util_concat(filename, ".",
            "bak", LIBZE_MAX_PATH_LEN, new_filename);
    if (err != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Backup boot mount unit exceeds max path length.\n");
    }

    file = fopen(filename, "rb");
    if (file == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to open %s", filename);
    }

    new_file = fopen(new_filename, "w+b");
    if (new_file == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to open %s", new_filename);
        goto err;
    }

    ret = do_copy_file(lzeh, file, new_file);

err:
    fclose(file);
    if (new_file != NULL) {
        fclose(new_file);
    }

    return ret;
}

static libze_error
form_suffix_string(char suffix[LIBZE_MAX_PATH_LEN],
                   char efi_mountpoint[LIBZE_MAX_PATH_LEN],
                   char be[LIBZE_MAX_PATH_LEN]) {

    if ((strlcat(suffix, efi_mountpoint, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(suffix, "/env/", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(suffix, be, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
        return LIBZE_ERROR_MAXPATHLEN;
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
file_accessible(libze_handle *lzeh, char unit_buf[LIBZE_MAX_PATH_LEN]) {
    /* Check if boot.mount is r/w */
    errno = 0;
    if (access(unit_buf, R_OK|W_OK) != 0) {
        switch (errno) {
            case EACCES:
                return libze_error_set(lzeh, LIBZE_ERROR_EPERM,
                        "Boot mountpoint unit %s in not in read/write mode.\n", unit_buf);
            case ENAMETOOLONG:
                return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                        "Boot mountpoint unit path exceeds max path length.\n");
            default:
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                        "Boot mountpoint unit %s could not be accessed.\n", unit_buf);
        }
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
copy_matched(libze_handle *lzeh,
            char unit_buf[LIBZE_MAX_PATH_LEN],
            char replace_line[LIBZE_MAX_PATH_LEN],
            regex_t *regexp,
            int tmpfd) {

    int err = 0;

    FILE *tmp = fdopen(tmpfd, "w");
    if (tmp == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to open tempfile fd %d.\n", tmpfd);
    }

    FILE* file = fopen(unit_buf, "r");
    if (file == NULL) {
        fclose(tmp);
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to open unit file fd %s.\n", unit_buf);
    }

    boolean_t matched = B_FALSE;
    char line_buf[LIBZE_MAX_PATH_LEN];

    while (fgets(line_buf, LIBZE_MAX_PATH_LEN, file)) {
        err = regexec(regexp, line_buf, 0, NULL, 0);

        if (err == 0) {
            matched = B_TRUE;
            fwrite(replace_line, 1, strlen(replace_line), tmp);
            fflush(tmp);
            continue;
        }

        fwrite(line_buf, 1, strlen(line_buf), tmp);
        fflush(tmp);
    }

    fclose(file);
    fclose(tmp);

    if (!matched) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed find regular expression for %s in boot mount unit.\n",
                replace_line);
    }

    return LIBZE_ERROR_SUCCESS;
}
/**
 * @brief Using boot mountpoint update .mount unit string
 * @param lzeh               libze handle
 * @param boot_mountpoint    Mountpoint of boot partition
 * @param be_mountpoint      BE mountpoint
 * @return                   @p LIBZE_ERROR_UNKNOWN Boot mountpoint is not set
 *                           @p LIBZE_ERROR_MAXPATHLEN Max path length exceeded
 *                           @p LIBZE_ERROR_SUCCESS On success
 */
static libze_error
update_boot_unit(libze_handle *lzeh, libze_activate_data *activate_data,
                 char boot_mountpoint[LIBZE_MAX_PATH_LEN],
                 char efi_mountpoint[LIBZE_MAX_PATH_LEN]) {
    /*
     * Update mount unit:
     * Where=/boot
     * What=/efi/env/<BE>
     */
    libze_error ret = LIBZE_ERROR_SUCCESS;
    int interr = 0;

    char active_be[ZFS_MAX_DATASET_NAME_LEN];
    if (libze_boot_env_name(lzeh->bootfs, ZFS_MAX_DATASET_NAME_LEN, active_be) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Bootfs exceeds max path length.\n");
    }

    /* Get path to boot.mount */
    char unit_buf[LIBZE_MAX_PATH_LEN];
    ret = form_boot_mountpoint_unit(lzeh, boot_mountpoint, unit_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    if ((ret = file_accessible(lzeh, unit_buf)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    /* backup unit */
    if ((ret = backup_file(lzeh, unit_buf)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    /* Setup regular expression */
    char reg_buf[REGEX_BUFLEN];
    char suffix[LIBZE_MAX_PATH_LEN];
    char replace_suffix[LIBZE_MAX_PATH_LEN];

    /* Create 'What=' suffix and replacement suffix */
    ret = form_suffix_string(suffix, efi_mountpoint, active_be);
    ret = (ret != 0) ? ret : form_suffix_string(replace_suffix, efi_mountpoint, activate_data->be_name);
    if (ret != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Unit EFI path exceeds max path length.\n");
        goto err;
    }

    /* Create a regex to find 'What' in boot.mount */
    ret = form_unit_regex(reg_buf, "What", suffix);
    if (ret != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Regex exceeds max path length.\n");
        goto err;
    }
    regex_t regexp;
    interr = regcomp(&regexp, reg_buf, 0);
    if (interr != 0) {
        char buf[LIBZE_MAX_ERROR_LEN];
        (void)regerror(interr, &regexp, buf, LIBZE_MAX_ERROR_LEN);
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Regex %s failed to compile:\n%s\n", reg_buf, buf);
        goto err;
    }

    /* Create a tempfile to manipulate before replacing original */
    char tmpfile[LIBZE_MAX_PATH_LEN];
    interr = libze_util_concat(SYSTEMD_SYSTEM_UNIT_PATH, "/",
            ".zectl-sdboot.XXXXXX", LIBZE_MAX_PATH_LEN, tmpfile);
    if (interr != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Temporary boot mount unit exceeds max path length.\n");
        goto err;
    }

    /* Create the replacement line */
    char replace_line[LIBZE_MAX_PATH_LEN];
    interr = libze_util_concat("What=", replace_suffix,
            "\n", LIBZE_MAX_PATH_LEN, replace_line);
    if (interr != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Unit EFI path exceeds max path length.\n");
        goto err;
    }

    int fd = mkstemp(tmpfile);
    if (fd == -1) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to create temporary file\n");
        goto err;
    }

    ret = copy_matched(lzeh, unit_buf, replace_line, &regexp, fd);
    if (ret != LIBZE_ERROR_SUCCESS) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed find regular expression for %s in boot mount unit.\n",
                replace_line);
        goto err;
    }

    errno = 0;
    /* Use rename for atomicity */
    interr = rename(tmpfile, unit_buf);
    if (interr != 0) {
        // TODO: Better error message from errno
        remove(tmpfile);
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                "Failed to replace boot mount unit %s.\n",
                unit_buf);
        goto err;
    }

err:
    regfree(&regexp);
    return ret;
}

/**
 * @brief Run mid-activate hook
 * @param lzeh Initialized libze handle
 * @param be_mountpoint
 * @param be_name New be
 * @return Non-zero on failure
 */
libze_error
libze_plugin_systemdboot_mid_activate(libze_handle *lzeh, libze_activate_data *activate_data) {

    libze_error ret = LIBZE_ERROR_SUCCESS;

    char boot_mountpoint[ZFS_MAXPROPLEN];
    char efi_mountpoint[ZFS_MAXPROPLEN];

    char namespace_buf[ZFS_MAXPROPLEN];
    if (libze_plugin_form_namespace(PLUGIN_SYSTEMDBOOT, namespace_buf)
        != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Exceeded max property name length.\n");
    }

    ret = libze_be_prop_get(lzeh, boot_mountpoint, "boot", namespace_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Couldn't access systemdboot:boot property.\n");
    }
    ret = libze_be_prop_get(lzeh, efi_mountpoint, "efi", namespace_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Couldn't access systemdboot:efi property.\n");
    }

    ret = update_boot_unit(lzeh, activate_data, boot_mountpoint, efi_mountpoint);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return ret;
    }
    return 0;
}

/**
 * @brief Run mid-activate hook
 * @param lzeh
 * @return
 */
libze_error
libze_plugin_systemdboot_post_activate(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]) {
    /*
     * Steps:
     *   - Modify tempdir/loader/entries/org.zectl-<be>.conf
     *       - Replace <oldbe> with <newbe>
     *   - Copy old kernels to esp/env/org.zectl-<be>/
     *   - Modify loader.conf
     */

    return 0;
}

libze_error
libze_plugin_systemdboot_post_destroy(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]) {
    puts("sd_post_destroy");

    return 0;
}