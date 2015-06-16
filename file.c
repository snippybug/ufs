#include "fsinfo.h"
#include "fs.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct ufs_super_block sb;	// 超级块
struct ufs_dir_entry wd;		// 当前工作目录

#define DISK "fs.img"
#define MAX_PATH_LEN 256 
// 如果byte的第bit位是1,则返回1,否则返回0
int testbit(char byte, char bit){
	assert(bit < 8);
	assert(bit >= 0);
	return (byte & (1 << bit));
}

char* readblk(int num){
	static char buf[BLKSIZE];
	int r;

	FILE *disk = fopen(DISK, "r");
	assert(disk);
	r = fseek(disk ,BLKSIZE*num, SEEK_SET);
	assert(r==0);
	r = fread(buf, BLKSIZE, 1, disk);
	assert(r==1);
	fclose(disk);
	return buf;
}

int writeblk(char *buf, int num){
	assert(buf);
	int r;
	FILE *disk = fopen(DISK, "r+");
	assert(disk);
	r = fseek(disk, BLKSIZE*num, SEEK_SET);
	assert(r==0);
	r = fwrite(buf, BLKSIZE, 1, disk);
	assert(r==1);
	fclose(disk);
	return 0;
}

int freeblk(int num){
	int r;
	FILE *disk = fopen(DISK, "r+");
	assert(disk);
	int size = ROUNDUP(sb.s_nblocks/8, BLKSIZE);
	u32 *blkbitmap = malloc(size);
	r = fseek(disk, sb.s_block_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fread(blkbitmap, size, 1, disk);
	assert(r==1);
	assert((blkbitmap[num/32] & (1<<(num%32))) == 0);		// 0表示已分配
	blkbitmap[num/32] |= 1<<(num%32);

	r = fseek(disk, sb.s_block_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(blkbitmap, size, 1, disk);
	assert(r==1);

	free(blkbitmap);
	fclose(disk);
	return 0;
}

int freeinode(int num){
	int r;
	FILE *disk = fopen(DISK, "r+");
	assert(disk);
	int size = ROUNDUP(sb.s_inodes_count/8, BLKSIZE);
	u32 *inodebitmap = malloc(size);
	assert(inodebitmap);
	r = fseek(disk, sb.s_inode_bitmap*BLKSIZE, SEEK_SET);
	assert(r == 0);
	r = fread(inodebitmap, size, 1, disk);
	assert(r == 1);
	assert((inodebitmap[num/32] & (1 << (num%32))) == 0);
	inodebitmap[num/32] |= 1 << (num%32);

	r = fseek(disk, sb.s_inode_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(inodebitmap, size, 1, disk);
	assert(r==1);
	free(inodebitmap);
	fclose(disk);
	return 0;
}

// 分配索引节点
int 
allocinode(){
	int r;
	FILE *disk = fopen(DISK, "r+");
	assert(disk);		// FIXME
	int size = ROUNDUP(sb.s_inodes_count/8, BLKSIZE);
	u32 *inodebitmap = malloc(size);
	assert(inodebitmap);	// FIXME
	r = fseek(disk, sb.s_inode_bitmap*BLKSIZE, SEEK_SET);
	assert(r == 0);
	r = fread(inodebitmap, size, 1, disk);
	assert(r == 1);
	// 寻找空闲位
	int i,j,k;
	fprintf(stderr, "allocinode: size=%d\n", size);
	for(i=0;i<sb.s_inodes_count/32;i++){
		if(inodebitmap[i] != 0){
			char *p = (char *)&inodebitmap[i];
			for(j=0;j<4;j++){
				if(p[j] != 0){
					for(k=0;k<8;k++){
						if(testbit(p[j], k)){
							fprintf(stderr,"allocinode, i=%d,j=%d,k=%d\n", i,j,k);
							p[j] &= ~(1<<k);
							goto out;
						}
					}
				}
			}
		}
	}
out:
	assert(i<sb.s_inodes_count/32);
	r = fseek(disk, sb.s_inode_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(inodebitmap, size, 1, disk);
	assert(r==1);
	free(inodebitmap);

	size = ROUNDUP(sb.s_inode_size*sb.s_inodes_count, BLKSIZE);
	u32* inodetable = malloc(size);
	int index = i*32 + j*8 + k;			// 计算位置
	struct ufs_innode *table = (struct ufs_innode *)inodetable;
	r = fseek(disk, sb.s_inode_table*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fread(inodetable, size, 1, disk);
	assert(r==1);
	memset(&table[index], 0, sizeof(struct ufs_innode));	// 防止文件信息错误
	r = fseek(disk, sb.s_inode_table*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(inodetable, size, 1, disk);
	assert(r==1);

	free(inodetable);
	r = fclose(disk);
	assert(r == 0);
	return index;
}

int 
allocblk(){
	int r;
	FILE *disk = fopen(DISK, "r+");
	assert(disk);
	int size = ROUNDUP(sb.s_nblocks/8, BLKSIZE);
	u32 *blkbitmap = malloc(size);
	r = fseek(disk, sb.s_block_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fread(blkbitmap, size, 1, disk);
	assert(r==1);
	int i,j,k;
	for(i=0;i<sb.s_nblocks/32;i++){
		if(blkbitmap[i] != 0){
			char *p = (char *)&blkbitmap[i];
			for(j=0;j<4;j++){
				if(p[j] != 0){
					for(k=0;k<8;k++){
						if(testbit(p[j], k)){
							p[j] &= ~(1<<k);
							goto out;
						}
					}
				}
			}
		}
	}
out:
	assert(i<sb.s_nblocks/32);
	r = fseek(disk, sb.s_block_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(blkbitmap, size, 1, disk);
	assert(r==1);

	free(blkbitmap);
	fclose(disk);
	fprintf(stderr, "allocblk: i=%d, j=%d, k=%d, ret=%d\n", i, j, k, i*32+j*8+k);
	return i*32+j*8+k;
}


int readsuper(struct ufs_super_block *psb){
	int r;
	FILE *disk = fopen(DISK, "r");
	if(disk==NULL){
		perror("readsuper: fopen fails ");
		return -1;
	}
	r=fseek(disk, BLKSIZE, SEEK_SET);
	if(r != 0){
		perror("readsuper: fseek fails ");
		return -1;
	}
	r = fread(psb, sizeof(struct ufs_super_block), 1, disk);
	if(r != 1){
		perror("readsuper: fread fails ");
		return -1;
	}
	if(psb->s_magic != FS_MAGIC){
		fprintf(stderr, "readsuper: Unknown filesystem type, expected %x\n", FS_MAGIC);
		return -1;
	}
	fclose(disk);
	return 0;
}

// 根据索引节点号，读取索引节点结构
// 1. 根据超级块信息，找出索引节点位图和第一块索引节点表块的块号
// 2. 用块号计算偏移量，从磁盘中读出索引节点位图
// 3. 如果位图显示未分配，错误
// 4. 否则，计算索引节点结构的位置，将其读入内存并返回

// 索引节点号 ---> 所在位图的块号 ---> 位图中的字节 ---> 位
int readinode(int num, struct ufs_innode *pinode){
	int r;
	FILE *disk = fopen(DISK, "r");
	assert(disk);		// FIXME
	u32 *inodebitmap = malloc(BLKSIZE);		// 只读取一块
	assert(inodebitmap);				// FIXME
	r = fseek(disk, BLKSIZE*(sb.s_inode_bitmap+num/(BLKSIZE*8)), SEEK_SET);
	assert(r==0);
	r = fread(inodebitmap, BLKSIZE, 1, disk);
	assert(r==1);		// FIXME

	// 3
	num = num % (BLKSIZE*8);
	assert(!testbit(inodebitmap[num/8], num%8)); 	// FIXME

	// 4
	int inodesblk = BLKSIZE / sb.s_inode_size;
	r = fseek(disk, BLKSIZE*(sb.s_inode_table+num/inodesblk) + (num % inodesblk) * sizeof(struct ufs_innode), SEEK_SET);	// 块偏移 + 块内偏移量
	assert(r==0);		// FIXME
	r = fread(pinode, sizeof(struct ufs_innode), 1, disk);
	assert(r==1);

	free(inodebitmap);
	fclose(disk);
	return 0;
}

int writeinode(int num, struct ufs_innode *pi){
	int r;
	FILE *disk = fopen(DISK, "r+");
	assert(disk);			// FIXME
	int inodesblk = BLKSIZE / sb.s_inode_size;
	r = fseek(disk, BLKSIZE*(sb.s_inode_table + num / inodesblk) + (num % inodesblk)*sizeof(struct ufs_innode), SEEK_SET);
	assert(r == 0);
	r = fwrite(pi, sizeof(struct ufs_innode), 1, disk);
	assert(r==1);

	fclose(disk);
	return 0;
}

// 返回路径的最后一个目录项
int 
walkdir(char *path, struct ufs_dir_entry *pe){
	assert(path);
	assert(pe);

	struct ufs_dir_entry dir;
	if(path[0] == '/')
		dir = sb.s_root;	// 绝对路径从根目录开始
	else
		dir = wd;		// 相对路径从工作目录开始

	char *name = strtok(path, "/");
	int ret = 0;
	struct ufs_innode inode;
	while(name != NULL){
		fprintf(stderr,"walkdir: name=%s\n", name);
		// 寻找下一级目录
		u32 num = dir.inode;
		readinode(num, &inode);
		int n = inode.i_blocks;
		int i=0;
		int found=0;
		fprintf(stderr, "walkdir: n=%d\n", n);
		while(i<n){
			struct ufs_dir_entry *entry;
			int over = 0;
			fprintf(stderr, "walkdir: readblk %d\n", inode.i_block[i]);
			u32 *bptr = (u32 *)readblk(inode.i_block[i]);
			u32 *ptr = bptr;
			while((ptr - bptr) < BLKSIZE/4){
				entry = (struct ufs_dir_entry *)ptr;
				if(entry->rec_len != 0){
					fprintf(stderr, "walkdir: found an entry, name=%s, rec_len=%d\n", entry->name, entry->rec_len);
					if(strcmp(entry->name, name) == 0){
						dir = *entry;
						over = 1;
						found = 1;
						break;
					}else{
						ptr += entry->rec_len / 4;
					}
				}else{
					fprintf(stderr, "walkdir: reach end because entry->rec_len == 0\n");
					over = 1;
					break;
				}
			}
			if(over) break;
			i++;
		}
		if(!found){
			fprintf(stderr, "walkdir: Not found!\n");
			ret = -1;
			goto out;
		}
		name = strtok(NULL, "/");
	}
out:
	*pe = dir;
	return ret;
}

// readdir获取的内存由调用者释放
// 1. 读取超级块，获得根目录inode
// 2. 根据根目录inode，遍历所有数据块
struct ufs_entry_info* 
readdir(char *path){

	struct ufs_dir_entry dir;
	int r;
	if(path == NULL){
		dir = wd;
	}else{
		r = walkdir(path, &dir);
		assert(r==0);
	}

	assert(dir.file_type == FDIR);

	u32 num = dir.inode;
	struct ufs_innode inode;
	// 1.
	readinode(num, &inode);
	// 2.
	int n = inode.i_blocks;	// n表示目录项的数据块个数
	int i;
	struct ufs_entry_info *elist=NULL;
	i = 0;
	while(i<n){
		// 读取数据块
		struct ufs_dir_entry *entry;
		int over = 0;
		u32 *bptr = (u32 *)readblk(inode.i_block[i]);
		u32 *ptr = bptr;
		while((ptr - bptr) < BLKSIZE/4){
			entry = (struct ufs_dir_entry *)ptr;
			if(entry->rec_len != 0){
				struct ufs_entry_info *ut = (struct ufs_entry_info *)malloc(sizeof(struct ufs_entry_info));
				ut->file_type = entry->file_type;
				strcpy(ut->name, entry->name);
				ut->next = elist;
				elist = ut;
				ptr += (entry->rec_len) / 4;
			}else{
				over = 1;
				break;
			}
		}
		if(over) break;
		i++;
	}
	return elist;
}

// 思路：
// 1. 读入超级块，找出根目录
// 2. 如果根目录的数据块还有位置，分配结构与inode
// 3. 否则报错
int 
ufs_create(char *path, int type){
	assert(type == FREG || type == FDIR);
	int r;
	struct ufs_dir_entry dir;

	char tpath[MAX_PATH_LEN];
	strncpy(tpath, path, MAX_PATH_LEN);		// walkdir会改变path
	r = walkdir(path, &dir);
	assert(r==-1);			// 一定会失败
	if(tpath[strlen(tpath)-1] == '/')
		tpath[strlen(tpath)-1] = 0;
	char *name = strrchr(tpath, '/');
	if(name==NULL){			// 当输入当前目录的文件或目录时，可能不含/
		name = tpath;
	}else{
		name++;
	}
	fprintf(stderr, "ufs_create: file name=%s\n", name);

	u32 num = dir.inode;
	struct ufs_innode inode;
	readinode(num, &inode);
	int n = inode.i_blocks;
	int i=0;
	struct ufs_dir_entry *entry;
	u32 *bptr;
	while(i<n){
		int found = 0;
		fprintf(stderr, "ufs_create: readblk %d\n", inode.i_block[i]);
		bptr = (u32 *)readblk(inode.i_block[i]);
		u32 *ptr = bptr;
		while((ptr - bptr) < BLKSIZE/4){
			entry = (struct ufs_dir_entry *)ptr;
			if(entry->rec_len == 0){
				found = 1;	// 找到空位
				break;
			}
			ptr += (entry->rec_len) / 4;
		}
		if(found) break;
		i++;
	}
	assert(i < n || n < NFS_N_BLOCKS);
	if(i == n && n < NFS_N_BLOCKS){		// 需要分配新的数据块
		fprintf(stderr, "ufs_create: Need a new block, index=%d\n", i);
		i = allocblk();
		inode.i_block[inode.i_blocks++] = i;
		writeinode(num, &inode);
		bptr = (u32 *)readblk(i);
		entry = (struct ufs_dir_entry *)bptr;
	}else{
		fprintf(stderr, "ufs_create: Use an existed block, index=%d", i);
		i = inode.i_block[i];
		fprintf(stderr, ", num=%d\n", i);
	}
	entry->file_type = type;
	entry->inode = allocinode();
	entry->pinode = dir.inode;
	strcpy(entry->name, name);
	entry->name_len = strlen(name)+1;	// 保留串尾结束符
	entry->rec_len = REC_LEN(*entry);
	
	writeblk((char *)bptr, i);		// i保存inode结构的块号
	return 0;
}

// 读取超级块并缓存在内存中
// 初始化当前工作目录
int initfs(){
	int r;
	if((r = readsuper(&sb)) < 0)
		return r;
	wd = sb.s_root;
	return 0;
}

// 获取路径的目录结构，然后将其复制到wd
int ufs_cd(char *path){
	assert(path);

	int r;
	struct ufs_dir_entry dir;
	r = walkdir(path, &dir);
	assert(r==0);
	wd = dir;
	return 0;
}

// 删除索引号为num的目录中的项name
int rmentry(int num, char *name){
	struct ufs_innode inode;
	readinode(num, &inode);
	int i, n;
	n = inode.i_blocks;
	assert(n != 0);
	i=0;
	fprintf(stderr, "rmentry: inode %d, file=%s\n", num, name);
	while(i<n){
		// 读取数据块
		struct ufs_dir_entry *entry;
		int over = 0;
		u32 *bptr = (u32*)readblk(inode.i_block[i]);
		u32 *ptr = bptr;
		while((ptr - bptr) < BLKSIZE/4){
			entry = (struct ufs_dir_entry *)ptr;
			fprintf(stderr, "entry->name=%s\n", entry->name);
			if(strcmp(name, entry->name) == 0){
				// 当前块内，所有文件向前移动
				// entry保存着起始指针
				u32 *temp = ptr;
				u32 *temp2 = ptr + entry->rec_len / 4;
				struct ufs_dir_entry *dtemp = (struct ufs_dir_entry *)temp2;
				while((temp2 - bptr) < BLKSIZE / 4){
					int off = dtemp->rec_len;
					fprintf(stderr, "rmentry: off=%d\n", off);
					if(off != 0){
						memmove(temp, temp2, off);
					}else{
						break;
					}
					temp += off / 4;
					temp2 += off / 4;
					dtemp = (struct ufs_dir_entry *)temp2;
				}
				if(temp2 == (bptr + entry->rec_len / 4)){		// 该目录的最后一个文件
					fprintf(stderr, "rmentry: Notice, the last file is removed\n");
					freeblk(inode.i_block[i]);
					inode.i_block[i] = 0;
					inode.i_blocks--;
					writeinode(num, &inode);
				}else{
					// 清除多余的数据，并将数据块写回
					memset(temp, 0, (temp2-temp)*4);
					writeblk((char *)bptr, inode.i_block[i]);
				}
				over = 1;
				break;
			}else{
				ptr += entry->rec_len / 4;
			}
		}
		if(over) break;
		i++;
	}
}

// 删除步骤：
// 1. 如果有数据块，释放所有的数据块
// 2. 释放inode
// 3. 在父目录的数据块中删除记录项
// FIXME: 万一是根目录怎么办?
int ufs_rm(char *path, int type){
	assert(path);
	assert(type == FREG || type == FDIR);
	int r;
	struct ufs_dir_entry entry;
	r = walkdir(path, &entry);
	assert(r==0);
	assert(type == entry.file_type);
	struct ufs_innode inode;
	readinode(entry.inode, &inode);
	if(type == FDIR){
		assert(inode.i_blocks==0);		// 只允许删除空目录
	}else{
		if(inode.i_blocks != 0){
			int i;
			for(i=0;i<inode.i_blocks;i++){
				fprintf(stderr, "ufs_rm: free block %d\n", inode.i_block[i]);
				freeblk(inode.i_block[i]);
				inode.i_block[i] = 0;
			}
		}
	}
	freeinode(entry.inode);
	rmentry(entry.pinode, entry.name);
	return 0;
}

int ufs_read(char *path, int n, char *buf){
	assert(buf);
	int r;
	struct ufs_dir_entry entry;
	r = walkdir(path, &entry);
	assert(r==0);
	assert(entry.file_type == FREG);		// 只支持普通文件读写
	struct ufs_innode inode;
	readinode(entry.inode, &inode);

	buf[0] = 0;
	int count = (n < inode.i_size) ? n : inode.i_size;	// count代表实际读取的字节数
	fprintf(stderr, "ufs_read: count=%d\n", count);
	while(count){
		int i = 0;		// i代表当前的数据块号
		int t = 0;		// t是数据存储指针
		int temp = (count / BLKSIZE) ? BLKSIZE : count;
		
		memmove(&buf[t], readblk(inode.i_block[i]), temp);
		fprintf(stderr, "ufs_read: read from block %d\n", inode.i_block[i]);
		t += temp;
		count -= temp;
		i++;
	}
	return 0;
}

int 
ufs_write(char *path, int n, char *buf){
	assert(buf);
	fprintf(stderr, "ufs_write: n=%d\n", n);
	int r;
	struct ufs_dir_entry entry;
	r = walkdir(path, &entry);
	assert(r==0);
	assert(entry.file_type == FREG);
	struct ufs_innode inode;
	readinode(entry.inode, &inode);
	inode.i_size = n;

	while(n){
		int i = 0;
		int t = 0;
		int temp = (n / BLKSIZE) ? BLKSIZE : n;
		char *block;

		if(inode.i_block[i] == 0){		// 需要重新分配
			inode.i_block[i] = allocblk();
			inode.i_blocks++;
			fprintf(stderr, "ufs_write: new block %d\n", inode.i_block[i]);
		}
		fprintf(stderr, "ufs_write: temp=%d\n", temp);
		block = readblk(inode.i_block[i]);
		memmove(block, &buf[t], temp);
		writeblk(block, inode.i_block[i]);
		t += temp;
		n -= temp;
		i++;
	}

	writeinode(entry.inode, &inode);
	return 0;
}
