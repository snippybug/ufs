#include "fsinfo.h"
#include "fs.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct ufs_super_block sb;

#define DISK "fs.img"

// 如果byte的第bit位是1,则返回1,否则返回0
int testbit(char byte, char bit){
	assert(bit < 8);
	assert(bit >= 0);
	return (byte & (1 << bit));
}

u32* readblk(int num){
	static char buf[BLKSIZE];
	int r;

	FILE *disk = fopen(DISK, "r");
	assert(disk);
	r = fseek(disk ,BLKSIZE*num, SEEK_SET);
	assert(r==0);
	r = fread(buf, BLKSIZE, 1, disk);
	assert(r==1);
	fclose(disk);
	return (u32 *)buf;
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

// 分配索引节点
int allocinode(){
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
	for(i=0;i<size/4;i++){
		if(inodebitmap[i] != 0){
			char *p = (char *)&inodebitmap[i];
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
	assert(i<size/4);
	r = fseek(disk, sb.s_inode_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(inodebitmap, size, 1, disk);
	assert(r==1);
	free(inodebitmap);

	size = ROUNDUP(sb.s_inode_size*sb.s_inodes_count, BLKSIZE);
	u32* inodetable = malloc(size);
	int index = i*32 + j*4 + k;			// 计算位置
	struct ufs_innode *table = (struct ufs_innode *)inodetable;
	r = fseek(disk, sb.s_inode_table*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fread(inodetable, size, 1, disk);
	assert(r==1);
	table[index].i_mode = 1;
	r = fseek(disk, sb.s_inode_table*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(inodetable, size, 1, disk);
	assert(r==1);
	fclose(disk);
	return index;
}

int allocblk(){
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
	for(i=0;i<size/4;i++){
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
	assert(i<size/4);
	r = fseek(disk, sb.s_block_bitmap*BLKSIZE, SEEK_SET);
	assert(r==0);
	r = fwrite(blkbitmap, size, 1, disk);
	assert(r==1);

	free(blkbitmap);
	fclose(disk);
	return i*32+j*4+k;
}

void readsuper(struct ufs_super_block *psb){
	int r;
	FILE *disk = fopen(DISK, "r");
	assert(disk);		// FIXME
	r=fseek(disk, BLKSIZE, SEEK_SET);
	assert(r==0);		// FIXME
	r = fread(psb, sizeof(struct ufs_super_block), 1, disk);
	assert(r == 1);		// FIXME
	assert(psb->s_magic == FS_MAGIC);
	fclose(disk);
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

// readdir获取的内存由调用者释放
// 1. 读取超级块，获得根目录inode
// 2. 根据根目录inode，遍历所有数据块
// FIXME: 只能处理根目录
struct ufs_entry_info* readdir(char *dir){
	readsuper(&sb);
	u32 num = sb.s_root.inode;
	struct ufs_innode rootinode;
	// 1.
	readinode(num, &rootinode);
	// 2.
	int n = rootinode.i_blocks;	// n表示目录项的数据块个数
	int i;
	struct ufs_entry_info *elist=NULL;
	i = 0;
	while(i<n){
		// 读取数据块
		struct ufs_dir_entry *entry;
		int over = 0;
		u32 *bptr = readblk(rootinode.i_block[i]);
		u32 *ptr = bptr;
		while((ptr - bptr) < BLKSIZE){
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
// FIXME: 没有检查文件的重名
int createfile(char *name){
	readsuper(&sb);
	u32 num = sb.s_root.inode;
	struct ufs_innode rootinode;
	readinode(num, &rootinode);
	int n = rootinode.i_blocks;
	int i=0;
	struct ufs_dir_entry *entry;
	u32 *bptr;
	while(i<n){
		int found = 0;
		bptr = readblk(rootinode.i_block[i]);
		u32 *ptr = bptr;
		while((ptr - bptr) < BLKSIZE/4){
			entry = (struct ufs_dir_entry *)ptr;
			if(entry->rec_len == 0){
				// FIXME: 没有检查越界
				found = 1;
				break;
			}
			ptr += (entry->rec_len) / 4;
		}
		if(found) break;
		i++;
	}
	assert(i < n || n < NFS_N_BLOCKS);
	if(i == n && n < NFS_N_BLOCKS){		// 需要分配新的数据块
		i = allocblk();
		rootinode.i_block[rootinode.i_blocks++] = i;
		writeinode(num, &rootinode);
		bptr = readblk(i);
		entry = (struct ufs_dir_entry *)bptr;
	}else{
		i = rootinode.i_block[i];
	}
	entry->file_type = FREG;
	entry->inode = allocinode();
	strcpy(entry->name, name);
	entry->name_len = strlen(name);
	entry->rec_len = REC_LEN(*entry);
	
	writeblk((char *)bptr, i);		// i保存inode结构的块号
	return 0;
}

// 除了文件类型，其余的和createfile一样(包括FIXME)
int createdir(char *name){
	readsuper(&sb);
	u32 num = sb.s_root.inode;
	struct ufs_innode rootinode;
	readinode(num, &rootinode);
	int n = rootinode.i_blocks;
	int i=0;
	struct ufs_dir_entry *entry;
	u32 *bptr;
	while(i<n){
		int found = 0;
		bptr = readblk(rootinode.i_block[i]);
		u32 *ptr = bptr;
		while((ptr - bptr) < BLKSIZE/4){
			entry = (struct ufs_dir_entry *)ptr;
			if(entry->rec_len == 0){
				// FIXME: 没有检查越界
				found = 1;
				break;
			}
			ptr += (entry->rec_len) / 4;
		}
		if(found) break;
		i++;
	}
	assert(i < n || n < NFS_N_BLOCKS);
	if(i == n && n < NFS_N_BLOCKS){		// 需要分配新的数据块
		i = allocblk();
		rootinode.i_block[rootinode.i_blocks++] = i;
		writeinode(num, &rootinode);
		bptr = readblk(i);
		entry = (struct ufs_dir_entry *)bptr;
	}else{
		i = rootinode.i_block[i];
	}
	entry->file_type = FDIR;
	entry->inode = allocinode();
	strcpy(entry->name, name);
	entry->name_len = strlen(name);
	entry->rec_len = REC_LEN(*entry);
	
	writeblk((char *)bptr, i);		// i保存inode结构的块号
	return 0;
}
