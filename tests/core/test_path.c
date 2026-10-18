#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "test.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>

static void check_contents(const char *path) {
    char text[8] = {0};
    FILE *file = bongo_cat_file_open(path, "rb");
    CHECK(file != NULL);
    if (!file) return;
    CHECK(fread(text, 1, sizeof(text), file) == 4);
    CHECK(strcmp(text, "core") == 0);
    CHECK(fclose(file) == 0);
}
#endif

void test_path(void) {
#ifndef _WIN32
    char directory[] = "/tmp/bongocat-copy-XXXXXX";
    if (!mkdtemp(directory)) { CHECK(0); return; }
    char source[BONGO_CAT_PATH_CAP], alias[BONGO_CAT_PATH_CAP];
    char copy[BONGO_CAT_PATH_CAP], spelling[BONGO_CAT_PATH_CAP];
    CHECK(bongo_cat_path_join(source, sizeof(source), directory, "source"));
    CHECK(bongo_cat_path_join(alias, sizeof(alias), directory, "alias"));
    CHECK(bongo_cat_path_join(copy, sizeof(copy), directory, "copy"));
    CHECK(bongo_cat_path_join(spelling, sizeof(spelling), directory, "./source"));
    FILE *file = bongo_cat_file_open(source, "wb");
    CHECK(file != NULL);
    if (!file) { CHECK(rmdir(directory) == 0); return; }
    CHECK(fputs("core", file) >= 0);
    CHECK(fclose(file) == 0);
    CHECK(!bongo_cat_path_copy_file(source, source));
    CHECK(!bongo_cat_path_copy_file(source, spelling));
    CHECK(link(source, alias) == 0);
    CHECK(!bongo_cat_path_copy_file(source, alias));
    check_contents(source);
    CHECK(unlink(alias) == 0);
    CHECK(symlink(source, alias) == 0);
    CHECK(!bongo_cat_path_copy_file(source, alias));
    check_contents(source);
    CHECK(bongo_cat_path_copy_file(source, copy));
    check_contents(copy);
    CHECK(unlink(alias) == 0);
    CHECK(unlink(copy) == 0);
    CHECK(unlink(source) == 0);
    CHECK(rmdir(directory) == 0);
#endif
}
