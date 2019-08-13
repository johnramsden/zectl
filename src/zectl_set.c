#include "zectl.h"

/**
 * @brief Given a property with an optional prefix for a bootloader,
 *        form a ZFS property nvlist
 * @param[in,out] properties Pre-allocated nvlist to add a property to
 * @param property Individual property to add to the list
 * @return @p LIBZE_ERROR_SUCCESS on success,
 *         @p LIBZE_ERROR_MAXPATHLEN if property is too long,
 *         @p LIBZE_ERROR_UNKNOWN if no '=' in property
 * @pre @p properties is allocated and non NULL
 */
static libze_error
add_property(nvlist_t *properties, const char *property) {
    libze_error ret = LIBZE_ERROR_SUCCESS;

    // Property value, part after '='
    char *value = NULL;

    /* Prefix, just ZE_NAMESPACE if no colon in property
     * Otherwise ZE_NAMESPACE + part before colon */
    char prop_prefix[ZFS_MAXPROPLEN];

    // Full resulting ZFS property
    char prop_full_name[ZFS_MAXPROPLEN];
    char prop_after_colon[ZFS_MAXPROPLEN];
    char temp_prop[ZFS_MAXPROPLEN];

    if (strlcpy(temp_prop, property, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
        fprintf(stderr, "property '%s' is too long\n", property);
        return LIBZE_ERROR_MAXPATHLEN;
    }

    char *suffix_value = NULL;
    if ((suffix_value = strchr(temp_prop, ':')) != NULL) {
        // Cut at ':'
        *suffix_value = '\0'; // Prefix in temp_prop
        suffix_value++;
        ret = libze_util_concat(ZE_PROP_NAMESPACE, ".", temp_prop, ZFS_MAXPROPLEN, prop_prefix);
        if (ret != LIBZE_ERROR_SUCCESS) {
            fprintf(stderr, "property '%s' is too long\n", property);
            return ret;
        }
    } else {
        if (strlcpy(prop_prefix, ZE_PROP_NAMESPACE, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
            fprintf(stderr, "property '%s' is too long\n", property);
            return LIBZE_ERROR_MAXPATHLEN;
        }
        suffix_value = temp_prop;
    }

    if (strlcpy(prop_after_colon, suffix_value, ZFS_MAXPROPLEN) >= ZFS_MAXPROPLEN) {
        fprintf(stderr, "property '%s' is too long\n", property);
        return LIBZE_ERROR_MAXPATHLEN;
    }
    if ((value = strchr(prop_after_colon, '=')) == NULL) {
        fprintf(stderr, "missing '=' for property=value argument\n");
        return LIBZE_ERROR_UNKNOWN;
    }

    // Cut at '='
    *value = '\0';
    value++;

    ret = libze_util_concat(prop_prefix, ":", prop_after_colon, ZFS_MAXPROPLEN, prop_full_name);
    if (ret != LIBZE_ERROR_SUCCESS) {
        fprintf(stderr, "property '%s' is too long\n", property);
        return ret;
    }

    if (nvlist_exists(properties, prop_full_name)) {
        fprintf(stderr, "property '%s' specified multiple times\n", property);
        return LIBZE_ERROR_UNKNOWN;
    }

    if (nvlist_add_string(properties, prop_full_name, value) != 0) {
        return LIBZE_ERROR_NOMEM;
    }

    return LIBZE_ERROR_SUCCESS;
}

/**
 * set command main function
 * @param lzeh Initialized handle to libze object
 * @param argc As passed to main
 * @param argv As passed to main, contains boot env to create
 * @return LIBZE_ERROR_SUCCESS upon success
 */
libze_error
ze_set(libze_handle *lzeh, int argc, char **argv) {

    libze_error ret = LIBZE_ERROR_SUCCESS;
    nvlist_t *properties = NULL;

    if (argc == 0) {
        fprintf(stderr, "No properties provided\n");
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    if (argc > 1 && argv[1][0] == '-') {
        fprintf(stderr, "%s set: unknown option '-%c'\n", ZE_PROGRAM, argv[1][1]);
        ze_usage();
        return LIBZE_ERROR_UNKNOWN;
    }

    properties = fnvlist_alloc();
    if (properties == NULL) {
        return LIBZE_ERROR_NOMEM;
    }

    for (int i = 1; i < argc; i++) {
        if ((ret = add_property(properties, argv[i])) != LIBZE_ERROR_SUCCESS) {
            goto err;
        }
    }

    if ((ret = libze_set(lzeh, properties)) != LIBZE_ERROR_SUCCESS) {
        fputs(lzeh->libze_error_message, stderr);
    }

err:
    fnvlist_free(properties);
    return ret;
}