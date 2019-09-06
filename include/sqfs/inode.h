/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * inode.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_INODE_H
#define SQFS_INODE_H

#include "sqfs/predef.h"

typedef enum {
	SQFS_INODE_DIR = 1,
	SQFS_INODE_FILE = 2,
	SQFS_INODE_SLINK = 3,
	SQFS_INODE_BDEV = 4,
	SQFS_INODE_CDEV = 5,
	SQFS_INODE_FIFO = 6,
	SQFS_INODE_SOCKET = 7,
	SQFS_INODE_EXT_DIR = 8,
	SQFS_INODE_EXT_FILE = 9,
	SQFS_INODE_EXT_SLINK = 10,
	SQFS_INODE_EXT_BDEV = 11,
	SQFS_INODE_EXT_CDEV = 12,
	SQFS_INODE_EXT_FIFO = 13,
	SQFS_INODE_EXT_SOCKET = 14,
} E_SQFS_INODE_TYPE;

struct sqfs_inode_t {
	uint16_t type;
	uint16_t mode;
	uint16_t uid_idx;
	uint16_t gid_idx;
	uint32_t mod_time;
	uint32_t inode_number;
};

struct sqfs_inode_dev_t {
	uint32_t nlink;
	uint32_t devno;
};

struct sqfs_inode_dev_ext_t {
	uint32_t nlink;
	uint32_t devno;
	uint32_t xattr_idx;
};

struct sqfs_inode_ipc_t {
	uint32_t nlink;
};

struct sqfs_inode_ipc_ext_t {
	uint32_t nlink;
	uint32_t xattr_idx;
};

struct sqfs_inode_slink_t {
	uint32_t nlink;
	uint32_t target_size;
	/*uint8_t target[];*/
};

struct sqfs_inode_slink_ext_t {
	uint32_t nlink;
	uint32_t target_size;
	/*uint8_t target[];*/
	uint32_t xattr_idx;
};

struct sqfs_inode_file_t {
	uint32_t blocks_start;
	uint32_t fragment_index;
	uint32_t fragment_offset;
	uint32_t file_size;
	/*uint32_t block_sizes[];*/
};

struct sqfs_inode_file_ext_t {
	uint64_t blocks_start;
	uint64_t file_size;
	uint64_t sparse;
	uint32_t nlink;
	uint32_t fragment_idx;
	uint32_t fragment_offset;
	uint32_t xattr_idx;
	/*uint32_t block_sizes[];*/
};

struct sqfs_inode_dir_t {
	uint32_t start_block;
	uint32_t nlink;
	uint16_t size;
	uint16_t offset;
	uint32_t parent_inode;
};

struct sqfs_inode_dir_ext_t {
	uint32_t nlink;
	uint32_t size;
	uint32_t start_block;
	uint32_t parent_inode;
	uint16_t inodex_count;
	uint16_t offset;
	uint32_t xattr_idx;
};

struct sqfs_inode_generic_t {
	sqfs_inode_t base;
	char *slink_target;
	uint32_t *block_sizes;
	size_t num_file_blocks;

	union {
		sqfs_inode_dev_t dev;
		sqfs_inode_dev_ext_t dev_ext;
		sqfs_inode_ipc_t ipc;
		sqfs_inode_ipc_ext_t ipc_ext;
		sqfs_inode_slink_t slink;
		sqfs_inode_slink_ext_t slink_ext;
		sqfs_inode_file_t file;
		sqfs_inode_file_ext_t file_ext;
		sqfs_inode_dir_t dir;
		sqfs_inode_dir_ext_t dir_ext;
	} data;

	uint8_t extra[];
};

#endif /* SQFS_INODE_H */