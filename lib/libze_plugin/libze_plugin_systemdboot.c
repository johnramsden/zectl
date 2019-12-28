#include "libze_plugin/libze_plugin_systemdboot.h"

#include "libze/libze_util.h"

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGEX_BUFLEN 512
#define REGEX_MAX_MATCHES 25
#define SYSTEMDBOOT_ENTRY_PREFIX "org.zectl"

#define NUM_SYSTEMDBOOT_PROPERTY_VALUES 2
#define NUM_SYSTEMDBOOT_PROPERTIES 2
const char *systemdboot_properties[NUM_SYSTEMDBOOT_PROPERTIES][NUM_SYSTEMDBOOT_PROPERTY_VALUES] = {
    {"efi", "/efi"}, {"boot", "/boot"}};

libze_error
libze_plugin_systemdboot_defaults(libze_handle *lzeh, nvlist_t **default_properties) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *properties = NULL;

    properties = fnvlist_alloc();
    if (properties == NULL) {
        return libze_error_nomem(lzeh);
    }
    char namespace_buf[ZFS_MAXPROPLEN];
    if (libze_plugin_form_namespace(PLUGIN_SYSTEMDBOOT, namespace_buf) != LIBZE_ERROR_SUCCESS) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Exceeded max property name length.\n");
        goto err;
    }
    for (int i = 0; i < NUM_SYSTEMDBOOT_PROPERTIES; i++) {
        if (libze_default_prop_add(&properties, systemdboot_properties[i][0],
                                   systemdboot_properties[i][1], namespace_buf) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                                  "Failed to add %s property to systemdboot nvlist.\n",
                                  systemdboot_properties[i][0]);
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
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to access nvlist %s.\n",
                                  default_nvp_name);
            goto err;
        }
        nvlist_t *nvl_copy = NULL;
        if (nvlist_dup(nvl, &nvl_copy, 0) != 0) {
            ret = libze_error_nomem(lzeh);
            goto err;
        }
        if (nvlist_add_nvlist(lzeh->ze_props, default_nvp_name, nvl_copy) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed adding default property %s.\n",
                                  default_nvp_name);
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
                          char be_mountpoint[LIBZE_MAX_PATH_LEN],
                          char unit_buf[LIBZE_MAX_PATH_LEN]) {

    if (strlen(boot_mountpoint) <= 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Boot mountpoint is not set.\n");
    }
    // Remove leading '/' for path name
    char tmp_boot_mountpoint_buf[ZFS_MAXPROPLEN];
    if (strlcpy(tmp_boot_mountpoint_buf, boot_mountpoint + 1, LIBZE_MAX_PATH_LEN) >=
        LIBZE_MAX_PATH_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Boot mountpoint exceeds max path length.\n");
    }

    if ((strlcat(unit_buf, be_mountpoint, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(unit_buf, SYSTEMD_SYSTEM_UNIT_PATH, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(unit_buf, "/", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(unit_buf, tmp_boot_mountpoint_buf, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) ||
        (strlcat(unit_buf, ".mount", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Boot mountpoint unit path exceeds max path length.\n");
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
form_unit_regex(char reg_buf[REGEX_BUFLEN], char prefix[LIBZE_MAX_PATH_LEN],
                char suffix[ZFS_MAX_DATASET_NAME_LEN]) {

    int ret = snprintf(reg_buf, REGEX_BUFLEN, "^[\t ]*%s[\t ]*=[\t ]*%s[\t\n ]*$", prefix, suffix);
    if (ret >= REGEX_BUFLEN) {
        return LIBZE_ERROR_MAXPATHLEN;
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
form_loader_path(const char efi_mountpoint[LIBZE_MAX_PATH_LEN],
                 const char middle_dir[LIBZE_MAX_PATH_LEN], const char be_name[LIBZE_MAX_PATH_LEN],
                 char loader_buf[LIBZE_MAX_PATH_LEN]) {

    int ret = snprintf(loader_buf, LIBZE_MAX_PATH_LEN, "%s/%s/%s-%s", efi_mountpoint, middle_dir,
                       SYSTEMDBOOT_ENTRY_PREFIX, be_name);

    if (ret >= REGEX_BUFLEN) {
        return LIBZE_ERROR_MAXPATHLEN;
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
form_loader_config(const char efi_mountpoint[LIBZE_MAX_PATH_LEN],
                   const char be_name[LIBZE_MAX_PATH_LEN], char loader_buf[LIBZE_MAX_PATH_LEN]) {

    libze_error ret = form_loader_path(efi_mountpoint, "loader/entries", be_name, loader_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    if (strlcat(loader_buf, ".conf", LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) {
        return LIBZE_ERROR_MAXPATHLEN;
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
file_accessible(libze_handle *lzeh, const char unit_buf[LIBZE_MAX_PATH_LEN]) {
    /* Check if boot.mount is r/w */
    errno = 0;
    if (access(unit_buf, R_OK | W_OK) != 0) {
        switch (errno) {
            case EACCES:
                return libze_error_set(lzeh, LIBZE_ERROR_EPERM,
                                       "Boot mountpoint unit %s in not in read/write mode.\n",
                                       unit_buf);
            case ENAMETOOLONG:
                return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                       "Boot mountpoint unit path exceeds max path length.\n");
            default:
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                                       "Boot mountpoint unit %s could not be accessed.\n",
                                       unit_buf);
        }
    }

    return LIBZE_ERROR_SUCCESS;
}

static libze_error
copy_matched(libze_handle *lzeh, char unit_buf[LIBZE_MAX_PATH_LEN],
             char replace_line[LIBZE_MAX_PATH_LEN], regex_t *regexp, int tmpfd) {

    FILE *tmp = fdopen(tmpfd, "w");
    if (tmp == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to open tempfile fd %d.\n",
                               tmpfd);
    }

    FILE *file = fopen(unit_buf, "r");
    if (file == NULL) {
        fclose(tmp);
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to open unit file fd %s.\n",
                               unit_buf);
    }

    boolean_t matched = B_FALSE;
    char line_buf[LIBZE_MAX_PATH_LEN];

    while (fgets(line_buf, LIBZE_MAX_PATH_LEN, file)) {

        if (regexec(regexp, line_buf, 0, NULL, 0) == 0) {
            /* Regex matched */
            matched = B_TRUE;
            fwrite(replace_line, 1, strlen(replace_line), tmp);
            continue;
        }

        fwrite(line_buf, 1, strlen(line_buf), tmp);
    }

    fflush(tmp);

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
    if (libze_boot_env_name(lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN, active_be) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Bootfs exceeds max path length.\n");
    }

    /* Get path to boot.mount */
    char unit_buf[LIBZE_MAX_PATH_LEN];
    ret = form_boot_mountpoint_unit(lzeh, boot_mountpoint, activate_data->be_mountpoint, unit_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    if ((ret = file_accessible(lzeh, unit_buf)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    char new_filename[LIBZE_MAX_PATH_LEN];

    int err = libze_util_concat(unit_buf, ".", "bak", LIBZE_MAX_PATH_LEN, new_filename);
    if (err != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Backup boot mount unit exceeds max path length.\n");
    }

    /* backup unit */
    if ((ret = libze_util_copy_file(unit_buf, new_filename)) != 0) {
        return ret;
    }

    /* Setup regular expression */
    char reg_buf[REGEX_BUFLEN];
    char suffix[LIBZE_MAX_PATH_LEN];
    char replace_suffix[LIBZE_MAX_PATH_LEN];

    /* Create 'What=' suffix and replacement suffix */
    interr = libze_util_concat(efi_mountpoint, "/env/", active_be, LIBZE_MAX_PATH_LEN, suffix);
    if (interr != 0) {
        interr = libze_util_concat(efi_mountpoint, "/env/", activate_data->be_name,
                                   LIBZE_MAX_PATH_LEN, replace_suffix);
    }
    if (interr != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                              "Unit EFI path exceeds max path length.\n");
        goto err;
    }

    /* Create a regex to find 'What' in boot.mount */
    ret = form_unit_regex(reg_buf, "What", suffix);
    if (ret != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Regex exceeds max path length.\n");
        goto err;
    }
    regex_t regexp;
    interr = regcomp(&regexp, reg_buf, 0);
    if (interr != 0) {
        char buf[LIBZE_MAX_ERROR_LEN];
        (void) regerror(interr, &regexp, buf, LIBZE_MAX_ERROR_LEN);
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Regex %s failed to compile:\n%s\n",
                              reg_buf, buf);
        goto err;
    }

    /* Create a tempfile to manipulate before replacing original */
    char tmpfile_front[LIBZE_MAX_PATH_LEN];
    char tmpfile[LIBZE_MAX_PATH_LEN];
    interr = libze_util_concat(SYSTEMD_SYSTEM_UNIT_PATH, "/", "/.zectl-sdboot.XXXXXX",
                               LIBZE_MAX_PATH_LEN, tmpfile_front);

    if (interr == 0) {
        interr = libze_util_concat(activate_data->be_mountpoint, "/", tmpfile_front,
                                   LIBZE_MAX_PATH_LEN, tmpfile);
    }

    if (interr != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                              "Temporary boot mount unit exceeds max path length.\n");
        goto err;
    }

    /* Create the replacement line */
    char replace_line[LIBZE_MAX_PATH_LEN];
    interr = libze_util_concat("What=", replace_suffix, "\n", LIBZE_MAX_PATH_LEN, replace_line);
    if (interr != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                              "Unit EFI path exceeds max path length.\n");
        goto err;
    }

    int fd = mkstemp(tmpfile);
    if (fd == -1) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to create temporary file\n");
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
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to replace boot mount unit %s.\n",
                              unit_buf);
        goto err;
    }

err:
    regfree(&regexp);
    return ret;
}

/**
 * @brief TODO
 * @param lzeh
 * @param filename
 * @param filename_new
 * @param be_name
 * @param regexp
 * @return
 */
static libze_error
replace_matched(libze_handle *lzeh, const char filename[LIBZE_MAX_PATH_LEN],
                const char filename_new[LIBZE_MAX_PATH_LEN], const char be_name[LIBZE_MAX_PATH_LEN],
                const char be_name_active[LIBZE_MAX_PATH_LEN]) {

    libze_error ret = LIBZE_ERROR_SUCCESS;

    FILE *file_new = fopen(filename_new, "w+");
    if (file_new == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to open %s.\n", file_new);
    }

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fclose(file_new);
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to open %s.\n", filename);
    }

    char line_buf[LIBZE_MAX_PATH_LEN];
    char replace_line_buf[LIBZE_MAX_PATH_LEN];
    while (fgets(line_buf, LIBZE_MAX_PATH_LEN, file)) {
        /* TODO: Rewrite to be 'selective' and match each line based on regex */
        ret = libze_util_replace_string(be_name_active, be_name, LIBZE_MAX_PATH_LEN, line_buf,
                                        LIBZE_MAX_PATH_LEN, replace_line_buf);
        if (ret != LIBZE_ERROR_SUCCESS) {
            goto err;
        }
        fwrite(replace_line_buf, 1, strlen(replace_line_buf), file_new);
    }

    fflush(file_new);

err:
    fclose(file);
    fclose(file_new);

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief TODO
 * @param lzeh
 * @param activate_data
 * @param active_be
 * @param filename
 * @return
 */
static libze_error
replace_be_name(libze_handle *lzeh, const char be_name[ZFS_MAX_DATASET_NAME_LEN],
                const char active_be[ZFS_MAX_DATASET_NAME_LEN],
                const char filename[LIBZE_MAX_PATH_LEN],
                const char new_filename[LIBZE_MAX_PATH_LEN]) {

    libze_error ret = LIBZE_ERROR_SUCCESS;
    int interr = 0;

    if ((ret = file_accessible(lzeh, filename)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    /* Setup regular expression */
    char reg_buf[REGEX_BUFLEN];
    if (strlcpy(reg_buf, active_be, ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Regex exceeds max path length.\n");
    }

    regex_t regexp;
    interr = regcomp(&regexp, reg_buf, 0);
    if (interr != 0) {
        char buf[LIBZE_MAX_ERROR_LEN];
        (void) regerror(interr, &regexp, buf, LIBZE_MAX_ERROR_LEN);
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Regex %s failed to compile:\n%s\n",
                              reg_buf, buf);
        goto err;
    }

    // TODO:
    ret = replace_matched(lzeh, filename, new_filename, be_name, active_be);
    if (ret != LIBZE_ERROR_SUCCESS) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to replace %s in %s.\n", be_name,
                              filename);
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
    if (libze_plugin_form_namespace(PLUGIN_SYSTEMDBOOT, namespace_buf) !=
        LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Exceeded max property name length.\n");
    }

    ret = libze_be_prop_get(lzeh, boot_mountpoint, "boot", namespace_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                               "Couldn't access systemdboot:boot property.\n");
    }
    ret = libze_be_prop_get(lzeh, efi_mountpoint, "efi", namespace_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                               "Couldn't access systemdboot:efi property.\n");
    }

    ret = update_boot_unit(lzeh, activate_data, boot_mountpoint, efi_mountpoint);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    return ret;
}

/**
 * @brief Run mid-activate hook
 * @param lzeh Initialized libze handle
 * @return Non @p LIBZE_ERROR_SUCCESS on failure
 */
libze_error
libze_plugin_systemdboot_post_activate(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]) {
    /*
     * Steps:
     *   - Copy <esp>/loader/entries/<prefix>-<oldbe>.conf -> <prefix>-<be>.conf
     *   - Modify esp/loader/entries/org.zectl-<be>.conf
     *       - Replace <oldbe> with <newbe>
     *   - Copy old kernels to esp/env/org.zectl-<be>/
     *   - Modify loader.conf
     */

    libze_error ret = LIBZE_ERROR_SUCCESS;
    int iret = 0;

    char active_be[ZFS_MAX_DATASET_NAME_LEN];
    if (libze_boot_env_name(lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN, active_be) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Bootfs exceeds max path length.\n");
    }

    char boot_mountpoint[ZFS_MAXPROPLEN];
    char efi_mountpoint[ZFS_MAXPROPLEN];

    char namespace_buf[ZFS_MAXPROPLEN];
    if (libze_plugin_form_namespace(PLUGIN_SYSTEMDBOOT, namespace_buf) !=
        LIBZE_PLUGIN_MANAGER_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Exceeded max property name length.\n");
    }

    ret = libze_be_prop_get(lzeh, boot_mountpoint, "boot", namespace_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                               "Couldn't access systemdboot:boot property.\n");
    }
    ret = libze_be_prop_get(lzeh, efi_mountpoint, "efi", namespace_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                               "Couldn't access systemdboot:efi property.\n");
    }

    /* Copy <esp>/loader/entries/<prefix>-<oldbe>.conf -> <prefix>-<be>.conf */
    char loader_buf[LIBZE_MAX_PATH_LEN];
    char new_loader_buf[LIBZE_MAX_PATH_LEN];

    ret = form_loader_config(efi_mountpoint, active_be, loader_buf);
    if (ret == LIBZE_ERROR_SUCCESS) {
        ret = form_loader_config(efi_mountpoint, be_name, new_loader_buf);
    }

    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "BE loader path exceeds max path length.\n");
    }

    ret = replace_be_name(lzeh, be_name, active_be, loader_buf, new_loader_buf);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Failed to replace '%s' in '%s' with '%s'.\n", active_be,
                               new_loader_buf, be_name);
    }

    ret = form_loader_path(efi_mountpoint, "env", active_be, loader_buf);
    if (ret == LIBZE_ERROR_SUCCESS) {
        ret = form_loader_path(efi_mountpoint, "env", be_name, new_loader_buf);
    }
    if (ret != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    iret = libze_util_copydir(loader_buf, new_loader_buf);
    if (iret != 0) {
        // TODO: Check error, return better message
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to copy %s -> %s.\n", loader_buf,
                               new_loader_buf);
    }

    return ret;
}

libze_error
libze_plugin_systemdboot_post_destroy(libze_handle *lzeh, const char be_name[LIBZE_MAX_PATH_LEN]) {
    puts("sd_post_destroy");

    return 0;
}