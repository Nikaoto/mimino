#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include "dir.h"
#include "ascii.h"

void
print_file_info(FILE *f, File *file)
{
    fprintf(f, "(File) {\n");
    fprintf(f, "  .name = %s,\n", file->name);
    fprintf(f, "  .mode = %o,\n", file->mode);
    fprintf(f, "  .size = %ld,\n", file->size);
    fprintf(f, "}\n");
}

char*
get_human_file_perms(File *f)
{
    char *str = malloc(10);
    if (!str) return NULL;

    memset(str, '-', 9);

    // Link / dir bit
    if (f->is_link && f->is_dir) {
        str[0] = 'o';
    } else if (f->is_link) {
        str[0] = 'l';
    } else if (f->is_dir) {
        str[0] = 'd';
    }

    // User
    if (S_IRUSR & f->mode)
        str[1] = 'r';
    if (S_IWUSR & f->mode)
        str[2] = 'w';
    if (S_IXUSR & f->mode)
        str[3] = 'x';

    // Group
    if (S_IRGRP & f->mode)
        str[4] = 'r';
    if (S_IWGRP & f->mode)
        str[5] = 'w';
    if (S_IXGRP & f->mode)
        str[6] = 'x';

    // World
    if (S_IROTH & f->mode)
        str[7] = 'r';
    if (S_IWOTH & f->mode)
        str[8] = 'w';
    if (S_IXOTH & f->mode)
        str[9] = 'x';

    str[10] = '\0';

    return str;
}

char*
get_human_file_size(off_t size)
{
    #define LEN 22
    char *str = malloc(LEN);
    if (!str) return NULL;

    if (size < KB_SIZE) {
        snprintf(str, LEN, "%ld", size);
    } else if (size >= KB_SIZE && size < MB_SIZE) {
        snprintf(str, LEN, "%ldK", size / KB_SIZE);
    } else if (size >= MB_SIZE && size < GB_SIZE) {
        snprintf(str, LEN, "%ldM", size / MB_SIZE);
    } else if (size >= GB_SIZE && size < TB_SIZE) {
        snprintf(str, LEN, "%ldG", size / GB_SIZE);
    } else if (size >= MB_SIZE && size < GB_SIZE) {
        snprintf(str, LEN, "%ldT", size / TB_SIZE);
    }

    #undef LEN
    return str;
}

char*
get_file_type_suffix(File *f)
{
    static char *dir = "/";
    static char *link = " ->";
    static char *reg = " ";

    if (f->is_dir)
        return dir;
    if (f->is_link)
        return link;

    return reg;
}

void
swap_in_file_list(File_List *fl, size_t a, size_t b)
{
    if (a == b) return;
    File tmp_item = fl->files[a];
    fl->files[a] = fl->files[b];
    fl->files[b] = tmp_item;
}

// Return -1 if first arg greater
// Return 1 if second arg greater
// Return 0 otherwise
int
compare_file_names(char *a, char *b)
{
    // Sort alphabetically, but prioritize names starting with
    // a non alpha-numeric character (tilde, dot, comma...).
    // But prioritize those starting with a dot most.
    if (a[0] == '.' && b[0] == '.')
        return strcmp(a, b);
    else if (a[0] == '.')
        return -1;
    else if (b[0] == '.')
        return 1;

    if (!is_alnum(a[0]) && !is_alnum(b[0]))
        return strcmp(a, b);
    else if (!is_alnum(a[0]))
        return -1;
    else if (!is_alnum(b[0]))
        return 1;

    return strcmp(a, b);
}

// Return -1 if first arg greater
// Return 1 if second arg greater
// Return 0 otherwise
int
compare_files(File *a, File *b)
{
    // First come directories, then other file types.
    if (S_ISDIR(a->mode) && S_ISDIR(b->mode))
        return compare_file_names(a->name, b->name);
    else if (S_ISDIR(a->mode))
        return -1;
    else if (S_ISDIR(b->mode))
        return 1;

     return compare_file_names(a->name, b->name);
}

void
sort_file_list (File_List *fl)
{
    // Find '.' and '..'
    int curr_dir_found = 0;
    int prev_dir_found = 0;
    size_t curr_dir, prev_dir;
    for (size_t i = 0; i < fl->len; i++) {
        if (!strcmp(".", fl->files[i].name)) {
            curr_dir_found = 1;
            curr_dir = i;
        } else if (!strcmp("..", fl->files[i].name)) {
            prev_dir_found = 1;
            prev_dir = i;
        }
    }

    // Put '.' first
    if (curr_dir_found) {
        swap_in_file_list(fl, 0, curr_dir);
    }

    // Put '..' after '.' or first if it wasn't found
    if (prev_dir_found) {
        swap_in_file_list(fl, curr_dir_found, prev_dir);
    }

    // Do insertion sort for the rest of the array
    for (size_t i = curr_dir_found + prev_dir_found; i < fl->len; i++) {
        size_t smallest = i;
        // Linear search to find smallest
        for (size_t j = smallest + 1; j < fl->len; j++) {
            if (compare_files(fl->files + smallest, fl->files + j) == 1)
                smallest = j;
        }
        swap_in_file_list(fl, smallest, i);
    }
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

    // Copy dirents into file list
    for (size_t i = 0; i < file_list->len; i++) {
        // Resolve full path
        char *file_name = namelist[i]->d_name;
        strncpy(full_path + dir_len, file_name, 256);

        // Get stats
        struct stat sb;
        err = lstat(full_path, &sb);
        if (err) {
            //saved_errno = errno;
            perror("lstat()");
            fprintf(stderr, "Failed on file \"%s\"\n", full_path);
            // TODO: handle the error in saved_errno
        }

        // Save
        file_list->files[i] = (File) {
            .name = strndup(file_name, 256),
            .mode = sb.st_mode,
            .size = sb.st_size,
            .is_dir = S_ISDIR(sb.st_mode),
            .is_link = S_ISLNK(sb.st_mode),
        };

        //print_file_info(stdout, &(file_list->files[i]));
        free(namelist[i]);
    }
    free(namelist);
    free(full_path);

    sort_file_list(file_list);

    return file_list;
}
