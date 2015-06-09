#ifndef COMMON_H
#define COMMON_H

#include <assert.h>

#define NFS_NAME_LEN 255
enum FTYPE{
	FUNKNOWN, FREG, FDIR,
	FCHARDEV, FBLOCKDEV, FPIPE,
	FSOCKET, FLINK
};

#define ROUNDUP(n, v) ((n) - 1 + (v) - ((n) - 1) % (v))
				// (n)-1是为了保证n%v==0时正常工作, n>0
#endif
