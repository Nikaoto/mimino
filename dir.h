#ifndef _MIMINO_DIR_H
#define _MIMINO_DIR_H

#include <sys/types.h>

#define TB_SIZE 0xE8D4A51000
#define GB_SIZE 0x3B9ACA00
#define MB_SIZE 0xF4240
#define KB_SIZE 0x3E8

typedef struct {
    char *name;
    mode_t mode;
    off_t size;
    int is_dir;
    int is_link;
} File;

typedef struct {
    File *files;
    size_t len;
} File_List;

File_List* ls(char *dir);

char* get_file_type_suffix(File *f);
char* get_human_file_size(off_t size);

#endif // _MIMINO_DIR_H

/*
  struct dirent {
     ino_t          d_ino;       // Inode number.
     off_t          d_off;       // Not an offset; see below.
     unsigned short d_reclen;    // Length of this record.
     unsigned char  d_type;      // Type of file; not supported
                                 // by all filesystem types.
     char           d_name[256]; // Null-terminated filename.
  };

  struct stat {
      dev_t     st_dev;         // ID of device containing file
      ino_t     st_ino;         // Inode number
      mode_t    st_mode;        // File type and mode
      nlink_t   st_nlink;       // Number of hard links
      uid_t     st_uid;         // User ID of owner
      gid_t     st_gid;         // Group ID of owner
      dev_t     st_rdev;        // Device ID (if special file)
      off_t     st_size;        // Total size, in bytes
      blksize_t st_blksize;     // Block size for filesystem I/O
      blkcnt_t  st_blocks;      // Number of 512B blocks allocated
      struct timespec st_atim;  // Time of last access
      struct timespec st_mtim;  // Time of last modification
      struct timespec st_ctim;  // Time of last status change
  };
*/
