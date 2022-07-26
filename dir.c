#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include "dir.h"
#include "ascii.h"
#include "xmalloc.h"

void
print_file_info(FILE *f, File *file)
{
    // TODO: add other fields
    fprintf(f, "(File) {\n");
    fprintf(f, "  .name = %s,\n", file->name);
    fprintf(f, "  .mode = %o,\n", file->mode);
    fprintf(f, "  .size = %ld,\n", file->size);
    fprintf(f, "  .last_mod = %ld,\n", file->last_mod);
    fprintf(f, "}\n");
}

// Returns copy of path, but consecutive slashes removed
char*
cleanup_path(char *path)
{
    size_t l = strlen(path);
    char *ret = xmalloc(l + 1);
    char *rp = ret;
    for (size_t i = 0; i < l + 1; i++) {
        // Don't copy '/' if previous char was also '/'
        if (i > 0 && path[i-1] == '/' && path[i] == '/')
            continue;
        *(rp++) = path[i];
    }

    return ret;
}

// Return a newly allocated string of p2 appended to p1.
// Squeeze consecutive slashes together
// Return NULL on error.
char*
resolve_path(char *p1, char *p2)
{
    if (!strcmp(".", p2) || !strcmp("/", p2) ||
        !strcmp("./", p2) || !strcmp("/.", p2) ||
        !strcmp("./.", p2))
        return xstrdup(p1);

    // Allocate enough size
    size_t l1 = strlen(p1);
    size_t l2 = strlen(p2);
    char *res = xmalloc(l1 + l2 + 1);

    // Copy p1 onto res (excluding null term)
    char *rp = res;
    for (size_t i = 0; i < l1; i++) {
        // Don't copy '/' if previous char was also '/'
        if (i > 0 && p1[i-1] == '/' && p1[i] == '/')
            continue;
        *(rp++) = p1[i];
    }

    rp = res + l1 - 1;

    // Avoid double '/' in between p1 and p2
    if (*rp == '/') while (*p2 == '/') p2++;

    // Add '/' if not present between p1 and p2
    if (*rp != '/' && *p2 != '/')
        *(++rp) = '/';

    // Make rp stand on char after '/'
    rp++;

    // Copy p2 onto res (including null term)
    size_t l2_trimmed = strlen(p2);
    for (size_t i = 0; i < l2_trimmed + 1; i++) {
        // Don't copy '/' if previous char was also '/'
        if (i > 0 && p2[i] == '/' && p2[i-1] == '/')
            continue;
        *(rp++) = p2[i];
    }

    return res;
}

char*
get_human_file_perms(File *f)
{
    char *str = xmalloc(10);
    if (!str) return NULL;

    memset(str, '-', 9);

    // Link / dir bit
    if (f->is_link && f->is_dir) {
        str[0] = 'l';
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
    char *str = xmalloc(LEN);

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
    static char *dirlink = "/ ->";
    static char *reg = "";

    if (f->is_dir && f->is_link)
        return dirlink;
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
    // First '.', then '..'
    if (!strcmp(a, "."))
        return 1;
    if (!strcmp(b, "."))
        return -1;
    if (!strcmp(a, ".."))
        return 1;
    if (!strcmp(b, ".."))
        return -1;

    // Sort alphabetically, but prioritize names starting with
    // a non alpha-numeric character (tilde, dot, comma...).
    // But prioritize those starting with a dot most.
    if (a[0] == '.' && b[0] == '.')
        return -strcoll(a, b);
    else if (a[0] == '.')
        return 1;
    else if (b[0] == '.')
        return -1;

    if (!is_alnum(a[0]) && !is_alnum(b[0]))
        return -strcoll(a, b);
    else if (!is_alnum(a[0]))
        return 1;
    else if (!is_alnum(b[0]))
        return -1;

    return -strcoll(a, b);
}

// Return -1 if first arg greater
// Return 1 if second arg greater
// Return 0 otherwise
int
compare_files(File *a, File *b)
{
    // First come directories, then other file types.
    if (a->is_dir && b->is_dir)
        return compare_file_names(a->name, b->name);
    else if (a->is_dir)
        return 1;
    else if (b->is_dir)
        return -1;

     return compare_file_names(a->name, b->name);
}

void
sort_file_list (File_List *fl)
{
    // Insertion sort, don't judge
    for (size_t i = 0; i < fl->len; i++) {
        size_t smallest = i;
        // Linear search to find smallest
        for (size_t j = i + 1; j < fl->len; j++) {
            if (compare_files(fl->files + smallest, fl->files + j) < 0)
                smallest = j;
        }
        swap_in_file_list(fl, smallest, i);
    }
}

void
print_stat_error(int err, char *path, int is_link)
{
    if (is_link) {
        fprintf(stderr, "Failed stat on file linked from \"%s\": ", path);
    } else {
        fprintf(stderr, "Failed stat on file \"%s\": ", path);
    }
    switch (err) {
    case EACCES:
        fprintf(stderr, "(EACCES) access denied.\n");
        break;
    case ELOOP:
        fprintf(stderr, "(ELOOP) too many links.\n");
        break;
    case ENAMETOOLONG:
        fprintf(stderr, "(ENAMETOOLONG) path too long.\n");
        break;
    case ENOENT:
        fprintf(stderr,
                "(ENOENT) component of path does not exist or "
                "is a dangling symbolic link.\n");
        break;
    case ENOMEM:
        fprintf(stderr,
                "(ENOMEM) out of memory.\n");
        break;
    case EOVERFLOW:
        fprintf(stderr,
                "(EOVERFLOW) stats can't fit into statbuf struct; "
                "may happen when running 32 bit program on a 64 "
                "bit machine.\n");
        break;
    case ENOTDIR:
        fprintf(stderr,
                "(ENOTDIR) A component of the path is not a "
                "directory.\n");
        break;
    default:
        fprintf(stderr, "unknown error\n");
        break;
    }
}

// Return xstrdup-ed base name from given path.
char*
get_base_name(char *path)
{
    size_t l = strlen(path);
    if (l == 1 && (*path == '/' || *path == '.')) {
        return xstrdup(path);
    }

    char *copy_from = path + l - 1;
    size_t copy_len = 1;

    // If path ends with '/'
    if (*copy_from == '/') {
        copy_from--;
    }

    while (copy_from != path) {
        if (*copy_from == '/') {
            copy_from++;
            copy_len--;
            break;
        }
        copy_from--;
        copy_len++;
    }

    return xstrndup(copy_from, copy_len);
}

// Read data from file at given path into File struct.
// Return -2 and write NULL_FILE into f on complete failure.
// Return -1 and write NULL_FILE into f if file doesn't exist.
// Return 0 on first lstat success and write link stats into f.
// Return 1 on complete success.
int
read_file_info(File *f, char *path, char *base_name)
{
    // Get lstat
    struct stat sb;
    int err = lstat(path, &sb);
    if (err) {
        int saved_errno = errno;
        print_stat_error(saved_errno, path, 0);
        *f = NULL_FILE;
        return saved_errno == ENOENT ? -1 : -2;
    }

    // Read data into *f
    char *name = xstrdup(base_name);
    *f = (File) {
        .name = name,
        .fd = -1,
        .mode = sb.st_mode,
        .size = sb.st_size,
        .last_mod = sb.st_mtime,
        .is_dir = S_ISDIR(sb.st_mode),
        .is_link = S_ISLNK(sb.st_mode),
        .is_broken_link = 0,
        .is_null = 0,
    };

    // If link, get data about the linked file
    if (f->is_link) {
        err = stat(path, &sb);
        if (err) {
            print_stat_error(errno, path, 1);
            f->size = 0;
            f->is_broken_link = (errno == ENOENT);
            return 0;
        }

        f->is_dir = S_ISDIR(sb.st_mode);
        f->size = sb.st_size;
        f->last_mod = sb.st_mtime;
    }

    //print_file_info(stdout, f);
    return 1;
}

File_List*
ls(char *path)
{
    char *dir;
    size_t dir_len;
    size_t path_len = strlen(path);

    // Copy path to dir and add trailing '/'
    if (path[path_len - 1] != '/') {
        dir = xmalloc(path_len + 1);
        strcpy(dir, path);
        dir_len = path_len + 1;
        dir[dir_len - 1] = '/';
        dir[dir_len] = '\0';
    } else {
        dir = xstrdup(path);
        dir_len = path_len;
    }

    // Get dirent list
    struct dirent **namelist;
    int n = scandir(dir, &namelist, NULL, NULL);
    if (n == -1) {
        int saved_errno = errno;
        fprintf(stderr, "Failed scandir() on \"%s\"\n", dir);
        errno = saved_errno;
        perror("scandir()");
        return NULL;
    }

    // Init file list
    File_List *file_list = xmalloc(sizeof(File_List));
    file_list->len = (size_t) n;
    file_list->files = xmalloc(sizeof(File) * file_list->len);
    file_list->dir_info = xmalloc(sizeof(File));

    // Get directory info
    char *dir_base_name = get_base_name(dir);
    read_file_info(file_list->dir_info, dir, dir_base_name);
    free(dir_base_name);

    // Write directory part of full_path
    char *full_path = xmalloc(sizeof(char) * dir_len + 256);
    //                   Maximum size of dirent.d_name ^
    memcpy(full_path, dir, dir_len);

    // Copy dirents into file list
    for (size_t i = 0; i < file_list->len; i++) {
        // Resolve full path
        char *file_name = namelist[i]->d_name;
        strncpy(full_path + dir_len, file_name, 256);

        read_file_info(file_list->files + i, full_path, file_name);
        free(namelist[i]);
    }
    free(namelist);
    free(full_path);

    sort_file_list(file_list);

    return file_list;
}

void
free_file_parts(File *f)
{
    if (f->is_null)
        return;
    free(f->name);
}

void
free_file(File *f)
{
    free_file_parts(f);
    free(f);
}

void
free_file_list(File_List *fl)
{
    for (size_t i = 0; i < fl->len; i++) {
        free_file_parts(fl->files + i);
    }
    free(fl->files);
    free(fl->dir_info);
    free(fl);
}
