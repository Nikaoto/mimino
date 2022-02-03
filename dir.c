#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include "dir.h"

void
print_file_info(FILE *f, File *file)
{
    fprintf(f, "(File) {\n");
    fprintf(f, "  .name = %s,\n", file->name);
    fprintf(f, "  .mode = %o,\n", file->mode);
    fprintf(f, "  .size = %ld,\n", file->size);
    fprintf(f, "}\n");
}

int
dirsort (const void *x, const void *y)
{
    File *a = *(File**)x;
    File *b = *(File**)y;

    print_file_info(stdout, a);
    print_file_info(stdout, b);

    if (!strcmp(a->name, "."))
        return -1;
    if (!strcmp(b->name, "."))
        return 1;
    if (!strcmp(a->name, ".."))
        return -1;
    if (!strcmp(b->name, ".."))
        return 1;

    if (S_ISDIR(a->mode) && S_ISDIR(b->mode))
        return strcmp(a->name, b->name);
    if (S_ISDIR(a->mode))
        return -1;
    if (S_ISDIR(b->mode))
        return 1;

    return strcmp(a->name, b->name);
}

File_List*
ls(char *dir)
{
    int err;
    //int saved_errno;

    // Get dirent list
    size_t dir_len = strlen(dir);
    struct dirent **namelist;
    int n = scandir(dir, &namelist, NULL, NULL);
    if (n == -1) {
        perror("scandir()");
        return NULL;
    }

    File_List *file_list = malloc(sizeof(File_List));
    file_list->len = (size_t) n;
    file_list->files = malloc(sizeof(File) * file_list->len);

    char *full_path = malloc(sizeof(char) * dir_len + 256);
    memcpy(full_path, dir, dir_len);
    full_path[dir_len] = '\0';

    fprintf(stdout, "Scanning dir \"%s\"\n", full_path);

    // Copy dirents into file list
    for (size_t i = 0; i < file_list->len; i++) {
        // Resolve full path
        char *file_name = namelist[i]->d_name;
        strncpy(full_path + dir_len, file_name, 256);
        printf("full_path: '%s'\n", full_path);

        // Get stats
        struct stat sb;
        err = stat(full_path, &sb);
        if (err) {
            //saved_errno = errno;
            perror("stat()");
            fprintf(stderr, "Failed on file \"%s\"\n", full_path);
            // TODO: handle the error in saved_errno
        }

        // Save
        file_list->files[i] = (File) {
            .name = strndup(file_name, 256),
            .mode = sb.st_mode,
            .size = sb.st_size,
        };

        //print_file_info(stdout, &(file_list->files[i]));
        free(namelist[i]);
    }
    free(namelist);
    free(full_path);

    // TODO: write own qsort, this one can't work with the struct
    /* qsort(&file_list->files, (size_t) file_list->len, */
    /*     sizeof(file_list->files), dirsort); */

    return file_list;
}
