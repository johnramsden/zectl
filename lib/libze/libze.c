// Required for spl stat.h
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "libze/libze.h"
#include "libze/libze_plugin_manager.h"
#include "libze/libze_util.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <dirent.h>
#include <libzfs_core.h>
#include <sys/nvpair.h>

#include <sys/stat.h>
#include <sys/types.h>

// Unsigned long long is 64 bits or more
#define ULL_SIZE 128

const char *ZE_PROP_NAMESPACE = "org.zectl";

static int libze_clone_cb(zfs_handle_t *zhdl, void *data);

/**
 * @brief TODO comment
 * @param property
 * @param property_prefix
 * @param property_suffix
 * @return
 */
static libze_error parse_property(const char property[static 1], char property_prefix[ZFS_MAXPROPLEN],
                                  char property_suffix[ZFS_MAXPROPLEN]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    /* Prefix, just ZE_NAMESPACE if no colon in property
     * Otherwise ZE_NAMESPACE + part before colon */
    char prop_prefix[ZFS_MAXPROPLEN] = "";

    // Full ZFS property
    char temp_prop[ZFS_MAXPROPLEN] = "";
    if (strlcpy(temp_prop, property, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
        fprintf(stderr, "property '%s' is too long.\n", property);
        return LIBZE_ERROR_MAXPATHLEN;
    }

    if (strlcpy(property_suffix, property, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
        return LIBZE_ERROR_MAXPATHLEN;
    }

    char *suffix_value = NULL;
    if ((suffix_value = strchr(temp_prop, ':')) != NULL) {
        // Cut at ':'
        *suffix_value = '\0'; // Prefix in temp_prop
        suffix_value++;
        ret = libze_util_concat(ZE_PROP_NAMESPACE, ".", temp_prop, ZFS_MAXPROPLEN, prop_prefix);
        if (ret != LIBZE_ERROR_SUCCESS) {
            return ret;
        }
    }
    else {
        if (strlcpy(prop_prefix, ZE_PROP_NAMESPACE, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
            return LIBZE_ERROR_MAXPATHLEN;
        }
        suffix_value = temp_prop;
    }

    if (strlcpy(property_suffix, suffix_value, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
        return LIBZE_ERROR_MAXPATHLEN;
    }
    if (strlcpy(property_prefix, prop_prefix, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
        return LIBZE_ERROR_MAXPATHLEN;
    }
    return ret;
}

/**
 * @brief Given a property with an optional prefix for a bootloader,
 *        form a ZFS property nvlist
 * @param[in,out] properties Pre-allocated nvlist to add a property to
 * @param property Individual property to add to the list
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if property is too long,
 *         @p LIBZE_ERROR_UNKNOWN if no '=' in property
 *
 * @pre @p properties is allocated and non NULL
 */
libze_error libze_add_set_property(nvlist_t *properties, const char *property) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    // Property value, part after '='
    char *value = NULL;

    /* Prefix, just ZE_NAMESPACE if no colon in property
     * Otherwise ZE_NAMESPACE + part before colon */
    char prop_prefix[ZFS_MAXPROPLEN] = "";
    // Full resulting ZFS property
    char prop_full_name[ZFS_MAXPROPLEN]   = "";
    char prop_after_colon[ZFS_MAXPROPLEN] = "";

    if (parse_property(property, prop_prefix, prop_after_colon) != LIBZE_ERROR_SUCCESS) {
        fprintf(stderr, "property '%s' is too long.\n", property);
        return LIBZE_ERROR_MAXPATHLEN;
    }

    if ((value = strchr(prop_after_colon, '=')) == NULL) {
        fprintf(stderr, "missing '=' for property=value argument.\n");
        return LIBZE_ERROR_UNKNOWN;
    }

    // Cut at '='
    *value = '\0';
    value++;

    ret = libze_util_concat(prop_prefix, ":", prop_after_colon, ZFS_MAXPROPLEN, prop_full_name);
    if (ret != LIBZE_ERROR_SUCCESS) {
        fprintf(stderr, "property '%s' is too long.\n", property);
        return ret;
    }

    if (nvlist_exists(properties, prop_full_name)) {
        fprintf(stderr, "property '%s' specified multiple times.\n", property);
        return LIBZE_ERROR_UNKNOWN;
    }

    if (nvlist_add_string(properties, prop_full_name, value) != 0) {
        return LIBZE_ERROR_NOMEM;
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief TODO
 * @param lzeh
 * @param properties
 * @param property
 * @return
 */
libze_error libze_add_get_property(libze_handle *lzeh, nvlist_t **properties, const char *property) {
    nvpair_t *  pair = NULL;
    libze_error ret  = LIBZE_ERROR_SUCCESS;

    /* Prefix, just ZE_NAMESPACE if no colon in property
     * Otherwise ZE_NAMESPACE + part before colon */
    char prop_prefix[ZFS_MAXPROPLEN] = "";

    // Full resulting ZFS property
    char prop_full_name[ZFS_MAXPROPLEN]   = "";
    char prop_after_colon[ZFS_MAXPROPLEN] = "";

    if (parse_property(property, prop_prefix, prop_after_colon) != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Name of the property (%s) exceeds max property name (%d).\n", property, ZFS_MAXPROPLEN);
    }

    ret = libze_util_concat(prop_prefix, ":", prop_after_colon, ZFS_MAXPROPLEN, prop_full_name);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, ret, "Name of the property (%s:%s) exceeds max property name (%d).\n", prop_prefix,
                               prop_after_colon, ZFS_MAXPROPLEN);
    }

    for (pair = nvlist_next_nvpair(lzeh->ze_props, NULL); pair != NULL;
         pair = nvlist_next_nvpair(lzeh->ze_props, pair)) {
        if (strcmp(nvpair_name(pair), prop_full_name) == 0) {
            if (nvlist_add_nvpair(*properties, pair) != 0) {
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to add property (%s) to the nvlist.\n",
                                       property);
            }
        }
    }

    // Add empty property
    if (nvlist_empty(*properties)) {
        nvlist_t *prop_nvl = fnvlist_alloc();
        if (prop_nvl == NULL) {
            return libze_error_set(lzeh, LIBZE_ERROR_NOMEM, "Failed to allocate nvlist.\n");
        }
        if ((nvlist_add_string(prop_nvl, "value", "-") != 0) || (nvlist_add_string(prop_nvl, "source", "-") != 0) ||
            (nvlist_add_nvlist(*properties, prop_full_name, prop_nvl) != 0)) {
            return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to add property (%s) to the nvlist.\n", property);
        }
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Add a default property
 * @param[out] prop_out Output property list
 * @param name Name of property without nameapace
 * @param value Value of default property
 * @param namespace Property namespace without colon
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_NOMEM if nvlist can't be duplicated,
 *         @p LIBZE_ERROR_UNKNOWN otherwise
 *
 * Properties in form:
 * @verbatim
   org.zectl:bootloader:
       value: 'systemdboot'
       source: 'zroot/ROOT'
   @endverbatim
 */
libze_error libze_default_prop_add(nvlist_t **prop_out, const char *name, const char *value, const char *namespace) {
    nvlist_t *default_prop = fnvlist_alloc();
    if (default_prop == NULL) {
        return LIBZE_ERROR_NOMEM;
    }

    if (nvlist_add_string(default_prop, "value", value) != 0) {
        goto err;
    }

    char name_buf[ZFS_MAXPROPLEN] = "";
    if (libze_util_concat(namespace, ":", name, ZFS_MAXPROPLEN, name_buf)) {
        goto err;
    }

    if (nvlist_add_nvlist(*prop_out, name_buf, default_prop) != 0) {
        goto err;
    }

    return LIBZE_ERROR_SUCCESS;

err:
    nvlist_free(default_prop);
    return LIBZE_ERROR_UNKNOWN;
}

/**
 * @brief Set default properties
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] default_prop Properties of boot environment
 * @param[in] namespace TODO
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_NOMEM if nvlist can't be duplicated,
 *         @p LIBZE_ERROR_UNKNOWN otherwise
 *
 * @pre @p default_prop != NULL
 * @pre @p namespace != NULL
 */
libze_error libze_default_props_set(libze_handle *lzeh, nvlist_t *default_prop, const char *namespace) {
    nvpair_t *  pair = NULL;
    libze_error ret  = LIBZE_ERROR_SUCCESS;

    for (pair = nvlist_next_nvpair(default_prop, NULL); pair != NULL; pair = nvlist_next_nvpair(default_prop, pair)) {

        char *nvp_name            = nvpair_name(pair);
        char  buf[ZFS_MAXPROPLEN] = "";

        if (libze_util_cut(nvp_name, ZFS_MAXPROPLEN, buf, ':') != 0) {
            return LIBZE_ERROR_UNKNOWN;
        }

        if (strcmp(buf, namespace) != 0) {
            continue;
        }

        // Check if property set
        boolean_t ze_prop_unset = B_TRUE;
        nvpair_t *ze_pair       = NULL;
        for (ze_pair = nvlist_next_nvpair(lzeh->ze_props, NULL); ze_pair != NULL;
             ze_pair = nvlist_next_nvpair(lzeh->ze_props, ze_pair)) {
            char *ze_nvp_name = nvpair_name(ze_pair);
            if (strcmp(ze_nvp_name, nvp_name) == 0) {
                ze_prop_unset = B_FALSE;
                break;
            }
        }

        // Property unset, set default
        if (ze_prop_unset) {
            nvlist_t *ze_default_prop_nvl = NULL;
            nvlist_t *ze_prop_nvl         = NULL;
            if (nvpair_value_nvlist(pair, &ze_prop_nvl) != 0) {
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to get nvpair_value.\n");
            }
            if (nvlist_dup(ze_prop_nvl, &ze_default_prop_nvl, 0) != 0) {
                return libze_error_set(lzeh, LIBZE_ERROR_NOMEM, "Failed to duplicate nvlist.\n");
            }
            if (nvlist_add_nvlist(lzeh->ze_props, nvp_name, ze_default_prop_nvl) != 0) {
                return libze_error_set(lzeh, LIBZE_ERROR_NOMEM, "Failed to add default property (%s) to nvlist.\n",
                                       nvp_name);
            }
        }
    }

    return ret;
}

/**
 * @brief Filter out boot environment properties based on name of program namespace
 * @param[in] unfiltered_nvl @p nvlist_t to filter based on namespace
 * @param[out] result_nvl Filtered @p nvlist_t continuing only properties matching namespace
 * @param namespace Prefix property to filter based on
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_UNKNOWN on failure
 *
 * @pre @p unfiltered_nvl != NULL
 * @pre @p namespace != NULL
 */
static libze_error libze_filter_be_props(nvlist_t *unfiltered_nvl, nvlist_t **result_nvl,
                                         const char namespace[static 1]) {
    nvpair_t *  pair = NULL;
    libze_error ret  = LIBZE_ERROR_SUCCESS;

    for (pair = nvlist_next_nvpair(unfiltered_nvl, NULL); pair != NULL;
         pair = nvlist_next_nvpair(unfiltered_nvl, pair)) {
        char *nvp_name            = nvpair_name(pair);
        char  buf[ZFS_MAXPROPLEN] = "";

        // Make sure namespace ends
        if ((strlen(nvp_name) + 1) < (strlen(namespace) + 1)) {
            char after_namespace = nvp_name[strlen(namespace) + 1];
            if ((after_namespace != '.') && (after_namespace != ':')) {
                continue;
            }
        }

        if (libze_util_cut(nvp_name, ZFS_MAXPROPLEN, buf, ':') != 0) {
            return LIBZE_ERROR_UNKNOWN;
        }

        if (strncmp(buf, namespace, strlen(namespace)) == 0) {
            nvlist_add_nvpair(*result_nvl, pair);
        }
    }

    return ret;
}

/**
 * @brief Get a ZFS property value from @a lzeh->ze_props
 *
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[out] result_prop Property of boot environment
 * @param[in] prop ZFS property looking for
 * @param[in] namespace ZFS property prefix
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if property exceeds max allowed length,
 *         @p LIBZE_ERROR_UNKNOWN otherwise
 *
 * @pre @p lzeh != NULL
 * @pre @p property != NULL
 * @pre @p namespace != NULL
 */
libze_error libze_be_prop_get(libze_handle *lzeh, char *result_prop, const char *property, const char *namespace) {
    nvlist_t *lookup_prop = NULL;

    char prop_buf[ZFS_MAXPROPLEN] = "";
    if (libze_util_concat(namespace, ":", property, ZFS_MAXPROPLEN, prop_buf)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Name of property (%s:%s) exceeds max property length (%d).\n", namespace, property,
                               ZFS_MAXPROPLEN);
    }

    if (nvlist_lookup_nvlist(lzeh->ze_props, prop_buf, &lookup_prop) != 0) {
        (void)strlcpy(result_prop, "", ZFS_MAXPROPLEN);
        return LIBZE_ERROR_SUCCESS;
    }

    nvpair_t *prop = NULL;
    // Should always have a value if set correctly
    if (nvlist_lookup_nvpair(lookup_prop, "value", &prop) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Property nvlist set incorrectly.\n");
    }

    char *string_prop = NULL;
    if (nvpair_value_string(prop, &string_prop) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Property nvlist value is wrong type. Should be a string.\n");
    }

    if (strlcpy(result_prop, string_prop, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Value (%s) of property (%s) exceeds max property length (%d).\n", string_prop, prop_buf,
                               ZFS_MAXPROPLEN);
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Get all the ZFS properties which have been set with the @p namespace prefix
 *        and return them in @a result.
 *
 *        Properties in form:
 * @verbatim
   org.zectl:bootloader:
       value: 'systemdboot'
       source: 'zroot/ROOT'
   @endverbatim
 *
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[out] result Properties of boot environment
 * @param[in] namespace ZFS property prefix
 * @return @p LIBZE_ERROR_SUCCESS on success,
*          @p LIBZE_ERROR_NOMEM TODO,
 *         @p LIBZE_ERROR_UNKNOWN if property can't be read,
 *         @p LIBZE_ERROR_ZFS_OPEN if handle can't be opened
 *
 * @pre @p lzeh != NULL
 * @pre @p namespace != NULL
 */
libze_error libze_be_props_get(libze_handle *lzeh, nvlist_t **result, const char *namespace) {
    nvlist_t *  user_props          = NULL;
    nvlist_t *  filtered_user_props = NULL;
    libze_error ret                 = LIBZE_ERROR_SUCCESS;

    zfs_handle_t *zhp = zfs_open(lzeh->lzh, lzeh->env_root, ZFS_TYPE_FILESYSTEM);
    if (zhp == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed opening handle to %s.\n", lzeh->env_root);
    }

    if ((user_props = zfs_get_user_props(zhp)) == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to retrieve user properties for %s.\n",
                              zfs_get_name(zhp));
        goto err;
    }

    if ((filtered_user_props = fnvlist_alloc()) == NULL) {
        ret = libze_error_nomem(lzeh);
        goto err;
    }

    if ((ret = libze_filter_be_props(user_props, &filtered_user_props, namespace)) != LIBZE_ERROR_SUCCESS) {
        goto err;
    }

    zfs_close(zhp);
    *result = filtered_user_props;
    return ret;
err:
    zfs_close(zhp);
    fnvlist_free(user_props);
    fnvlist_free(filtered_user_props);
    return ret;
}

/**
 * @brief Set an error message to @p lzeh->libze_error_message and return the error type given in @p lze_err.
 * @param[in,out] initialized lzeh libze handle
 * @param[in] lze_err Error value returned
 * @param[in] lze_fmt Format specifier used by @p lzeh
 * @param ... Variable args used to format the error message saved in @p lzeh
 * @return @p lze_err
 *
 * @pre @p lzeh != NULL
 * @pre if @p lze_fmt == NULL, @p ... should have zero arguments.
 * @pre Length of formatted string < @p LIBZE_MAX_ERROR_LEN
 */
libze_error libze_error_set(libze_handle *lzeh, libze_error lze_err, const char *lze_fmt, ...) {

    if (lzeh == NULL) {
        return lze_err;
    }

    lzeh->libze_error = lze_err;

    if (lze_fmt == NULL) {
        strlcpy(lzeh->libze_error_message, "", LIBZE_MAX_ERROR_LEN);
        return lze_err;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
    va_list argptr;
    va_start(argptr, lze_fmt);
    int length = vsnprintf(lzeh->libze_error_message, LIBZE_MAX_ERROR_LEN, lze_fmt, argptr);
    va_end(argptr);
#pragma clang diagnostic pop

    // Not worth failing on assert? Will just truncate
    // assert(length < LIBZE_MAX_ERROR_LEN);

    return lze_err;
}

/**
 * @brief Convenience function to set no memory error message
 * @param[in,out] initialized lzeh libze handle
 * @return @p LIBZE_ERROR_NOMEM
 *
 * @pre @p lzeh != NULL
 */
libze_error libze_error_nomem(libze_handle *lzeh) {
    if (lzeh == NULL) {
        return LIBZE_ERROR_NOMEM;
    }
    return libze_error_set(lzeh, LIBZE_ERROR_NOMEM, "Failed to allocate memory.\n");
}

/**
 * @brief Convenience function to set no memory error message
 * @param[in,out] initialized lzeh libze handle
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_NOMEM TODO
 *
 * @pre @p lzeh != NULL
 */
libze_error libze_error_clear(libze_handle *lzeh) {
    if (lzeh == NULL) {
        return LIBZE_ERROR_SUCCESS;
    }
    return libze_error_set(lzeh, LIBZE_ERROR_SUCCESS, NULL);
}

/**
 * @brief Check if a plugin is set, if it is initialize it.
 * @param lzeh Initialized @p libze_handle
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_PLUGIN if plugin can't be opened or initialized,
 *         @p LIBZE_ERROR_PLUGIN_EEXIST if plugin doesn't exist
 *
 * @post if @a lzeh->ze_props contains an existing org.zectl:bootloader,
 *            the bootloader plugin is initialized.
 */
libze_error libze_bootloader_set(libze_handle *lzeh) {
    char        plugin[ZFS_MAXPROPLEN] = "";
    libze_error ret                    = LIBZE_ERROR_SUCCESS;

    ret = libze_be_prop_get(lzeh, plugin, "bootloader", ZE_PROP_NAMESPACE);
    if (ret != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    // No plugin set
    if (strlen(plugin) == 0) {
        return LIBZE_ERROR_SUCCESS;
    }

    void *                     p_handle = NULL;
    libze_plugin_manager_error p_ret    = libze_plugin_open(plugin, &p_handle);

    if (p_ret == LIBZE_PLUGIN_MANAGER_ERROR_EEXIST) {
        return libze_error_set(lzeh, LIBZE_ERROR_PLUGIN_EEXIST, "Plugin (%s) doesn't exist.\n", plugin);
    }

    if (p_handle == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_PLUGIN, "Failed to open plugin (%s).\n", plugin);
    }
    else {
        if (libze_plugin_export(p_handle, &lzeh->lz_funcs) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN, "Failed to open export table for plugin (%s).\n", plugin);
        }
        else {
            if (lzeh->lz_funcs->plugin_init(lzeh) != 0) {
                ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN, "Failed to initialize plugin (%s).\n", plugin);
            }
        }
    }

    return ret;
}

/********************************************************
 ************** libze initialize / destroy **************
 ********************************************************/

/**
 * @brief Rep invariant check for initialized @p libze_handle
 * @param lzeh @p libze_handle to check
 * @return Non-zero if rep check fails, else zero
 */
static int libze_handle_rep_check_init(libze_handle *lzeh) {
    int  ret                                = 0;
    char check_failure[LIBZE_MAX_ERROR_LEN] = "ERROR - libze_handle RI.\n";

    if (lzeh == NULL) {
        fprintf(stderr, "ERROR - libze_handle RI: libze_handle isn't initialized.\n");
        return -1;
    }

    if ((lzeh->lzh == NULL) || (lzeh->pool_zhdl == NULL) || ((lzeh->ze_props == NULL))) {
        (void)strlcat(check_failure, "A handle isn't initialized.\n", LIBZE_MAX_ERROR_LEN);
        ret++;
    }
    if (!((strlen(lzeh->env_root) >= 1) && (strlen(lzeh->env_running_path) >= 3) && (strlen(lzeh->env_running) >= 1) &&
          (strlen(lzeh->env_pool) >= 1))) {
        (void)strlcat(check_failure, "Lengths of strings incorrect.\n", LIBZE_MAX_ERROR_LEN);
        ret++;
    }
    if ((lzeh->libze_error != LIBZE_ERROR_SUCCESS) || (strlen(lzeh->libze_error_message) != 0)) {
        (void)strlcat(check_failure, "Errors not cleared.\n", LIBZE_MAX_ERROR_LEN);
        ret++;
    }

    if (ret != 0) {
        (void)libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, check_failure);
    }

    return ret;
}

/**
 * @brief Check if a boot pool is set, if it is, open the pool and set @p lzeh->bootpool
 * @param lzeh Initialized @p libze_handle
 * @return @p LIBZE_ERROR_SUCCESS if no boot pool set, or if boot pool dataset set successfully,
 *         @p LIBZE_ERROR_EEXIST if root path for boot datasets on the bootpool does not exist,
 *         @p LIBZE_ERROR_LIBZFS if property can't be retrieved,
 *         @p LIBZE_ERROR_MAXPATHLEN if requested boot pool is too long,
 *         @p LIBZE_ERROR_MAXPATHLEN if @p bootpool is too long,
 *         @p LIBZE_ERROR_ZFS_OPEN if @p lzeh->bootpool.lzbph can't be opened,
 *         @p LIBZE_ERROR_UNKNOWN if env name from dataset can't be retrieved
 *
 * @post @p lzeh->bootpol.pool_zhdl if success: is opened and != NULL
 *                                  else:       NULL
 */
libze_error libze_boot_pool_set(libze_handle *lzeh) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    // NOTE constant string
    char bootpool_root_path[ZFS_MAX_DATASET_NAME_LEN] = "";
    if ((ret = libze_be_prop_get(lzeh, bootpool_root_path, "bootpool_root", ZE_PROP_NAMESPACE)) !=
        LIBZE_ERROR_SUCCESS) {
        return ret;
    }
    char boot_prefix[ZFS_MAX_DATASET_NAME_LEN] = "";
    if ((ret = libze_be_prop_get(lzeh, boot_prefix, "bootpool_prefix", ZE_PROP_NAMESPACE)) != LIBZE_ERROR_SUCCESS) {
        return ret;
    }

    if (strlen(bootpool_root_path) == 0) {
        // No parameters are set, assume there is no separate boot pool
        lzeh->bootpool.pool_zhdl = NULL;
        (void)strlcpy(lzeh->bootpool.zpool_name, "", ZFS_MAX_DATASET_NAME_LEN);
        (void)strlcpy(lzeh->bootpool.root_path, "", ZFS_MAX_DATASET_NAME_LEN);
        (void)strlcpy(lzeh->bootpool.root_path_full, "", ZFS_MAX_DATASET_NAME_LEN);
        (void)strlcpy(lzeh->bootpool.dataset_prefix, "", ZFS_MAX_DATASET_NAME_LEN);
        (void)strlcpy(lzeh->bootpool.env_activated_path, "", ZFS_MAX_DATASET_NAME_LEN);
        (void)strlcpy(lzeh->bootpool.env_running_path, "", ZFS_MAX_DATASET_NAME_LEN);
        return ret;
    }

    char bootpool_name[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (!libze_get_zpool_name_from_dataset(bootpool_root_path, ZFS_MAX_DATASET_NAME_LEN, bootpool_name)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Can't determine ZFS pool name of specified root path (%s).\n",
                               bootpool_root_path);
    }

    zpool_handle_t *pool_zhdl;
    if ((pool_zhdl = zpool_open(lzeh->lzh, bootpool_name)) == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Can't open ZFS bootpool (%s).\n", bootpool_name);
    }

    // NOTE constant string
    if (!zfs_dataset_exists(lzeh->lzh, bootpool_root_path, ZFS_TYPE_DATASET)) { // NOLINT(hicpp-signed-bitwise)
        return libze_error_set(
            lzeh, LIBZE_ERROR_EEXIST,
            "Root path (bootpool:root) which holds all boot datasets on the bootpool (%s) does not exist.\n",
            bootpool_root_path);
    }

    zfs_handle_t *zph = zfs_open(lzeh->lzh, bootpool_root_path, ZFS_TYPE_FILESYSTEM);
    if (zph == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed to open root dataset from bootpool (%s).\n",
                               bootpool_root_path);
                               goto err;
    }

    char prop_buf[ZFS_MAXPROPLEN] = "";
    if (zfs_prop_get(zph, ZFS_PROP_MOUNTPOINT, prop_buf, sizeof(prop_buf), NULL, NULL, 0, 1) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_LIBZFS, "Failed to get ZFS mountpoint property for %s.\n",
                               bootpool_root_path);
    }

    char bootpool_path_temp[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(bootpool_root_path, "/", boot_prefix, ZFS_MAX_DATASET_NAME_LEN, bootpool_path_temp) >=
        ZFS_MAX_DATASET_NAME_LEN) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Requested path to bootpool (%s/%s) exceeds max length (%d).\n", bootpool_root_path,
                               boot_prefix, ZFS_MAX_DATASET_NAME_LEN);
    }

    char bootpool_root_path_full[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (strlen(boot_prefix) > 0) {
        if (libze_util_concat(bootpool_path_temp, "-", "", ZFS_MAX_DATASET_NAME_LEN, bootpool_root_path_full) >=
            ZFS_MAX_DATASET_NAME_LEN) {
            ret = libze_error_set(
                lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Requested subpath for activated boot environment on bootpool (%s-) exceeds max length (%d).\n",
                bootpool_path_temp, ZFS_MAX_DATASET_NAME_LEN);
        }
    }
    else {
        strlcpy(bootpool_root_path_full, bootpool_path_temp, ZFS_MAX_DATASET_NAME_LEN);
    }

    char bootpool_ds_activated[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(bootpool_root_path_full, "", lzeh->env_activated, ZFS_MAX_DATASET_NAME_LEN,
                          bootpool_ds_activated) >= ZFS_MAX_DATASET_NAME_LEN) {
        ret = libze_error_set(
            lzeh, LIBZE_ERROR_MAXPATHLEN,
            "Path to the dataset for the activated boot environment on bootpool (%s%s) exceeds max length (%d).\n",
            bootpool_root_path, boot_prefix, ZFS_MAX_DATASET_NAME_LEN);
    }
    if (!zfs_dataset_exists(lzeh->lzh, bootpool_ds_activated, ZFS_TYPE_DATASET)) { // NOLINT(hicpp-signed-bitwise)
        ret = libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                               "Dataset for the activated boot environment on bootpool (%s) does not exist.\n",
                               bootpool_ds_activated);
    }
    zfs_handle_t *zph_activated = zfs_open(lzeh->lzh, bootpool_ds_activated, ZFS_TYPE_FILESYSTEM);
    zfs_handle_t *zph_running = NULL;
    if (zph_activated == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                               "Failed to open the boot dataset of the activated boot environment (%s).\n",
                               bootpool_ds_activated);
    }
    if (zfs_prop_get(zph_activated, ZFS_PROP_MOUNTPOINT, prop_buf, sizeof(prop_buf), NULL, NULL, 0, 1) != 0) {
        ret = libze_error_set(
            lzeh, LIBZE_ERROR_LIBZFS,
            "Failed to get ZFS mountpoint property for dataset of the activated boot environment (%s).\n",
            bootpool_ds_activated);
    }
    if (strcmp(prop_buf, "none") == 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_INVALID_CONFIG,
                               "A boot dataset for the activated environment exists (%s) but is not mountable.\n",
                               bootpool_ds_activated);
    }

    char bootpool_ds_running[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_is_root_be(lzeh, lzeh->env_activated_path)) {
        (void)strlcpy(bootpool_ds_running, bootpool_ds_activated, ZFS_MAX_DATASET_NAME_LEN);
    }
    else {
        if (libze_util_concat(bootpool_root_path_full, "", lzeh->env_running, ZFS_MAX_DATASET_NAME_LEN,
                              bootpool_ds_running) >= ZFS_MAX_DATASET_NAME_LEN) {
            ret = libze_error_set(
                lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Requested path for the running boot environment on bootpool (%s%s) exceeds max length (%d).\n",
                bootpool_ds_running, lzeh->env_running, ZFS_MAX_DATASET_NAME_LEN);
        }
        if (!zfs_dataset_exists(lzeh->lzh, bootpool_ds_running, ZFS_TYPE_DATASET)) { // NOLINT(hicpp-signed-bitwise)
            ret = libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                                   "Dataset for the running boot environment on bootpool (%s) does not exist.\n",
                                   bootpool_ds_running);
        }
        zph_running = zfs_open(lzeh->lzh, bootpool_ds_running, ZFS_TYPE_FILESYSTEM);
        if (zph_running == NULL) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                                   "Failed to open the boot dataset of the running boot environment (%s).\n",
                                   bootpool_ds_running);
        }
        if (zfs_prop_get(zph_running, ZFS_PROP_MOUNTPOINT, prop_buf, sizeof(prop_buf), NULL, NULL, 0, 1) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_LIBZFS,
                                   "Failed to get the ZFS mountpoint property of the running boot environment (%s).\n",
                                   bootpool_ds_running);
        }
        if (strcmp(prop_buf, "none") == 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_INVALID_CONFIG,
                                   "A boot dataset for the running environment exists (%s) but is not mountable.\n",
                                   bootpool_ds_running);
        }
    }

    // No errors has been found, write the handle and paths to boot pool back
    lzeh->bootpool.pool_zhdl = pool_zhdl;
    strlcpy(lzeh->bootpool.zpool_name, bootpool_name, ZFS_MAX_DATASET_NAME_LEN);
    strlcpy(lzeh->bootpool.root_path, bootpool_root_path, ZFS_MAX_DATASET_NAME_LEN);
    strlcpy(lzeh->bootpool.root_path_full, bootpool_root_path_full, ZFS_MAX_DATASET_NAME_LEN);
    strlcpy(lzeh->bootpool.dataset_prefix, boot_prefix, ZFS_MAX_DATASET_NAME_LEN);
    strlcpy(lzeh->bootpool.env_activated_path, bootpool_ds_activated, ZFS_MAX_DATASET_NAME_LEN);
    strlcpy(lzeh->bootpool.env_running_path, bootpool_ds_running, ZFS_MAX_DATASET_NAME_LEN);

err:
    if (zph != NULL) {
        zfs_close(zph);
    }
    if (zph_activated != NULL) {
        zfs_close(zph_activated);
    }
    if (zph_running != NULL) {
        zfs_close(zph_running);
    }
    return ret;

}

/**
 * @brief Initialize libze handle.
 * @return Initialized handle, or NULL if unsuccessful.
 */
libze_handle *libze_init(void) {
    libze_handle *lzeh   = NULL;
    char *        slashp = NULL;
    char *        zpool  = NULL;

    if ((lzeh = calloc(1, sizeof(libze_handle))) == NULL) {
        return NULL;
    }
    if ((lzeh->lzh = libzfs_init()) == NULL) {
        goto err;
    }
    if (libze_get_root_dataset(lzeh) != 0) {
        goto err;
    }
    if (libze_util_cut(lzeh->env_running_path, ZFS_MAX_DATASET_NAME_LEN, lzeh->env_root, '/') != 0) {
        goto err;
    }
    if ((slashp = strchr(lzeh->env_root, '/')) == NULL) {
        goto err;
    }

    size_t pool_length = (slashp) - (lzeh->env_root);
    zpool              = calloc(1, pool_length + 1);
    if (zpool == NULL) {
        goto err;
    }

    // Get pool portion of dataset
    if (strncpy(zpool, lzeh->env_root, pool_length) == NULL) {
        goto err;
    }
    zpool[pool_length] = '\0';

    if (strlcpy(lzeh->env_pool, zpool, ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
        goto err;
    }

    if ((lzeh->pool_zhdl = zpool_open(lzeh->lzh, lzeh->env_pool)) == NULL) {
        goto err;
    }

    // Determine activated boot environment
    if (zpool_get_prop(lzeh->pool_zhdl, ZPOOL_PROP_BOOTFS, lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN, NULL,
                       B_TRUE) != 0) {
        goto err;
    }
    if (libze_boot_env_name(lzeh, lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN, lzeh->env_activated)) {
        strlcpy(lzeh->env_activated_path, "", sizeof(lzeh->env_activated_path));
        strlcpy(lzeh->env_activated, "", sizeof(lzeh->env_activated));
        goto err;
    }

    if (libze_be_props_get(lzeh, &lzeh->ze_props, ZE_PROP_NAMESPACE) != 0) {
        goto err;
    }

    // Clear bootloader
    lzeh->lz_funcs = NULL;

    // Clear bootpool, initialization is done later
    lzeh->bootpool.pool_zhdl = NULL;
    (void)strlcpy(lzeh->bootpool.zpool_name, "", ZFS_MAX_DATASET_NAME_LEN);
    (void)strlcpy(lzeh->bootpool.root_path, "", ZFS_MAX_DATASET_NAME_LEN);
    (void)strlcpy(lzeh->bootpool.root_path_full, "", ZFS_MAX_DATASET_NAME_LEN);
    (void)strlcpy(lzeh->bootpool.dataset_prefix, "", ZFS_MAX_DATASET_NAME_LEN);
    (void)strlcpy(lzeh->bootpool.env_activated_path, "", ZFS_MAX_DATASET_NAME_LEN);
    (void)strlcpy(lzeh->bootpool.env_running_path, "", ZFS_MAX_DATASET_NAME_LEN);

    (void)libze_error_clear(lzeh);

    assert(libze_handle_rep_check_init(lzeh) == 0);

    free(zpool);
    return lzeh;

err:
    libze_fini(lzeh);
    if (zpool != NULL) {
        free(zpool);
    }
    return NULL;
}

/**
 * @brief @p libze_handle cleanup.
 * @param lzeh @p libze_handle to de-allocate and close resources on.
 *
 * @post @p lzeh->lzh is closed
 * @post @p lzeh->pool_zhdl is closed
 * @post @p lzeh->bootpool.pool_zhdl is closed
 * @post @p lzeh->ze_props is free'd
 * @post @p lzeh is free'd
 */
void libze_fini(libze_handle *lzeh) {
    if (lzeh == NULL) {
        return;
    }

    if (lzeh->lzh != NULL) {
        libzfs_fini(lzeh->lzh);
        lzeh->lzh = NULL;
    }

    if (lzeh->pool_zhdl != NULL) {
        zpool_close(lzeh->pool_zhdl);
        lzeh->pool_zhdl = NULL;
    }

    if (lzeh->ze_props != NULL) {
        fnvlist_free(lzeh->ze_props);
        lzeh->ze_props = NULL;
    }

    // Cleanup bootloader
    if (lzeh->bootpool.pool_zhdl != NULL) {
        zpool_close(lzeh->bootpool.pool_zhdl);
        lzeh->bootpool.pool_zhdl = NULL;
    }

    free(lzeh);
}

/**************************************
 ************** activate **************
 **************************************/

typedef struct libze_activate_cbdata {
    libze_handle *lzeh;
} libze_activate_cbdata;

/**
 * @brief Callback run for ever sub dataset of @p zhdl
 * @param[in] zhdl Initialed @p zfs_handle_t to recurse based on.
 * @param[in,out] data @p libze_activate_cbdata to activate based on.
 * @return Non zero on failure.
 *
 * @pre zhdl != NULL
 * @pre data != NULL
 */
static int libze_activate_cb(zfs_handle_t *zhdl, void *data) {
    char                   buf[ZFS_MAXPROPLEN] = "";
    libze_activate_cbdata *cbd                 = data;

    if (zfs_prop_set(zhdl, "canmount", "noauto") != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed setting canmount=noauto for %s.\n",
                               zfs_get_name(zhdl));
    }

    // Check if clone
    if (zfs_prop_get(zhdl, ZFS_PROP_ORIGIN, buf, sizeof(buf), NULL, NULL, 0, 1) != 0) {
        // Not a clone, continue
        return 0;
    }

    if (zfs_promote(zhdl) != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed promoting %s.\n", zfs_get_name(zhdl));
    }

    if (zfs_iter_filesystems(zhdl, libze_activate_cb, cbd) != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Function run mid-activate, execute plugin if it exists.
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in] options Options to set properties based on
 * @param[in] be_zh Initialized @p zfs_handle_t to run mid activate on
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if path of mount directory is too long,
 *         @p LIBZE_ERROR_MKDIR if temporary directory cannot be created,
 *         @p LIBZE_ERROR_MOUNT if mount or unmount failed,
 *         @p LIBZE_ERROR_UNKNOWN, or @p LIBZE_ERROR_PLUGIN on failure
 *
 * @pre lzeh != NULL
 * @pre be_zh != NULL
 * @pre options != NULL
 * @post if be_zh != root dataset, be_zh unmounted on exit
 */
static libze_error mid_activate(libze_handle *lzeh, libze_activate_options *options, zfs_handle_t *be_zh) {
    libze_error ret         = LIBZE_ERROR_SUCCESS;
    char *      tmp_dirname = "/";
    const char *ds_name     = zfs_get_name(be_zh);
    boolean_t   is_root     = libze_is_root_be(lzeh, ds_name);

    nvlist_t *props = fnvlist_alloc();
    if (props == NULL) {
        return libze_error_nomem(lzeh);
    }
    nvlist_add_string(props, "canmount", "noauto");

    if (!is_root) {
        nvlist_add_string(props, "mountpoint", "/");

        // Not currently mounted
        char tmpdir_template[LIBZE_MAX_PATH_LEN] = "";
        if (libze_util_concat("/tmp/ze.", options->be_name, ".XXXXXX", LIBZE_MAX_PATH_LEN, tmpdir_template) >=
            LIBZE_MAX_PATH_LEN) {
            return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                   "Name of directory template (/tmp/ze.%s.XXXXXX) exceeds max length (%d).\n",
                                   options->be_name, LIBZE_MAX_PATH_LEN);
        }

        // Create tmp mountpoint
        tmp_dirname = mkdtemp(tmpdir_template);
        if (tmp_dirname == NULL) {
            return libze_error_set(lzeh, LIBZE_ERROR_MKDIR, "Could not create tmp directory (%s).\n", tmpdir_template);
        }

        // AFTER here always goto err to cleanup

        if (zfs_prop_set(be_zh, "mountpoint", tmp_dirname) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to set mountpoint=%s for %s.\n", tmpdir_template,
                                  ds_name);
            goto err;
        }

        if (zfs_mount(be_zh, NULL, 0) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_MOUNT, "Failed to mount %s to %s.\n", ds_name, tmpdir_template);
            goto err;
        }
    }

    libze_activate_data activate_data = {.be_name = options->be_name, .be_mountpoint = tmp_dirname};

    // mid_activate
    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_mid_activate(lzeh, &activate_data) != 0)) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_PLUGIN, "Failed to run mid-activate hook.\n");
        goto err;
    }

err:
    if (!is_root && zfs_is_mounted(be_zh, NULL)) {
        // Retain existing error if occurred
        if ((zfs_unmount(be_zh, NULL, 0) != 0) && (ret != LIBZE_ERROR_SUCCESS)) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_MOUNT, "Failed to unmount %s.\n", ds_name);
        }
        else {
            rmdir(tmp_dirname);
            if ((zfs_prop_set_list(be_zh, props) != 0) && (ret != LIBZE_ERROR_SUCCESS)) {
                ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to unset mountpoint for %s.\n", ds_name);
            }
        }
    }
    nvlist_free(props);
    return ret;
}

/**
 * @brief Based on @p options, activate a boot environment
 * @param[in] lzeh Initialized lzeh libze handle
 * @param options The options based on which the boot environment is activated
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if name of requested dataset is too long,
 *         @p LIBZE_ERROR_EEXIST if requested dataset does not exist,
 *         @p LIBZE_ERROR_PLUGIN if a plugin interaction failed,
 *         @p LIBZE_ERROR_ZFS_OPEN if the requested dataset can't be opened,
 *         @p LIBZE_ERROR_UNKNOWN otherwise
 */
libze_error libze_activate(libze_handle *lzeh, libze_activate_options *options) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char env_request_path[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->env_root, "/", options->be_name, ZFS_MAX_DATASET_NAME_LEN, env_request_path)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Requested boot environment (%s/%s) exceeds max length (%d).\n", lzeh->env_root,
                               options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, env_request_path, ZFS_TYPE_DATASET)) { // NOLINT(hicpp-signed-bitwise)
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Dataset for requested boot environment (%s) doesn't exist.\n",
                               env_request_path);
    }

    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_pre_activate(lzeh) != 0)) {
        return LIBZE_ERROR_PLUGIN;
    }

    zfs_handle_t *be_zh = zfs_open(lzeh->lzh, env_request_path, ZFS_TYPE_DATASET); // NOLINT(hicpp-signed-bitwise)
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed opening dataset (%s).\n", env_request_path);
    }

    if ((ret = mid_activate(lzeh, options, be_zh)) != LIBZE_ERROR_SUCCESS) {
        goto err;
    }

    if (zpool_set_prop(lzeh->pool_zhdl, "bootfs", env_request_path) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "An error occurred while setting bootfs=%s.\n",
                              env_request_path);
        goto err;
    }

    // Set for top level dataset
    libze_activate_cbdata cbd = {lzeh};
    if (libze_activate_cb(be_zh, &cbd) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    // Set for all child datasets and promote
    if (zfs_iter_filesystems(be_zh, libze_clone_cb, &cbd) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_post_activate(lzeh, options->be_name) != 0)) {
        goto err;
    }

err:
    zfs_close(be_zh);
    return ret;
}

/**********************************************
 ************** clone and create **************
 **********************************************/

typedef struct libze_clone_prop_cbdata {
    libze_handle *lzeh;
    zfs_handle_t *zhp;
    nvlist_t *    props;
} libze_clone_prop_cbdata_t;

/**
 * @brief Callback to run on each property
 * @param prop Current property
 * @param[in,out] data Initialized @p libze_clone_prop_cbdata_t to save props into.
 * @return @p ZPROP_CONT to continue iterating.
 */
static int clone_prop_cb(int prop, void *data) {
    libze_clone_prop_cbdata_t *pcbd = data;

    zprop_source_t src;
    char           propbuf[ZFS_MAXPROPLEN] = "";
    char           statbuf[ZFS_MAXPROPLEN] = "";
    const char *   prop_name;

    // Skip if readonly or canmount
    if (zfs_prop_readonly(prop)) {
        return ZPROP_CONT;
    }

    prop_name = zfs_prop_to_name(prop);

    // Always set canmount=noauto
    if (prop == ZFS_PROP_CANMOUNT) {
        fnvlist_add_string(pcbd->props, prop_name, "noauto");
        return ZPROP_CONT;
    }

    if (zfs_prop_get(pcbd->zhp, prop, propbuf, sizeof(propbuf), &src, statbuf, sizeof(statbuf), B_FALSE) != 0) {
        return ZPROP_CONT;
    }

    // Skip if not LOCAL and not RECEIVED
    if ((src != ZPROP_SRC_LOCAL) && (src != ZPROP_SRC_RECEIVED)) {
        return ZPROP_CONT;
    }

    if (nvlist_add_string(pcbd->props, prop_name, propbuf) != 0) {
        return ZPROP_CONT;
    }

    return ZPROP_CONT;
}

/**
 * @brief Callback run recursively on a dataset
 * @param[in] zhdl Initialized @p zfs_handle_t representing current dataset
 * @param[in,out] data Initialized @p libze_clone_cbdata to save properties into.
 * @return Non-zero on success.
 */
static int libze_clone_cb(zfs_handle_t *zhdl, void *data) {
    libze_clone_cbdata *cbd   = data;
    int                 ret   = LIBZE_ERROR_SUCCESS;
    nvlist_t *          props = NULL;

    if (((props = fnvlist_alloc()) == NULL)) {
        return libze_error_nomem(cbd->lzeh);
    }

    libze_clone_prop_cbdata_t cb_data = {.lzeh = cbd->lzeh, .props = props, .zhp = zhdl};

    // Iterate over all props
    if (zprop_iter(clone_prop_cb, &cb_data, B_FALSE, B_FALSE, ZFS_TYPE_FILESYSTEM) == ZPROP_INVAL) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                              "Failed to iterate over properties for top level dataset.\n");
        goto err;
    }

    fnvlist_add_nvlist(*cbd->outnvl, zfs_get_name(zhdl), props);

    if (cbd->recursive) {
        if (zfs_iter_filesystems(zhdl, libze_clone_cb, cbd) != 0) {
            ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to iterate over child datasets.\n");
            goto err;
        }
    }

    return ret;
err:
    fnvlist_free(props);
    return ret;
}

/**
 * @brief Create a recursive clone from a snapshot given the dataset and snapshot separately.
 *        The snapshot suffix should be the same for all nested datasets.
 * @param lzeh Initialized libze handle
 * @param source_root Top level dataset for clone
 * @param source_snap_suffix Snapshot name
 * @param be Name for new boot environment
 * @param recursive Do recursive clone
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN, @p LIBZE_ERROR_NOMEM, @p LIBZE_ERROR_ZFS_OPEN or
 *         @p LIBZE_ERROR_UNKNOWN on failure
 *
 * @pre lzeh != NULL
 * @pre source_root != NULL
 * @pre source_snap_suffix != NULL
 * @pre be != NULL
 */
libze_error libze_clone(libze_handle *lzeh, char source_root[static 1], char source_snap_suffix[static 1],
                        char be[static 1], boolean_t recursive) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    nvlist_t *cdata = NULL;
    if ((cdata = fnvlist_alloc()) == NULL) {
        return libze_error_nomem(lzeh);
    }

    // Get be root handle
    zfs_handle_t *zroot_hdl = NULL;
    if ((zroot_hdl = zfs_open(lzeh->lzh, source_root, ZFS_TYPE_FILESYSTEM)) == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Error opening top level dataset for clone (%s).\n",
                              source_root);
        goto err;
    }

    libze_clone_cbdata cbd = {.outnvl = &cdata, .lzeh = lzeh, .recursive = recursive};

    // Get properties for bootfs and under bootfs
    if (libze_clone_cb(zroot_hdl, &cbd) != 0) {
        // libze_clone_cb sets error message.
        ret = LIBZE_ERROR_UNKNOWN;
        goto err;
    }

    if (recursive) {
        if (zfs_iter_filesystems(zroot_hdl, libze_clone_cb, &cbd) != 0) {
            // libze_clone_cb sets error message.
            ret = LIBZE_ERROR_UNKNOWN;
            goto err;
        }
    }

    nvpair_t *pair = NULL;
    for (pair = nvlist_next_nvpair(cdata, NULL); pair != NULL; pair = nvlist_next_nvpair(cdata, pair)) {
        nvlist_t *ds_props = NULL;
        nvpair_value_nvlist(pair, &ds_props);

        // Recursive clone
        char *ds_name                                = nvpair_name(pair);
        char  ds_snap_buf[ZFS_MAX_DATASET_NAME_LEN]  = "";
        char  be_child_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
        char  ds_child_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
        if (libze_util_suffix_after_string(source_root, ds_name, ZFS_MAX_DATASET_NAME_LEN, be_child_buf) == 0) {
            if (strlen(be_child_buf) > 0) {
                if (libze_util_concat(be, be_child_buf, "/", ZFS_MAX_DATASET_NAME_LEN, ds_child_buf)) {
                    ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                          "Requested child clone (%s/%s) exceeds max length (%d).\n", be, be_child_buf,
                                          ZFS_MAX_DATASET_NAME_LEN);
                    goto err;
                }
            }
            else {
                // Child empty
                if (strlcpy(ds_child_buf, be, ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
                    ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                          "Requested child clone (%s) exceeds max length (%d).\n", ds_child_buf,
                                          ZFS_MAX_DATASET_NAME_LEN);
                    goto err;
                }
            }
        }

        if (libze_util_concat(ds_name, "@", source_snap_suffix, ZFS_MAX_DATASET_NAME_LEN, ds_snap_buf)) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Requested snapshot (%s@%s) exceeds max length (%d).\n",
                                  ds_name, source_snap_suffix, ZFS_MAX_DATASET_NAME_LEN);
            goto err;
        }

        zfs_handle_t *snap_handle = NULL;
        if ((snap_handle = zfs_open(lzeh->lzh, ds_snap_buf, ZFS_TYPE_SNAPSHOT)) == NULL) {
            ret =
                libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Requested snapshot (%s) can't be opened.\n", ds_snap_buf);
            goto err;
        }
        if (zfs_clone(snap_handle, ds_child_buf, ds_props) != 0) {
            zfs_close(snap_handle);
            ret =
                libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Requested snapshot (%s) can't be cloned.\n", ds_child_buf);
            goto err;
        }

        zfs_close(snap_handle);
    }

err:
    libze_list_free(cdata);
    zfs_close(zroot_hdl);
    return ret;
}

/**
 * @brief Create a snapshot timestamp
 * @param buflen Length of buffer @p buf
 * @param buf Buffer for snapshot timestamp
 * @return non-zero if buffer length exceeded
 */
static int gen_snap_suffix(size_t buflen, char buf[buflen]) {
    time_t now_time;
    time(&now_time);
    return strftime(buf, buflen, "%F-%T", localtime(&now_time)) != 0;
}

typedef struct create_data {
    char      snap_suffix[ZFS_MAX_DATASET_NAME_LEN];
    char      source_dataset[ZFS_MAX_DATASET_NAME_LEN];
    boolean_t is_snap;
} create_data;

/**
 * @brief Cut snapshot and dataset from full snapshot
 * @param[in] source_snap Source full snapshot
 * @param[out] dest_dataset Destination dataset
 * @param[out] dest_snapshot Destination snapshot suffix
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if @p ZFS_MAX_DATASET_NAME_LEN exceeded,
 *         @p LIBZE_ERROR_UNKNOWN if no suffix found
 */
static libze_error get_snap_and_dataset(const char source_snap[ZFS_MAX_DATASET_NAME_LEN],
                                        char       dest_dataset[ZFS_MAX_DATASET_NAME_LEN],
                                        char       dest_snapshot[ZFS_MAX_DATASET_NAME_LEN]) {
    // Get snapshot dataset
    if (libze_util_cut(source_snap, ZFS_MAX_DATASET_NAME_LEN, dest_dataset, '@') != 0) {
        return LIBZE_ERROR_MAXPATHLEN;
    }
    // Get snap suffix
    if (libze_util_suffix_after_string(dest_dataset, source_snap, ZFS_MAX_DATASET_NAME_LEN, dest_snapshot) != 0) {
        return LIBZE_ERROR_UNKNOWN;
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Prepare boot pool data
 * @param lzeh Initialized libze handle
 * @param source_snap Source snapshot
 * @param source_be_name Source boot environment name
 * @param dest_dataset Destination full dataset
 * @param dest_snapshot_suffix Destination snapshot suffix
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST TODO,
 *         @p LIBZE_ERROR_MAXPATHLEN if @p ZFS_MAX_DATASET_NAME_LEN exceeded,
 *         @p LIBZE_ERROR_UNKNOWN if no snapshot suffix found
 */
static libze_error prepare_existing_boot_pool_data(libze_handle *lzeh, const char source_snap[ZFS_MAX_DATASET_NAME_LEN],
                                                   const char source_be_name[ZFS_MAX_DATASET_NAME_LEN],
                                                   char       dest_dataset[ZFS_MAX_DATASET_NAME_LEN],
                                                   char       dest_snapshot_suffix[ZFS_MAX_DATASET_NAME_LEN]) {
    char t_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    switch (get_snap_and_dataset(source_snap, t_buff, dest_snapshot_suffix)) {
        case LIBZE_ERROR_MAXPATHLEN:
            return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Source snapshot (%s) is too long (%d).\n",
                                   source_snap, ZFS_MAX_DATASET_NAME_LEN);
        case LIBZE_ERROR_UNKNOWN:
            return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                                   "Source snapshot (%s) doesn't contain snapshot suffix or is too long.\n",
                                   source_snap);
        default:
            break;
    }

    if (libze_util_concat(lzeh->bootpool.root_path_full, "", source_be_name, ZFS_MAX_DATASET_NAME_LEN, dest_dataset) >=
        ZFS_MAX_DATASET_NAME_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Path to source dataset on bootpool (%s%s) for snapshot is too long (%d).\n",
                               lzeh->bootpool.root_path_full, source_be_name, ZFS_MAX_DATASET_NAME_LEN);
    }
    if (!zfs_dataset_exists(lzeh->lzh, dest_dataset, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Source boot pool dataset (%s) doesn't exist.\n",
                               dest_dataset);
    }
    char dataset_snap_name[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(dest_dataset, "@", source_snap, ZFS_MAX_DATASET_NAME_LEN, dataset_snap_name) >=
        ZFS_MAX_DATASET_NAME_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Snapshot name for source boot pool dataset (%s@%s) is too long (%d).\n", dest_dataset,
                               source_snap, ZFS_MAX_DATASET_NAME_LEN);
    }
    if (!zfs_dataset_exists(lzeh->lzh, dataset_snap_name, ZFS_TYPE_SNAPSHOT)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Source boot pool snapshot (%s) doesn't exist.\n",
                               dataset_snap_name);
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Populate @p cdata with what is necessary for create
 * @param[in] lzeh Initialized libze handle
 * @param[in] options Create options
 * @param[out] cdata Data to populate
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if source snapshot doesn't exist,
 *         @p LIBZE_ERROR_MAXPATHLEN if source snapshot is too long,
 *         @p LIBZE_ERROR_UNKNOWN if no snap suffix found
 *
 * @pre (options->existing == B_TRUE) && (strlen(cdata->source_dataset) > 0)
 */
static libze_error prepare_create_from_existing(libze_handle *lzeh, const char be_source[ZFS_MAX_DATASET_NAME_LEN],
                                                create_data *cdata) {
    // Is a snapshot
    if (strchr(be_source, '@') != NULL) {
        cdata->is_snap = B_TRUE;
        if (!zfs_dataset_exists(lzeh->lzh, be_source, ZFS_TYPE_SNAPSHOT)) {
            return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Source snapshot (%s) doesn't exist.\n", be_source);
        }

        switch (get_snap_and_dataset(be_source, cdata->source_dataset, cdata->snap_suffix)) {
            case LIBZE_ERROR_MAXPATHLEN:
                return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Source snapshot name (%s) is too long (%d).\n",
                                       be_source, ZFS_MAX_DATASET_NAME_LEN);
                break;
            case LIBZE_ERROR_UNKNOWN:
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                                       "Source snapshot (%s) doesn't contain snapshot suffix or is too long.\n",
                                       be_source);
            default:
                return LIBZE_ERROR_SUCCESS;
        }
    }

    cdata->is_snap = B_FALSE;
    // Regular dataset
    if (!zfs_dataset_exists(lzeh->lzh, be_source, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Source dataset (%s) doesn't exist.\n", be_source);
    }
    if (strlcpy(cdata->source_dataset, be_source, ZFS_MAX_DATASET_NAME_LEN) >= ZFS_MAX_DATASET_NAME_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Source dataset (%s) exceeds max dataset length (%d).\n",
                               be_source, ZFS_MAX_DATASET_NAME_LEN);
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Create boot environment
 * @param lzeh Initialized libze handle
 * @param options Options for boot environment
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if a name of a dataset or snapshot is too long,
 *         @p LIBZE_ERROR_UNKNOWN if a snapshot
 */
libze_error libze_create(libze_handle *lzeh, libze_create_options *options) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    create_data cdata;
    create_data boot_pool_cdata;

    // Populate cdata from existing dataset or snap
    if (options->existing) {
        ret = prepare_create_from_existing(lzeh, options->be_source, &cdata);
        if (ret != LIBZE_ERROR_SUCCESS) {
            return ret;
        }

        if (lzeh->bootpool.pool_zhdl != NULL) {
            /* Setup source as (boot pool root)/(existing name)
             * Since from existing, use snap suffix from existing */
            // TODO

            char dest_ds_buf[ZFS_MAX_DATASET_NAME_LEN]   = "";
            char dest_snap_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
            // TODO will not work
            ret =
                prepare_existing_boot_pool_data(lzeh, options->be_source, options->be_name, dest_ds_buf, dest_snap_buf);
            if (ret != LIBZE_ERROR_SUCCESS) {
                return ret;
            }
            ret = prepare_create_from_existing(lzeh, dest_ds_buf, &boot_pool_cdata);
            if (ret != LIBZE_ERROR_SUCCESS) {
                return ret;
            }
        }
    }
    else { // Populate cdata from bootfs
        cdata.is_snap = B_FALSE;
        if (strlcpy(cdata.source_dataset, lzeh->env_activated_path, ZFS_MAX_DATASET_NAME_LEN) >=
            ZFS_MAX_DATASET_NAME_LEN) {
            return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                   "Source dataset name (%s) can't be copied because it's exceeds max length (%d).\n",
                                   cdata.snap_suffix, ZFS_MAX_DATASET_NAME_LEN);
        }
        char snap_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
        (void)gen_snap_suffix(ZFS_MAX_DATASET_NAME_LEN, cdata.snap_suffix);
        if (libze_util_concat(cdata.source_dataset, "@", cdata.snap_suffix, ZFS_MAX_DATASET_NAME_LEN, snap_buf) >=
            ZFS_MAX_DATASET_NAME_LEN) {
            return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                   "Source dataset snapshot (%s@%s) exceeds max dataset length (%d).\n",
                                   cdata.source_dataset, cdata.snap_suffix, ZFS_MAX_DATASET_NAME_LEN);
        }

        if (zfs_snapshot(lzeh->lzh, snap_buf, options->recursive, NULL) != 0) {
            return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to create snapshot for source dataset (%s).\n",
                                   snap_buf);
        }

        if (lzeh->bootpool.pool_zhdl != NULL) {
            boot_pool_cdata.is_snap = B_FALSE;
            // libze_str be_name;
            // Boot env name
            // We already checked length
            (void)strlcpy(boot_pool_cdata.source_dataset, lzeh->bootpool.env_activated_path, ZFS_MAX_DATASET_NAME_LEN);
            (void)strlcpy(boot_pool_cdata.snap_suffix, cdata.snap_suffix, ZFS_MAX_DATASET_NAME_LEN);
            if (libze_util_concat(boot_pool_cdata.source_dataset, "@", boot_pool_cdata.snap_suffix,
                                  ZFS_MAX_DATASET_NAME_LEN, snap_buf) >= ZFS_MAX_DATASET_NAME_LEN) {
                return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                       "Source boot pool dataset snapshot (%s@%s) exceeds max dataset length (%d).\n",
                                       boot_pool_cdata.source_dataset, boot_pool_cdata.snap_suffix,
                                       ZFS_MAX_DATASET_NAME_LEN);
            }
            if (zfs_snapshot(lzeh->lzh, snap_buf, options->recursive, NULL) != 0) {
                return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to create snapshot (%s).\n", snap_buf);
            }
        }
    }

    char clone_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->env_root, "/", options->be_name, ZFS_MAX_DATASET_NAME_LEN, clone_buf) >=
        ZFS_MAX_DATASET_NAME_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "New boot environment (%s/%s) exceeds max dataset length (%d).\n", lzeh->env_root,
                               options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }
    if (libze_clone(lzeh, cdata.source_dataset, cdata.snap_suffix, clone_buf, options->recursive) !=
        LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                               "The separate dataset on the bootpool (%s) can't be cloned.\n", clone_buf);
    }
    if (libze_util_concat(lzeh->bootpool.root_path_full, "", options->be_name, ZFS_MAX_DATASET_NAME_LEN, clone_buf)) {
        return libze_error_set(
            lzeh, LIBZE_ERROR_MAXPATHLEN,
            "The name for the bootpool dataset of new boot environment (%s%s) exceeds max dataset length (%d).\n",
            lzeh->bootpool.root_path_full, options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }
    if (libze_clone(lzeh, boot_pool_cdata.source_dataset, boot_pool_cdata.snap_suffix, clone_buf, options->recursive) !=
        LIBZE_ERROR_SUCCESS) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                               "The separate dataset on the bootpool (%s) can't be cloned.\n", clone_buf);
    }

    return LIBZE_ERROR_SUCCESS;
}

/*************************************
 ************** destroy **************
 *************************************/

typedef struct libze_destroy_cbdata {
    libze_handle *         lzeh;
    libze_destroy_options *options;
} libze_destroy_cbdata;

/**
 * @brief Destroy callback called for each child recursively
 * @param zh Handle of each dataset to dataset
 * @param data @p libze_destroy_cbdata callback data
 * @return non-zero if not successful
 */
static int libze_destroy_cb(zfs_handle_t *zh, void *data) {
    int                   ret = 0;
    libze_destroy_cbdata *cbd = data;

    const char *ds = zfs_get_name(zh);
    if (zfs_is_mounted(zh, NULL)) {
        if (cbd->options->force) {
            zfs_unmount(zh, NULL, 0);
        }
        else {
            return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN,
                                   "Dataset (%s) is mounted, run with force or unmount dataset.\n", ds);
        }
    }

    zfs_handle_t *origin_h = NULL;
    // Dont run destroy_origin if snap callback
    if ((strchr(zfs_get_name(zh), '@') == NULL) && cbd->options->destroy_origin) {
        // Check if clone, origin snapshot saved to buffer
        char buf[ZFS_MAX_DATASET_NAME_LEN] = "";
        if (zfs_prop_get(zh, ZFS_PROP_ORIGIN, buf, sizeof(buf), NULL, NULL, 0, 1) == 0) {
            // Destroy origin snapshot
            origin_h = zfs_open(cbd->lzeh->lzh, buf, ZFS_TYPE_SNAPSHOT);
            if (origin_h == NULL) {
                return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to open origin snapshot (%s).\n", buf);
            }
        }
    }

    // Destroy children recursively
    if (zfs_iter_children(zh, libze_destroy_cb, cbd) != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to iterate over children of %s.\n", ds);
    }
    // Destroy dataset, ignore error if inner snap recurse so destroy continues
    if (zfs_destroy(zh, B_FALSE) != 0) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to destroy dataset (%s).\n", ds);
    }

    if (cbd->options->destroy_origin && (origin_h != NULL)) {
        if ((ret = libze_destroy_cb(origin_h, cbd)) != 0) {
            libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to destroy origin snapshot (%s).\n",
                            zfs_get_name(origin_h));
        }
        zfs_close(origin_h);
    }

    return ret;
}

/**
 * @brief Destroy a boot environment clone or dataset
 * @param lzeh Initialized @p libze_handle
 * @param options Destroy options
 * @param filesystem Clone or boot environment full name
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_ZFS_OPEN if @p filesystem can't be opened,
 *         @p LIBZE_ERROR_EEXIST if @p filesystem doesn't exist,
 */
static libze_error destroy_filesystem(libze_handle *lzeh, libze_destroy_options *options,
                                      const char filesystem[ZFS_MAX_DATASET_NAME_LEN]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    if (!zfs_dataset_exists(lzeh->lzh, filesystem, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Dataset (%s) does not exist.\n", filesystem);
    }
    zfs_handle_t *be_zh = NULL;
    be_zh               = zfs_open(lzeh->lzh, filesystem, ZFS_TYPE_FILESYSTEM);
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed opening dataset (%s).\n", filesystem);
    }

    libze_destroy_cbdata cbd = {.lzeh = lzeh, .options = options};

    if (libze_destroy_cb(be_zh, &cbd) != 0) {
        ret = LIBZE_ERROR_UNKNOWN;
    }
    zfs_close(be_zh);

    return ret;
}

/**
 * @brief Destroy a boot environment snapshot
 * @param lzeh Initialized @p libze_handle
 * @param options Destroy options
 * @param snapshot Snapshot full name
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_ZFS_OPEN if @p snapshot dataset can't be opened,
 *         @p LIBZE_ERROR_EEXIST if @p snapshot doesn't exist, or isn't a BE,
 *         @p LIBZE_ERROR_MAXPATHLEN if snapshot name is too long
 */
static libze_error destroy_snapshot(libze_handle *lzeh, libze_destroy_options *options,
                                    const char snapshot[ZFS_MAX_DATASET_NAME_LEN]) {
    if (!zfs_dataset_exists(lzeh->lzh, snapshot, ZFS_TYPE_SNAPSHOT)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Snapshot (%s) doesn't exist.\n", snapshot);
    }
    char be_snap_ds_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    char be_full_ds[ZFS_MAX_DATASET_NAME_LEN]      = "";

    // Get boot environment name, we know ZFS_MAX_DATASET_NAME_LEN wont be exceeded
    (void)libze_util_cut(options->be_name, ZFS_MAX_DATASET_NAME_LEN, be_snap_ds_buff, '@');

    // Join BE name with BE root to verify requested snap is from a BE
    if (libze_util_concat(lzeh->env_root, "/", be_snap_ds_buff, ZFS_MAX_DATASET_NAME_LEN, be_full_ds)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Requested boot environment (%s/%s) exceeds max length (%d).\n", lzeh->env_root,
                               be_snap_ds_buff, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_name_valid(be_full_ds, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Invalid dataset (%s).\n", be_full_ds);
    }

    if (!zfs_dataset_exists(lzeh->lzh, be_full_ds, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Dataset (%s) does not exist.\n", be_full_ds);
    }

    zfs_handle_t *be_zh = zfs_open(lzeh->lzh, snapshot, ZFS_TYPE_SNAPSHOT);
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed opening dataset (%s).\n", snapshot);
    }

    if (zfs_destroy(be_zh, B_FALSE) != 0) {
        zfs_close(be_zh);
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Failed to destroy snapshot (%s).\n", snapshot);
    }
    zfs_close(be_zh);

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Check if a bootpool is set and if a dataset for the specified boot environment exists there.
 *        If this is true, delete it.
 * @param lzeh Initialized @p libze_handle
 * @param options Destroy options
 * @return @p LIBZE_ERROR_SUCCESS if no boot pool set, if boot pool dataset doesn't exist,
 *             or if boot pool dataset destroyed successfully.
 *         @p LIBZE_ERROR_EEXIST if the requested boot pool dataset nonexistent,
 *         @p LIBZE_ERROR_MAXPATHLEN if requested name of the dataset on the bootpool is too long,
 *         @p LIBZE_ERROR_ZFS_OPEN if @p filesystem can't be opened
 */
static libze_error libze_destroy_boot_pool(libze_handle *lzeh, libze_destroy_options *options) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    if (lzeh->bootpool.pool_zhdl == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "No additional bootpool available.\n");
    }

    char boot_pool_dataset[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->bootpool.root_path_full, "", options->be_name, ZFS_MAX_DATASET_NAME_LEN,
                          boot_pool_dataset) >= ZFS_MAX_DATASET_NAME_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Requested boot pool (%s%s) is too long (%d).\n",
                               lzeh->bootpool.root_path_full, options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }

    ret = destroy_filesystem(lzeh, options, boot_pool_dataset);

    // Ignore if doesn't exist, could be an old non-bootpool BE
    switch (ret) {
        case LIBZE_ERROR_EEXIST:
            return LIBZE_ERROR_SUCCESS;
        case LIBZE_ERROR_SUCCESS:
            return LIBZE_ERROR_SUCCESS;
        default:
            return ret;
    }
}

/**
 * @brief Destroy a boot environment
 * @param lzeh Initialized @p libze_handle
 * @param options Destroy options
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN If the requested environment to destroy is too long,
 *         @p LIBZE_ERROR_UNKNOWN If the environment is active,
 *         @p LIBZE_ERROR_EEXIST If the environment doesn't exist,
 *         @p LIBZE_ERROR_ZFS_OPEN If the dataset couldn't be opened,
 *         @p LIBZE_ERROR_PLUGIN If the plugin hook failed
 */
libze_error libze_destroy(libze_handle *lzeh, libze_destroy_options *options) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char be_ds_buff[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->env_root, "/", options->be_name, ZFS_MAX_DATASET_NAME_LEN, be_ds_buff) >=
        ZFS_MAX_DATASET_NAME_LEN) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Requested boot environment (%s/%s) exceeds max length (%d).\n", lzeh->env_root,
                               options->be_name, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (libze_is_active_be(lzeh, be_ds_buff)) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Can't destroy active boot environment (%s).\n",
                               options->be_name);
    }
    if (libze_is_root_be(lzeh, be_ds_buff)) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Can't destroy running boot environment (%s).\n",
                               options->be_name);
    }

    if ((strchr(be_ds_buff, '@') == NULL)) {
        if ((ret = destroy_filesystem(lzeh, options, be_ds_buff)) != LIBZE_ERROR_SUCCESS) {
            return ret;
        }

        if ((ret = libze_destroy_boot_pool(lzeh, options)) != LIBZE_ERROR_SUCCESS) {
            return ret;
        }
    }
    else {
        if ((ret = destroy_snapshot(lzeh, options, be_ds_buff)) != LIBZE_ERROR_SUCCESS) {
            return ret;
        }
    }

    if ((lzeh->lz_funcs != NULL) && (lzeh->lz_funcs->plugin_post_destroy(lzeh, options->be_name) != 0)) {
        return LIBZE_ERROR_PLUGIN;
    }

    return ret;
}

/**********************************
 ************** list **************
 **********************************/

typedef struct libze_list_cbdata {
    nvlist_t **   outnvl;
    libze_handle *lzeh;
} libze_list_cbdata_t;

/**
 * @brief Callback for each boot environment.
 * @param[in] zhdl Initialized zfs handle for boot environment being acted on.
 * @param[in,out] data Pointer to initialized @p libze_list_cbdata_t struct.
 * @return Non-zero on failure.
 *
 * @pre @p zhdl != NULL
 * @pre @p data != NULL
 */
static int libze_list_cb(zfs_handle_t *zhdl, void *data) {
    libze_list_cbdata_t *cbd                                  = data;
    char                 prop_buffer[ZFS_MAXPROPLEN]          = "";
    char                 dataset[ZFS_MAX_DATASET_NAME_LEN]    = "";
    char                 be_name[ZFS_MAX_DATASET_NAME_LEN]    = "";
    char                 mountpoint[ZFS_MAX_DATASET_NAME_LEN] = "";
    int                  ret                                  = LIBZE_ERROR_SUCCESS;
    nvlist_t *           props                                = NULL;

    const char *handle_name = zfs_get_name(zhdl);

    if (((props = fnvlist_alloc()) == NULL)) {
        return libze_error_set(cbd->lzeh, LIBZE_ERROR_NOMEM, "Failed to allocate nvlist.\n");
    }

    // Name
    if (zfs_prop_get(zhdl, ZFS_PROP_NAME, dataset, sizeof(dataset), NULL, NULL, 0, 1) != 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed get 'name' property for %s.\n", handle_name);
        goto err;
    }
    fnvlist_add_string(props, "dataset", dataset);

    // Boot env name
    if ((ret = libze_boot_env_name(cbd->lzeh, dataset, ZFS_MAX_DATASET_NAME_LEN, be_name)) != LIBZE_ERROR_SUCCESS) {
        goto err;
    }
    fnvlist_add_string(props, "name", be_name);

    // Mountpoint
    char mounted[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (zfs_prop_get(zhdl, ZFS_PROP_MOUNTED, mounted, sizeof(mounted), NULL, NULL, 0, 1) != 0) {
        ret =
            libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed get ZFS property 'mounted' for %s.\n", handle_name);
        goto err;
    }

    int is_mounted = strcmp(mounted, "yes");
    if (is_mounted == 0) {
        if (zfs_prop_get(zhdl, ZFS_PROP_MOUNTPOINT, mountpoint, sizeof(mountpoint), NULL, NULL, 0, 1) != 0) {
            ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed get FS property 'mountpoint' for %s.\n",
                                  handle_name);
            goto err;
        }
    }
    fnvlist_add_string(props, "mountpoint", (is_mounted == 0) ? mountpoint : "-");

    // Creation
    if (zfs_prop_get(zhdl, ZFS_PROP_CREATION, prop_buffer, sizeof(prop_buffer), NULL, NULL, 0, 1) != 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed get 'creation' for %s.\n", handle_name);
        goto err;
    }

    char                   t_buf[ULL_SIZE] = "";
    unsigned long long int formatted_time  = strtoull(prop_buffer, NULL, 10);
    // ISO 8601 date format
    if (strftime(t_buf, ULL_SIZE, "%F %H:%M", localtime((time_t *)&formatted_time)) == 0) {
        ret = libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed get time from creation for %s.\n", handle_name);
        goto err;
    }
    fnvlist_add_string(props, "creation", t_buf);

    // Nextboot
    fnvlist_add_boolean_value(props, "nextboot", libze_is_active_be(cbd->lzeh, dataset));

    // Active
    fnvlist_add_boolean_value(props, "active", libze_is_root_be(cbd->lzeh, dataset));

    fnvlist_add_nvlist(*cbd->outnvl, prop_buffer, props);

    return ret;
err:
    fnvlist_free(props);
    return ret;
}

/**
 * @brief Prepare a listing with valid properies
 * @param[in] lzeh Initialized @p libze_handle
 * @param[in,out] outnvl Reference to an @p nvlist_t*, populated with valid 'list properties'
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_LIBZFS, @p LIBZE_ERROR_ZFS_OPEN or @p LIBZE_ERROR_NOMEM on failure.
 */
libze_error libze_list(libze_handle *lzeh, nvlist_t **outnvl) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    if ((libzfs_core_init()) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_LIBZFS, "Failed to initialize libzfs_core.\n");
        goto err;
    }

    // Get be root handle
    zfs_handle_t *zroot_hdl = NULL;
    if ((zroot_hdl = zfs_open(lzeh->lzh, lzeh->env_root, ZFS_TYPE_FILESYSTEM)) == NULL) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_LIBZFS, "Failed to open handle to %s.\n", lzeh->env_root);
        goto err;
    }

    // Out nvlist callback
    if ((*outnvl = fnvlist_alloc()) == NULL) {
        ret = libze_error_nomem(lzeh);
        goto err;
    }
    libze_list_cbdata_t cbd = {.outnvl = outnvl, .lzeh = lzeh};

    zfs_iter_filesystems(zroot_hdl, libze_list_cb, &cbd);

err:
    zfs_close(zroot_hdl);
    libzfs_core_fini();
    return ret;
}

/*********************************
 ************** Mount **************
 *********************************/

typedef struct libze_mount_cb_data {
    libze_handle *lzeh;
    const char *  mountpoint;
} libze_mount_cb_data;

/**
 * @brief Create a directory
 * @param path Path of directory to create if it doesn't exist
 * @return non-zero on failure.
 */
static int directory_create_if_nonexistent(const char path[static 1]) {
    DIR *dir = opendir(path);
    if (dir != NULL) {
        // Directory exists
        return closedir(dir);
    }

    return mkdir(path, 0700);
}

/**
 * @brief Mount the boot pool dataset
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] boot_environment Boot environment name to mount boot pool dataset for
 * @param[in] mount_directory Path to the directory where `boot_environment` is mounted to
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if the requested boot pool dataset nonexistent,
 *         @p LIBZE_ERROR_LIBZFS if the mountpoint property can't be retrieved or the dataset can't be mounted,
 *         @p LIBZE_ERROR_MAXPATHLEN if the name of a dataset or path is longer than allowed,
 *         @p LIBZE_ERROR_MKDIR if the mountpoint directory does not exist and can't be created,
 *         @p LIBZE_ERROR_NOT_IMPLEMENTED if the ZFS mountpoint of the requested boot pool is not legacy,
 *         @p LIBZE_ERROR_ZFS_OPEN if the requested boot pool dataset can't be opened
 *
 * @pre mount_directory != NULL and points to a valid directory
 */
libze_error mount_boot_pool(libze_handle *lzeh, const char boot_environment[static 1],
                            const char mount_directory[ZFS_MAX_DATASET_NAME_LEN]) {

    if (lzeh->bootpool.pool_zhdl == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "No additional bootpool available.\n");
    }

    char target_dataset[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->bootpool.root_path_full, "", boot_environment, LIBZE_MAX_PATH_LEN, target_dataset)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Name for the requested boot dataset (%s%s) is too long (%d).\n",
                               lzeh->bootpool.root_path_full, boot_environment, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, target_dataset, ZFS_TYPE_DATASET)) { // NOLINT(hicpp-signed-bitwise)
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "The requested boot dataset (%s) does not exist.\n",
                               target_dataset);
    }

    zfs_handle_t *target_dataset_hndl = zfs_open(lzeh->lzh, target_dataset, ZFS_TYPE_FILESYSTEM);
    if (target_dataset_hndl == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed to open the requested boot dataset (%s).\n",
                               target_dataset);
    }

    char prop_buf[ZFS_MAXPROPLEN] = "";
    if (zfs_prop_get(target_dataset_hndl, ZFS_PROP_MOUNTPOINT, prop_buf, sizeof(prop_buf), NULL, NULL, 0, 1) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_LIBZFS,
                               "Failed to get the mountpoint for the requested boot dataset (%s).\n", target_dataset);
    }

    // The mountpoint of the specified boot dataset is set to legacy.
    // Mount it manually to ../boot
    if (strcmp(prop_buf, "legacy") == 0) {
        char mount_directory_boot[LIBZE_MAX_PATH_LEN] = "";
        if (libze_util_concat(mount_directory, "/", "boot", LIBZE_MAX_PATH_LEN, mount_directory_boot) >=
            LIBZE_MAX_PATH_LEN) {
            return libze_error_set(
                lzeh, LIBZE_ERROR_MAXPATHLEN,
                "Path to the boot directory (%s/boot) for the requested boot dataset (%s) is too long (%d).\n",
                mount_directory, target_dataset, LIBZE_MAX_PATH_LEN);
        }

        if (directory_create_if_nonexistent(mount_directory_boot) != 0) {
            return libze_error_set(lzeh, LIBZE_ERROR_MKDIR,
                                   "Failed to create mountpoint (%s) for (%s), or a file existed there.\n.",
                                   mount_directory_boot, target_dataset);
        }

        if (libze_util_temporary_mount(target_dataset, mount_directory_boot) != LIBZE_ERROR_SUCCESS) {
            return libze_error_set(
                lzeh, LIBZE_ERROR_MOUNT,
                "Failed to mount the boot directory (%s) for the requested boot dataset (%s) in legacy mode.\n.",
                mount_directory_boot, target_dataset);
        }
    }
    else {
        // TODO mount temporary if not legacy ...
        return libze_error_set(lzeh, LIBZE_ERROR_NOT_IMPLEMENTED,
                               "Mounting a boot dataset which is not set to 'legacy' is currently not supported.\n");
    }
}

/**
 * @brief Mount callback called for each child of boot environment
 * @param zh Handle to current dataset to mount
 * @param data @p libze_mount_cb_data object
 * @return Non-zero on failure.
 */
static int mount_callback(zfs_handle_t *zh, void *data) {
    libze_mount_cb_data *cbd                      = data;
    const char *         dataset                  = zfs_get_name(zh);
    char                 prop_buf[ZFS_MAXPROPLEN] = "";

    // Get mountpoint
    if (zfs_prop_get(zh, ZFS_PROP_MOUNTPOINT, prop_buf, sizeof(prop_buf), NULL, NULL, 0, 1) != 0) {
        (void)libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to get mountpoint for %s.\n.", dataset);
        return -1;
    }

    // No mountpoint, just for hierarchy, or not ZFS managed so skip
    if ((strcmp(prop_buf, "none") == 0) || (strcmp(prop_buf, "legacy") == 0)) {
        return zfs_iter_filesystems(zh, mount_callback, cbd);
    }

    char mountpoint_buf[LIBZE_MAX_PATH_LEN] = "";
    if (libze_util_concat(cbd->mountpoint, "", prop_buf, LIBZE_MAX_PATH_LEN, mountpoint_buf) >= LIBZE_MAX_PATH_LEN) {
        (void)libze_error_set(cbd->lzeh, LIBZE_ERROR_MAXPATHLEN,
                              "Path to the requested dataset (%s%s) for mounting is too long (%d).\n.", cbd->mountpoint,
                              prop_buf, LIBZE_MAX_PATH_LEN);
        return -1;
    }

    if (zfs_is_mounted(zh, NULL)) {
        (void)libze_error_set(cbd->lzeh, LIBZE_ERROR_MOUNT, "Dataset (%s) is already mounted.\n", dataset);
        return -1;
    }

    if (directory_create_if_nonexistent(mountpoint_buf) != 0) {
        (void)libze_error_set(
            cbd->lzeh, LIBZE_ERROR_MKDIR,
            "Failed to create a mountpoint (%s) for the specified dataset (%s), or a file existed there.\n",
            mountpoint_buf, dataset);
        return -1;
    }

    if (libze_util_temporary_mount(dataset, mountpoint_buf) != LIBZE_ERROR_SUCCESS) {
        (void)libze_error_set(cbd->lzeh, LIBZE_ERROR_MOUNT,
                              "Failed to mount the dataset (%s) to the mountpoint (%s).\n", dataset, mountpoint_buf);
        return -1;
    }

    return zfs_iter_filesystems(zh, mount_callback, cbd);
}

/**
 * @brief Recursively mount boot environment
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] boot_environment Boot environment to mount
 * @param[in] mountpoint Mountpoint for boot environment.
 *            If @p NULL a temporary mountpoint will be created
 * @param[out] mountpoint_buffer Mountpoint boot environment was mounted to.
 *            If @p mountpoint was @p NULL, the temporary mountpoint will be here.
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if a path length exceeded,
 *         @p LIBZE_ERROR_ZFS_OPEN if a dataset handle can't be opened,
 *         @p LIBZE_ERROR_UNKNOWN if failure to set properties or the dataset is already mounted
 */
libze_error libze_mount(libze_handle *lzeh, const char boot_environment[static 1], const char *mountpoint,
                        char mountpoint_buffer[LIBZE_MAX_PATH_LEN]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;
    const char *real_mountpoint;
    char        dataset_buffer[ZFS_MAX_DATASET_NAME_LEN];

    if (libze_util_concat(lzeh->env_root, "/", boot_environment, ZFS_MAX_DATASET_NAME_LEN, dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Dataset name from requested boot environment (%s/%s) exceeds max length (%d).\n",
                               lzeh->env_root, boot_environment, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, dataset_buffer, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                               "Dataset from requested boot environment (%s) doesn't exist.\n", dataset_buffer);
    }

    if (libze_is_root_be(lzeh, dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "Requested boot environment (%s) is currently running!\n",
                               boot_environment);
    }

    zfs_handle_t *be_zh = zfs_open(lzeh->lzh, dataset_buffer, ZFS_TYPE_FILESYSTEM);
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                               "Failed to open dataset (%s) of requested boot environment.\n", dataset_buffer);
    }

    if (zfs_is_mounted(be_zh, NULL)) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                              "Requested dataset for boot environment (%s) is already mounted.\n", dataset_buffer);
        goto err;
    }

    char tmpdir_template[LIBZE_MAX_PATH_LEN] = "";
    if (libze_util_concat("/tmp/ze.", boot_environment, ".XXXXXX", LIBZE_MAX_PATH_LEN, tmpdir_template)) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                              "Name of directory template (/tmp/ze.%s.XXXXXX) exceeds max length (%d).\n",
                              boot_environment, LIBZE_MAX_PATH_LEN);
        goto err;
    }

    boolean_t tmpdir_created = B_FALSE;
    if (mountpoint == NULL) {
        // Create tmp mountpoint
        real_mountpoint = mkdtemp(tmpdir_template);
        if (real_mountpoint == NULL) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_MKDIR, "Could not create tmp directory (%s).\n", tmpdir_template);
            goto err;
        }
        tmpdir_created = B_TRUE;
    }
    else {
        real_mountpoint = mountpoint;
    }

    if (strlcpy(mountpoint_buffer, real_mountpoint, LIBZE_MAX_PATH_LEN) >= LIBZE_MAX_PATH_LEN) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "Mountpoint (%s) exceeds max length (%d).\n",
                              real_mountpoint, LIBZE_MAX_PATH_LEN);
        goto err;
    }

    if (libze_util_temporary_mount(dataset_buffer, real_mountpoint) != LIBZE_ERROR_SUCCESS) {
        if (tmpdir_created) {
            (void)rmdir(real_mountpoint);
        }
        ret = libze_error_set(lzeh, LIBZE_ERROR_MOUNT, "Failed to mount the boot environment (%s) to %s.\n",
                              boot_environment, real_mountpoint);
        goto err;
    }

    libze_mount_cb_data cbd = {.lzeh = lzeh, .mountpoint = real_mountpoint};

    if (zfs_iter_filesystems(be_zh, mount_callback, &cbd) != 0) {
        ret = lzeh->libze_error;
        goto err;
    }

    if (lzeh->bootpool.pool_zhdl != NULL) {
        ret = mount_boot_pool(lzeh, boot_environment, real_mountpoint);
    }

err:
    zfs_close(be_zh);
    return ret;
}

/************************************
 ************** Rename **************
 ************************************/

/**
 * @brief Rename a boot environment
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] boot_environment Original boot environment
 * @param[in] new_boot_environment Renamed boot environment
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if dataset to rename is nonexistent,
 *         @p LIBZE_ERROR_MAXPATHLEN if boot environment exceeds max dataset length,
 *         @p LIBZE_ERROR_UNKNOWN if unknown error,
 *         @p LIBZE_ERROR_ZFS_OPEN if dataset could not be opened
 */
libze_error libze_rename(libze_handle *lzeh, const char boot_environment[static 1],
                         const char new_boot_environment[static 1]) {

    libze_error   ret                                          = LIBZE_ERROR_SUCCESS;
    zfs_handle_t *be_zh                                        = NULL;
    char          dataset_buffer[ZFS_MAX_DATASET_NAME_LEN]     = "";
    char          new_dataset_buffer[ZFS_MAX_DATASET_NAME_LEN] = "";

    zfs_handle_t *boot_pool_zh                                      = NULL;
    char          boot_pool_ds_buffer[ZFS_MAX_DATASET_NAME_LEN]     = "";
    char          new_boot_pool_ds_buffer[ZFS_MAX_DATASET_NAME_LEN] = "";

    // Dataset name before renaming
    if (libze_util_concat(lzeh->env_root, "/", boot_environment, ZFS_MAX_DATASET_NAME_LEN, dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "The old dataset name (%s/%s) exceeds max length (%d).\n",
                               lzeh->env_root, boot_environment, ZFS_MAX_DATASET_NAME_LEN);
    }

    // Dataset name after renaming
    if (libze_util_concat(lzeh->env_root, "/", new_boot_environment, ZFS_MAX_DATASET_NAME_LEN, new_dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN, "The new dataset name (%s/%s) exceeds max length (%d).\n",
                               lzeh->env_root, new_dataset_buffer, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, dataset_buffer, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "The old dataset (%s) doesn't exist.\n", boot_environment);
    }
    if (zfs_dataset_exists(lzeh->lzh, new_dataset_buffer, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "The new dataset (%s) already exists.\n",
                               new_boot_environment);
    }

    if (libze_is_root_be(lzeh, dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Can't rename the currently running boot environment (%s).\n",
                               boot_environment);
    }
    if (libze_is_active_be(lzeh, dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Can't rename the currently active boot environment (%s).\n",
                               boot_environment);
    }

    be_zh = zfs_open(lzeh->lzh, dataset_buffer, ZFS_TYPE_FILESYSTEM);
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed to open the dataset (%s).\n", dataset_buffer);
    }

    if (zfs_is_mounted(be_zh, NULL)) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "The dataset (%s) is mounted and can't be renamed.\n",
                              dataset_buffer);
        goto err;
    }

    // Check boot pool validity
    if (lzeh->bootpool.pool_zhdl != NULL) {
        if (libze_util_concat(lzeh->bootpool.root_path_full, "", boot_environment, ZFS_MAX_DATASET_NAME_LEN,
                              boot_pool_ds_buffer) != LIBZE_ERROR_SUCCESS) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                  "The old dataset name from bootpool (%s%s) exceeds max length (%d).\n",
                                  lzeh->bootpool.root_path_full, boot_environment, ZFS_MAX_DATASET_NAME_LEN);
            goto err;
        }

        // Renamed dataset
        if (libze_util_concat(lzeh->bootpool.root_path_full, "", new_boot_environment, ZFS_MAX_DATASET_NAME_LEN,
                              new_boot_pool_ds_buffer)) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                                  "The new dataset name from bootpool (%s%s) exceeds max length (%d).\n",
                                  lzeh->bootpool.root_path_full, new_boot_environment, ZFS_MAX_DATASET_NAME_LEN);
            goto err;
        }

        if (!zfs_dataset_exists(lzeh->lzh, boot_pool_ds_buffer, ZFS_TYPE_FILESYSTEM)) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "The old bootpool dataset (%s) doesn't exist.\n",
                                  boot_pool_ds_buffer);
            goto err;
        }
        if (zfs_dataset_exists(lzeh->lzh, new_boot_pool_ds_buffer, ZFS_TYPE_FILESYSTEM)) {
            ret =
                libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                                "The bootpool dataset (%s) can't be renamed because the target (%s) already exists.\n",
                                boot_pool_ds_buffer, new_boot_pool_ds_buffer);
            goto err;
        }

        boot_pool_zh = zfs_open(lzeh->lzh, boot_pool_ds_buffer, ZFS_TYPE_FILESYSTEM);
        if (boot_pool_zh == NULL) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed to open the old bootpool dataset (%s).\n",
                                  boot_pool_ds_buffer);
            goto err;
        }

        if (zfs_is_mounted(be_zh, NULL)) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN,
                                  "The old bootpool dataset (%s) is mounted, can't rename.\n", boot_pool_ds_buffer);
            goto err;
        }
    }

    // Go ahead with rename, checks passed

    // No recurse, no create parents
    if (zfs_rename(be_zh, new_dataset_buffer, B_FALSE, B_FALSE) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Rename of dataset (%s) failed.\n", dataset_buffer);
        goto err;
    }

    if (lzeh->bootpool.pool_zhdl != NULL) {
        // No recurse, no create parents
        if (zfs_rename(boot_pool_zh, new_boot_pool_ds_buffer, B_FALSE, B_FALSE) != 0) {
            ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Rename of bootpool dataset (%s) failed.\n", boot_pool_zh);
        }
    }

err:
    zfs_close(be_zh);
    if (boot_pool_zh != NULL) {
        zfs_close(boot_pool_zh);
    }
    return ret;
}

/*********************************
 ************** Set **************
 *********************************/

/**
 * @brief Set a list of properties on the BE root
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] properties List of ZFS properties to set
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_UNKNOWN if failure to set properties on @p lzeh->env_root,
 *         @p LIBZE_ERROR_ZFS_OPEN if failure to open @p lzeh->env_root
 */
libze_error libze_set(libze_handle *lzeh, nvlist_t *properties) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    zfs_handle_t *env_root_zhdl = zfs_open(lzeh->lzh, lzeh->env_root, ZFS_TYPE_FILESYSTEM);
    if (env_root_zhdl == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                               "Failed to open the root dataset (%s) of all boot environments.\n", lzeh->env_root);
    }

    if (zfs_prop_set_list(env_root_zhdl, properties) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Failed to set properties for root dataset (%s).\n",
                              lzeh->env_root);
    }

    zfs_close(env_root_zhdl);
    return ret;
}

/*************************************
 ************** Snapshot **************
 *************************************/

/**
 * @brief Take a snapshot of boot pool dataset
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] boot_environment Boot environment name to snapshot boot pool dataset for
 * @param[in] snap_suffix Snapshot name
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if boot pool dataset nonexistent,
 *         @p LIBZE_ERROR_MAXPATHLEN if max dataset or snapshot length exceeded
 */
static libze_error snapshot_boot_pool(libze_handle *lzeh, const char boot_environment[static 1],
                                      const char snap_suffix[ZFS_MAX_DATASET_NAME_LEN]) {

    char boot_pool_buf[ZFS_MAX_DATASET_NAME_LEN] = "";
    char snap_buf[ZFS_MAX_DATASET_NAME_LEN]      = "";

    if (lzeh->bootpool.pool_zhdl == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "No additional bootpool available.\n");
        // No bootpool set
    }

    // Create snapshot from boot dataset which belongs to the specified environment
    if (libze_util_concat(lzeh->bootpool.root_path_full, "", boot_environment, ZFS_MAX_DATASET_NAME_LEN,
                          boot_pool_buf)) {
        return libze_error_set(
            lzeh, LIBZE_ERROR_MAXPATHLEN,
            "Path to the dataset for the specified boot environment on bootpool (%s%s) exceeds max length (%d).\n",
            lzeh->bootpool.root_path_full, boot_environment, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, boot_pool_buf, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                               "Dataset for the specified boot enviornment on bootpool (%s) doesn't exist.\n",
                               boot_pool_buf);
    }

    if (libze_util_concat(boot_pool_buf, "@", snap_suffix, ZFS_MAX_DATASET_NAME_LEN, snap_buf)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Snapshot name of the dataset for the specified boot environment on bootpool (%s@%s) "
                               "exceeds max dataset length (%d).\n",
                               boot_pool_buf, snap_suffix, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (zfs_snapshot(lzeh->lzh, snap_buf, B_TRUE, NULL) != 0) {
        return libze_error_set(
            lzeh, LIBZE_ERROR_MAXPATHLEN,
            "Failed to take snapshot of the dataset for the specified boot environment on bootpool (%s).\n", snap_buf);
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * @brief Take a snapshot of a boot environment
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] boot_environment Boot environment name to snapshot
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if a datataset to snapshot is nonexistent,
 *         @p LIBZE_ERROR_MAXPATHLEN if max dataset or snapshot length exceeded
 */
libze_error libze_snapshot(libze_handle *lzeh, const char boot_environment[static 1]) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    char dataset_buffer[ZFS_MAX_DATASET_NAME_LEN] = "";

    if (libze_util_concat(lzeh->env_root, "/", boot_environment, ZFS_MAX_DATASET_NAME_LEN, dataset_buffer)) {
        return libze_error_set(
            lzeh, LIBZE_ERROR_MAXPATHLEN,
            "Path to the dataset for the specified boot environment (%s/%s) exceeds max length (%d).\n", lzeh->env_root,
            boot_environment, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, dataset_buffer, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                               "Dataset for the specified boot environment (%s) doesn't exist.\n", dataset_buffer);
    }

    char snap_buf[ZFS_MAX_DATASET_NAME_LEN]    = "";
    char snap_suffix[ZFS_MAX_DATASET_NAME_LEN] = "";
    (void)gen_snap_suffix(ZFS_MAX_DATASET_NAME_LEN, snap_suffix);
    if (libze_util_concat(dataset_buffer, "@", snap_suffix, ZFS_MAX_DATASET_NAME_LEN, snap_buf)) {
        return libze_error_set(
            lzeh, LIBZE_ERROR_MAXPATHLEN,
            "Snapshot name of the dataset for the specified boot environment (%s@%s) exceeds max length (%d).\n",
            dataset_buffer, snap_suffix, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (zfs_snapshot(lzeh->lzh, snap_buf, B_TRUE, NULL) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Failed to take snapshot of the dataset for the specified boot environment (%s).\n",
                               snap_buf);
    }

    if (lzeh->bootpool.pool_zhdl != NULL) {
        ret = snapshot_boot_pool(lzeh, boot_environment, snap_suffix);
    }

    return ret;
}

/*************************************
 ************** Unmount **************
 *************************************/

typedef struct libze_umount_cb_data {
    libze_handle *lzeh;
} libze_umount_cb_data;

/**
 * @brief Unmount the boot pool dataset
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] boot_environment Boot environment name to unmount boot pool dataset for
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if the requested boot pool dataset nonexistent,
 *         @p LIBZE_ERROR_LIBZFS if the mountpoint property can't be retrieved or the dataset can't be unmounted,
 *         @p LIBZE_ERROR_MAXPATHLEN if the name of a dataset or path is longer than allowed,
 *         @p LIBZE_ERROR_NOT_IMPLEMENTED if the ZFS mountpoint of the requested dataset is not legacy,
 *         @p LIBZE_ERROR_ZFS_OPEN if the requested boot pool dataset can't be opened
 *
 */
libze_error unmount_boot_pool(libze_handle *lzeh, const char boot_environment[static 1]) {
    if (lzeh->bootpool.pool_zhdl == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "No additional bootpool available.\n");
    }

    char target_dataset[ZFS_MAX_DATASET_NAME_LEN] = "";
    if (libze_util_concat(lzeh->bootpool.root_path_full, "", boot_environment, LIBZE_MAX_PATH_LEN, target_dataset)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Name for the requested boot dataset (%s%s) is too long (%d).\n",
                               lzeh->bootpool.root_path_full, boot_environment, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, target_dataset, ZFS_TYPE_DATASET)) { // NOLINT(hicpp-signed-bitwise)
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST, "The requested boot dataset (%s) does not exist.\n",
                               target_dataset);
    }

    zfs_handle_t *target_dataset_hndl = zfs_open(lzeh->lzh, target_dataset, ZFS_TYPE_FILESYSTEM);
    if (target_dataset_hndl == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN, "Failed to open the requested boot dataset (%s).\n",
                               target_dataset);
    }

    char prop_buf[ZFS_MAXPROPLEN] = "";
    if (zfs_prop_get(target_dataset_hndl, ZFS_PROP_MOUNTPOINT, prop_buf, sizeof(prop_buf), NULL, NULL, 0, 1) != 0) {
        return libze_error_set(lzeh, LIBZE_ERROR_LIBZFS,
                               "Failed to get the mountpoint for the requested boot dataset (%s).\n", target_dataset);
    }

    libze_error ret = LIBZE_ERROR_SUCCESS;
    if (zfs_unmount(target_dataset_hndl, NULL, 0) != 0) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_MOUNT, "Failed to unmount the mounted boot dataset (%s).\n",
                              target_dataset);
    }
    zfs_close(target_dataset_hndl);
    return ret;
}

/**
 * @brief Mount callback called for each child of boot environment
 * @param zh Handle to current dataset to mount
 * @param data @p libze_mount_cb_data object
 * @return Non-zero on failure.
 */
static int unmount_callback(zfs_handle_t *zh, void *data) {
    libze_mount_cb_data *cbd     = data;
    const char *         dataset = zfs_get_name(zh);

    if (zfs_iter_filesystems(zh, unmount_callback, cbd) != 0) {
        (void)libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to iterate over %s.\n.", dataset);
        return -1;
    }

    if (!zfs_is_mounted(zh, NULL)) {
        return 0;
    }

    if (zfs_unmount(zh, NULL, 0) != 0) {
        (void)libze_error_set(cbd->lzeh, LIBZE_ERROR_UNKNOWN, "Failed to unmount %s.\n.", dataset);
        return -1;
    }
    return 0;
}

/**
 * @brief Recursively unmount boot environment
 * @param[in] lzeh Initialized lzeh libze handle
 * @param[in] boot_environment Boot environment to unmount
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_EEXIST if the specified dataset doesn't exist or isn't mounted,
 *         @p LIBZE_ERROR_MAXPATHLEN if the length of a filesystem path or dataset name exceeded,
 *         @p LIBZE_ERROR_UNKNOWN if a property can't be set or the current environment is specified,
 *         @p LIBZE_ERROR_ZFS_OPEN if a handle to the dataset can't be opened
 */
libze_error libze_unmount(libze_handle *lzeh, const char boot_environment[static 1]) {
    libze_error ret                                      = LIBZE_ERROR_SUCCESS;
    char        dataset_buffer[ZFS_MAX_DATASET_NAME_LEN] = "";

    if (libze_util_concat(lzeh->env_root, "/", boot_environment, ZFS_MAX_DATASET_NAME_LEN, dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_MAXPATHLEN,
                               "Path to the specified boot environment (%s/%s) exceeds max dataset length (%d).\n",
                               lzeh->env_root, boot_environment, ZFS_MAX_DATASET_NAME_LEN);
    }

    if (!zfs_dataset_exists(lzeh->lzh, dataset_buffer, ZFS_TYPE_FILESYSTEM)) {
        return libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                               "Dataset for the specified boot environment (%s) doesn't exist.\n", dataset_buffer);
    }

    if (libze_is_root_be(lzeh, dataset_buffer)) {
        return libze_error_set(lzeh, LIBZE_ERROR_UNKNOWN, "Can't unmount currently running boot environment (%s).\n",
                               boot_environment);
    }

    zfs_handle_t *be_zh = zfs_open(lzeh->lzh, dataset_buffer, ZFS_TYPE_FILESYSTEM);
    if (be_zh == NULL) {
        return libze_error_set(lzeh, LIBZE_ERROR_ZFS_OPEN,
                               "Failed to open dataset for the specified boot environment (%s).\n", dataset_buffer);
    }

    if (!zfs_is_mounted(be_zh, NULL)) {
        ret = libze_error_set(lzeh, LIBZE_ERROR_EEXIST,
                              "Dataset for the specified boot environment (%s) is not mounted.\n", dataset_buffer);
        goto err;
    }

    libze_umount_cb_data cbd = {.lzeh = lzeh};

    if (lzeh->bootpool.pool_zhdl != NULL) {
        (void)unmount_boot_pool(lzeh, boot_environment);
    }

    if (unmount_callback(be_zh, &cbd) != 0) {
        ret = lzeh->libze_error;
        goto err;
    }

err:
    zfs_close(be_zh);
    return ret;
}