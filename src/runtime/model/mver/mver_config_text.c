#include "mver_config_text.h"
#include "bongo_cat/file.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *mver_text_read(const char *path, size_t *length) {
    *length = 0;
    FILE *file = bongo_cat_file_open(path, "rb");
    if (!file) return NULL;
    long size = -1;
    if (fseek(file, 0, SEEK_END) == 0) size = ftell(file);
    char *text = NULL;
    if (size >= 0 && size <= MVER_CONFIG_MAX_BYTES && fseek(file, 0, SEEK_SET) == 0) {
        text = malloc((size_t)size + 1);
        if (text && fread(text, 1, (size_t)size, file) != (size_t)size) {
            free(text); text = NULL;
        }
    }
    fclose(file);
    if (text) { *length = (size_t)size; text[*length] = '\0'; }
    return text;
}
