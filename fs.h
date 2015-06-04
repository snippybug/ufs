#ifndef FS_H
#define FS_H

typedef unsigned int u32;
typedef short unsigned int u16;
typedef unsigned char u8;

#define NFS_N_BLOCKS 15
#define NFS_NAME_LEN 255

#define BLKSIZE 1024
#define BLKBITSIZE (BLKSIZE * 8)
#define INODESIZE 64
#define FS_MAGIC 0x756673

#define REC_LEN(entry) (sizeof(entry.inode)+sizeof(entry.rec_len)+sizeof(entry.name_len)+sizeof(entry.file_type)+entry.name_len)

enum FTYPE{
	FUNKNOWN, FREG, FDIR,
	FCHARDEV, FBLOCKDEV, FPIPE,
	FSOCKET, FLINK
};

struct ufs_dir_entry{
	u32 inode;		// 索引节点号
	u16 rec_len;		// 目录项的长度
	u8 name_len;		// 文件名长度
	u8 file_type;		// 文件类型
	char name[NFS_NAME_LEN];	// 文件名
};

struct ufs_super_block{
	u32 s_magic;		// Magic number: FS_MAGIC
	u32 s_nblocks;		// Total number of blocks on disk
	u32 s_inodes_count;	// 索引结点数
	u32 s_log_block_size;	// 块的大小
	u16 s_inode_size;	// 磁盘上索引节点结构的大小

	u32 s_block_bitmap;	// 块位图的块号
	u32 s_inode_bitmap;	// 索引节点位图的块号
	u32 s_inode_table;	// 第一个索引节点表块的块号
//	u32 s_block;		// 第一个数据块的块号
	struct ufs_dir_entry s_root;		// Root directory node
};

struct ufs_innode{
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
	u32 i_block[NFS_N_BLOCKS];	// 指向数据块的指针
	u8 padding[INODESIZE-38];	// 凑够2的整数倍
};

#endif
