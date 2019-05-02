/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_reader.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#define SWAB16(x) x = le16toh(x)
#define SWAB32(x) x = le32toh(x)
#define SWAB64(x) x = le64toh(x)

static int check_mode(sqfs_inode_t *inode)
{
	switch (inode->mode & S_IFMT) {
	case S_IFSOCK:
		if (inode->type != SQFS_INODE_SOCKET &&
		    inode->type != SQFS_INODE_EXT_SOCKET) {
			goto fail_mismatch;
		}
		break;
	case S_IFLNK:
		if (inode->type != SQFS_INODE_SLINK &&
		    inode->type != SQFS_INODE_EXT_SLINK) {
			goto fail_mismatch;
		}
		break;
	case S_IFREG:
		if (inode->type != SQFS_INODE_FILE &&
		    inode->type != SQFS_INODE_EXT_FILE) {
			goto fail_mismatch;
		}
		break;
	case S_IFBLK:
		if (inode->type != SQFS_INODE_BDEV &&
		    inode->type != SQFS_INODE_EXT_BDEV) {
			goto fail_mismatch;
		}
		break;
	case S_IFDIR:
		if (inode->type != SQFS_INODE_DIR &&
		    inode->type != SQFS_INODE_EXT_DIR) {
			goto fail_mismatch;
		}
		break;
	case S_IFCHR:
		if (inode->type != SQFS_INODE_CDEV &&
		    inode->type != SQFS_INODE_EXT_CDEV) {
			goto fail_mismatch;
		}
		break;
	case S_IFIFO:
		if (inode->type != SQFS_INODE_FIFO &&
		    inode->type != SQFS_INODE_EXT_FIFO) {
			goto fail_mismatch;
		}
		break;
	default:
		fputs("Found inode with unknown file mode\n", stderr);
		return -1;
	}

	return 0;
fail_mismatch:
	fputs("Found inode where type does not match mode\n", stderr);
	return -1;
}

static sqfs_inode_generic_t *read_inode_file(meta_reader_t *ir,
					     sqfs_inode_t *base,
					     size_t block_size)
{
	sqfs_inode_generic_t *out;
	sqfs_inode_file_t file;
	size_t i, count;

	if (meta_reader_read(ir, &file, sizeof(file)))
		return NULL;

	SWAB32(file.blocks_start);
	SWAB32(file.fragment_index);
	SWAB32(file.fragment_offset);
	SWAB32(file.file_size);

	count = file.file_size / block_size;

	out = calloc(1, sizeof(*out) + count * sizeof(uint32_t));
	if (out == NULL) {
		perror("reading extended file inode");
		return NULL;
	}

	out->base = *base;
	out->data.file = file;
	out->block_sizes = (uint32_t *)out->extra;

	if (meta_reader_read(ir, out->block_sizes, count * sizeof(uint32_t))) {
		free(out);
		return NULL;
	}

	for (i = 0; i < count; ++i)
		SWAB32(out->block_sizes[i]);

	return out;
}

static sqfs_inode_generic_t *read_inode_file_ext(meta_reader_t *ir,
						 sqfs_inode_t *base,
						 size_t block_size)
{
	sqfs_inode_file_ext_t file;
	sqfs_inode_generic_t *out;
	size_t i, count;

	if (meta_reader_read(ir, &file, sizeof(file)))
		return NULL;

	SWAB64(file.blocks_start);
	SWAB64(file.file_size);
	SWAB64(file.sparse);
	SWAB32(file.nlink);
	SWAB32(file.fragment_idx);
	SWAB32(file.fragment_offset);
	SWAB32(file.xattr_idx);

	count = file.file_size / block_size;

	out = calloc(1, sizeof(*out) + count * sizeof(uint32_t));
	if (out == NULL) {
		perror("reading extended file inode");
		return NULL;
	}

	out->base = *base;
	out->data.file_ext = file;
	out->block_sizes = (uint32_t *)out->extra;

	if (meta_reader_read(ir, out->block_sizes, count * sizeof(uint32_t))) {
		free(out);
		return NULL;
	}

	for (i = 0; i < count; ++i)
		SWAB32(out->block_sizes[i]);

	return out;
}

static sqfs_inode_generic_t *read_inode_slink(meta_reader_t *ir,
					      sqfs_inode_t *base)
{
	sqfs_inode_generic_t *out;
	sqfs_inode_slink_t slink;

	if (meta_reader_read(ir, &slink, sizeof(slink)))
		return NULL;

	SWAB32(slink.nlink);
	SWAB32(slink.target_size);

	out = calloc(1, sizeof(*out) + slink.target_size + 1);
	if (out == NULL) {
		perror("reading symlink inode");
		return NULL;
	}

	out->slink_target = (char *)out->extra;
	out->base = *base;
	out->data.slink = slink;

	if (meta_reader_read(ir, out->slink_target, slink.target_size)) {
		free(out);
		return NULL;
	}

	return out;
}

static sqfs_inode_generic_t *read_inode_slink_ext(meta_reader_t *ir,
						  sqfs_inode_t *base)
{
	sqfs_inode_generic_t *out = read_inode_slink(ir, base);
	uint32_t xattr;

	if (out != NULL) {
		if (meta_reader_read(ir, &xattr, sizeof(xattr))) {
			free(out);
			return NULL;
		}

		out->data.slink_ext.xattr_idx = le32toh(xattr);
	}
	return 0;
}

sqfs_inode_generic_t *meta_reader_read_inode(meta_reader_t *ir,
					     sqfs_super_t *super,
					     uint64_t block_start,
					     size_t offset)
{
	sqfs_inode_generic_t *out;
	sqfs_inode_t inode;

	/* read base inode */
	block_start += super->inode_table_start;

	if (meta_reader_seek(ir, block_start, offset))
		return NULL;

	if (meta_reader_read(ir, &inode, sizeof(inode)))
		return NULL;

	SWAB16(inode.type);
	SWAB16(inode.mode);
	SWAB16(inode.uid_idx);
	SWAB16(inode.gid_idx);
	SWAB32(inode.mod_time);
	SWAB32(inode.inode_number);

	if (check_mode(&inode))
		return NULL;

	/* inode types where the size is variable */
	switch (inode.type) {
	case SQFS_INODE_FILE:
		return read_inode_file(ir, &inode, super->block_size);
	case SQFS_INODE_SLINK:
		return read_inode_slink(ir, &inode);
	case SQFS_INODE_EXT_FILE:
		return read_inode_file_ext(ir, &inode, super->block_size);
	case SQFS_INODE_EXT_SLINK:
		return read_inode_slink_ext(ir, &inode);
	default:
		break;
	}

	/* everything else */
	out = calloc(1, sizeof(*out));
	if (out == NULL) {
		perror("reading symlink inode");
		return NULL;
	}

	out->base = inode;

	switch (inode.type) {
	case SQFS_INODE_DIR:
		if (meta_reader_read(ir, &out->data.dir,
				     sizeof(out->data.dir))) {
			goto fail_free;
		}
		SWAB32(out->data.dir.start_block);
		SWAB32(out->data.dir.nlink);
		SWAB16(out->data.dir.size);
		SWAB16(out->data.dir.offset);
		SWAB32(out->data.dir.parent_inode);
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		if (meta_reader_read(ir, &out->data.dev,
				     sizeof(out->data.dev))) {
			goto fail_free;
		}
		SWAB32(out->data.dev.nlink);
		SWAB32(out->data.dev.devno);
		break;
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		if (meta_reader_read(ir, &out->data.ipc,
				     sizeof(out->data.ipc))) {
			goto fail_free;
		}
		SWAB32(out->data.ipc.nlink);
		break;
	case SQFS_INODE_EXT_DIR:
		if (meta_reader_read(ir, &out->data.dir_ext,
				     sizeof(out->data.dir_ext))) {
			goto fail_free;
		}
		SWAB32(out->data.dir_ext.nlink);
		SWAB32(out->data.dir_ext.size);
		SWAB32(out->data.dir_ext.start_block);
		SWAB32(out->data.dir_ext.parent_inode);
		SWAB16(out->data.dir_ext.inodex_count);
		SWAB16(out->data.dir_ext.offset);
		SWAB32(out->data.dir_ext.xattr_idx);
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		if (meta_reader_read(ir, &out->data.dev_ext,
				     sizeof(out->data.dev_ext))) {
			goto fail_free;
		}
		SWAB32(out->data.dev_ext.nlink);
		SWAB32(out->data.dev_ext.devno);
		SWAB32(out->data.dev_ext.xattr_idx);
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		if (meta_reader_read(ir, &out->data.ipc_ext,
				     sizeof(out->data.ipc_ext))) {
			goto fail_free;
		}
		SWAB32(out->data.ipc_ext.nlink);
		SWAB32(out->data.ipc_ext.xattr_idx);
		break;
	default:
		fputs("Unknown inode type found\n", stderr);
		goto fail_free;
	}

	return out;
fail_free:
	free(out);
	return NULL;
}