/* CANDIDATE NON COMPILE. Real libpff headers are mandatory, no guessed ABI/ctypes.
 * Names in this file must be reviewed against the pinned upstream header.
 * Argument lists are NOT verified by the supplied documentary pack.
 * No bypass: build_libpff.py requires a separate API review record.
 */
#ifndef G1_API_REVIEWED
#error "STOP: pinned libpff headers/signatures have not been reviewed"
#endif
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <libpff.h>

static char last_error[8192];
static int result(int rc, libpff_error_t **error, const char *operation) {
    snprintf(last_error, sizeof(last_error), "%s: rc=%d", operation, rc);
    if (*error != NULL) {
        size_t used = 0;
        while (last_error[used] && used < sizeof(last_error) - 2) used++;
        last_error[used++] = '\n';
        libpff_error_backtrace_sprint(*error, last_error + used, sizeof(last_error) - used);
        last_error[sizeof(last_error) - 1] = '\0';
        libpff_error_free(error);
    }
    return rc;
}
const char *g1_error(void) { return last_error; }
const char *g1_version(void) { return libpff_get_version(); }
const char *g1_review_hash(void) { return G1_REVIEW_SHA256; }

int g1_open(const char *path, void **output) {
    libpff_file_t *file = NULL;
    libpff_error_t *error = NULL;
    int rc = libpff_file_initialize(&file, &error);
    if (rc != 1) return result(rc, &error, "file_initialize");
    rc = libpff_file_open(file, path, LIBPFF_OPEN_READ, &error);
    if (rc != 1) {
        result(rc, &error, "file_open READ");
        libpff_file_free(&file, NULL);
        return -1;
    }
    *output = file;
    return 1;
}
int g1_close(void **handle) {
    libpff_file_t *file = (libpff_file_t *)*handle;
    libpff_error_t *error = NULL;
    int rc = libpff_file_close(file, &error);
    if (rc != 0) {
        result(rc, &error, "file_close");
        libpff_file_free(&file, NULL);
        *handle = NULL;
        return -1;
    }
    rc = libpff_file_free(&file, &error);
    *handle = NULL;
    return result(rc, &error, "file_free");
}
int g1_root(void *file, int message_store, void **out) {
    libpff_item_t *item = NULL;
    libpff_error_t *error = NULL;
    int rc = message_store ? libpff_file_get_message_store(file, &item, &error)
                           : libpff_file_get_root_folder(file, &item, &error);
    *out = item;
    return result(rc, &error, message_store ? "get_message_store" : "get_root_folder");
}
int g1_free_item(void **handle) {
    libpff_item_t *item = (libpff_item_t *)*handle;
    libpff_error_t *error = NULL;
    int rc = libpff_item_free(&item, &error);
    *handle = NULL;
    return result(rc, &error, "item_free");
}
int g1_nid(void *item, uint32_t *nid) {
    libpff_error_t *error = NULL;
    int rc = libpff_item_get_identifier(item, nid, &error);
    return result(rc, &error, "item_get_identifier");
}
int g1_count(void *folder, int messages, int *count) {
    libpff_error_t *error = NULL;
    int rc = messages ? libpff_folder_get_number_of_sub_messages(folder, count, &error)
                      : libpff_folder_get_number_of_sub_folders(folder, count, &error);
    return result(rc, &error, messages ? "number_of_sub_messages" : "number_of_sub_folders");
}
int g1_child(void *folder, int messages, int index, void **out) {
    libpff_item_t *item = NULL;
    libpff_error_t *error = NULL;
    int rc = messages ? libpff_folder_get_sub_message(folder, index, &item, &error)
                      : libpff_folder_get_sub_folder(folder, index, &item, &error);
    *out = item;
    return result(rc, &error, messages ? "get_sub_message" : "get_sub_folder");
}
int g1_property(void *item, uint32_t tag, unsigned char *data, size_t capacity, size_t *size) {
    libpff_record_set_t *set = NULL;
    libpff_record_entry_t *entry = NULL;
    libpff_error_t *error = NULL;
    int count = 0, rc;
    rc = libpff_item_get_number_of_record_sets(item, &count, &error);
    if (rc != 1) return result(rc, &error, "number_of_record_sets");
    if (count != 1) {
        snprintf(last_error, sizeof(last_error), "property context requires exactly 1 record set; got %d", count);
        return -1;
    }
    rc = libpff_item_get_record_set_by_index(item, 0, &set, &error);
    if (rc != 1) return result(rc, &error, "get_record_set_by_index");
    /* Header review MUST confirm order/types of these arguments, including flags.
     * tag 0x0FF90102 => entry_type 0x0FF9, value_type 0x0102; never full tag as entry_type.
     */
    rc = libpff_record_set_get_entry_by_type(set, tag >> 16, tag & 0xFFFF, &entry, 0, &error);
    if (rc != 1) {
        result(rc, &error, "get_entry_by_type");
        libpff_record_set_free(&set, NULL);
        return rc;
    }
    rc = libpff_record_entry_get_data_size(entry, size, &error);
    if (rc == 1 && data != NULL) {
        if (*size > capacity) {
            snprintf(last_error, sizeof(last_error), "property size exceeds buffer");
            libpff_record_entry_free(&entry, NULL);
            libpff_record_set_free(&set, NULL);
            return -1;
        }
        rc = libpff_record_entry_get_data(entry, data, capacity, &error);
    }
    result(rc, &error, "record_entry_get_data[_size]");
    /* Ownership and free semantics are a mandatory API review item. */
    if (libpff_record_entry_free(&entry, &error) != 1) return result(-1, &error, "record_entry_free");
    if (libpff_record_set_free(&set, &error) != 1) return result(-1, &error, "record_set_free");
    return rc;
}
