/*
 * UFS file system format
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "fs.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#define ROUNDUP(n, v) ((n) - 1 + (v) - ((n) - 1) % (v))
				// (n)-1是为了保证n%v==0时正常工作, n>0
#define MAX_DIR_ENTS 128

u32 nblocks, ninodes;
char *diskmap, *diskpos;
struct ufs_super_block *super;
u32 *bitmap,*inodebitmap, *inodemap;

void
panic(const char*fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	abort();
}

u32 blockof(void *pos){
	return ((char *)pos - diskmap) / BLKSIZE;
}

void*
alloc(u32 bytes){
	void *start = diskpos;
	diskpos += ROUNDUP(bytes, BLKSIZE);
	if(blockof(diskpos) >= nblocks)
		panic("out of disk blocks");
	return start;

}

void
opendisk(const char *name){
	int r, diskfd, nbitblocks, nbitinodes;

	if((diskfd = open(name, O_RDWR | O_CREAT, 0666)) < 0)
		panic("open %s: %s", name, strerror(errno));
	if((r = ftruncate(diskfd, 0)) < 0
	|| (r = ftruncate(diskfd, nblocks*BLKSIZE))<0)
		panic("truncate %s: %s", name, strerror(errno));
	if((diskmap=mmap(NULL, nblocks*BLKSIZE, PROT_READ|PROT_WRITE,
				MAP_SHARED, diskfd, 0)) == MAP_FAILED)
		panic("mmap: %s: %s", name, strerror(errno));
	close(diskfd);

	diskpos = diskmap;
	alloc(BLKSIZE);	// 引导块
	super = alloc(BLKSIZE);
	super->s_magic = FS_MAGIC;
	super->s_nblocks = nblocks;
	super->s_root.file_type = FDIR;
	strcpy(super->s_root.name, "/");
	super->s_root.name_len = 4;
	super->s_root.inode = 0;
	super->s_root.rec_len=REC_LEN(super->s_root);
	super->s_inodes_count = ninodes;
	super->s_inode_size = INODESIZE;
	super->s_log_block_size = BLKSIZE;

	nbitblocks = (nblocks + BLKBITSIZE - 1) / BLKBITSIZE;	
	bitmap = alloc(nbitblocks * BLKSIZE);				// 数据块位图
	memset(bitmap, 0xFF, nbitblocks * BLKSIZE);
	super->s_block_bitmap = blockof(bitmap);

	nbitinodes = (ninodes + BLKBITSIZE - 1) / BLKBITSIZE;
	inodebitmap = alloc(nbitinodes * BLKSIZE);				// 索引结点位图
	memset(inodebitmap, 0xFF, nbitinodes * BLKSIZE);
	super->s_inode_bitmap = blockof(inodebitmap);

	inodemap = alloc(ninodes * INODESIZE);				// 索引结点表
	memset(inodemap, 0, ninodes * INODESIZE);
	super->s_inode_table = blockof(inodemap);
}

void
usage(void){
	fprintf(stderr, "Usage: fsformat fs.img NBLOCKS NINODES\n");
	exit(2);
}

void
finishdisk(){
	int r, i;
	for(i=0;i<blockof(diskpos);i++)
		bitmap[i/32] &= ~(1<<(i%32));

	inodebitmap[0] &= ~1;

	if((r = msync(diskmap, nblocks * BLKSIZE, MS_SYNC)) < 0)
		panic("msync: %s", strerror(errno));
}

int
main(int argc, char **argv){
	int i;
	char *s;
	
	if(argc < 4)
		usage();
	nblocks = strtol(argv[2], &s, 0);
	if(*s || s == argv[2] || nblocks < 2)
		usage();
	ninodes = strtol(argv[3], &s, 0);
	if(*s || s == argv[3] || ninodes < 2)
		usage();

	ninodes = ROUNDUP(ninodes, BLKSIZE/INODESIZE);	
	opendisk(argv[1]);

	printf("数据块位图块号=%d\n", super->s_block_bitmap);
	printf("索引节点表块号=%d\n", super->s_inode_table);
	printf("索引节点位图块号=%d\n", super->s_inode_bitmap);
	printf("待分配起始块号=%d\n", blockof(diskpos));

	finishdisk();
}
