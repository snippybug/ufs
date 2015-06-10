// 文件读写接口

#ifndef FILE_H
#define FILE_H

#include "fsinfo.h"

struct ufs_entry_info* readdir(char *dir);
int ufs_create(char *path, int type);
int ufs_cd(char *path);
int initfs();

#endif
