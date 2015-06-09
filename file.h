// 文件读写接口

#ifndef FILE_H
#define FILE_H

#include "fsinfo.h"

struct ufs_entry_info* readdir(char *dir);
int createfile(char *path);
int createdir(char *path);

#endif