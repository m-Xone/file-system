#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

typedef struct superblock_t superblock_t;
typedef struct directory_t directory_t;
typedef struct inode_bmp_t inode_bmp_t;
typedef struct data_bmp_t data_bmp_t;
typedef struct fmeta_t fmeta_t;
typedef struct map_t map_t;
typedef struct oft_t oft_t;

int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int dismount_fs(char *disk_name);
int fs_create(char *name);
int fs_open(char *name);
int fs_close(int fildes);
int fs_delete(char *name);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

