// 文件系统内存数据数据结构

#ifndef FSINFO_H
#define FSINFO_H

#include "type.h"
#include "common.h"

struct ufs_entry_info{
	u8 file_type;		// 文件类型
	char name[NFS_NAME_LEN];	// 文件名
	struct ufs_entry_info *next;
};

struct ufs_sb_info{
	u32 s_nblocks;		// Total number of blocks on disk
	u32 s_inodes_count;	// 索引结点数
	u32 s_log_block_size;	// 块的大小
	u16 s_inode_size;	// 磁盘上索引节点结构的大小

	u32 s_block_bitmap;	// 块位图的块号
	u32 s_inode_bitmap;	// 索引节点位图的块号
	u32 s_inode_table;	// 第一个索引节点表块的块号
//	u32 s_block;		// 第一个数据块的块号
};

struct ufs_inode_info{
	u16 i_mode;		// 文件类型和访问权限
	u16 i_uid;			// 拥有者标识
	u32 i_size;		// 以字节为单位的文件长度
	u32 i_atime;		// 最后一次访问文件的时间
	u32 i_ctime;		// 索引节点最后改变的时间
	u32 i_mtime;		// 文件内容最后改变的时间
	u32 idtime;		// 文件删除的时间
	u16 i_gid;		// 用户的组标识符
	u16 i_links_count;	// 硬链接计数器
	u32 i_blocks;		// 文件的数据块数
	u16 i_flags;		// 文件标志
};

#endif
