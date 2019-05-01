/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
#include "table.h"
#include "util.h"

#include <endian.h>
#include <stdio.h>

int sqfs_write_table(int outfd, sqfs_super_t *super, const void *data,
		     size_t entsize, size_t count, uint64_t *startblock,
		     compressor_t *cmp)
{
	size_t ent_per_blocks = SQFS_META_BLOCK_SIZE / entsize;
	uint64_t blocks[count / ent_per_blocks + 1];
	size_t i, blkidx = 0, tblsize;
	meta_writer_t *m;
	ssize_t ret;

	/* Write actual data. Whenever we cross a block boundary, remember
	   the block start offset */
	m = meta_writer_create(outfd, cmp);
	if (m == NULL)
		return -1;

	for (i = 0; i < count; ++i) {
		if (blkidx == 0 || m->block_offset > blocks[blkidx - 1])
			blocks[blkidx++] = m->block_offset;

		if (meta_writer_append(m, data, entsize))
			goto fail;

		data = (const char *)data + entsize;
	}

	if (meta_writer_flush(m))
		goto fail;

	for (i = 0; i < blkidx; ++i)
		blocks[i] = htole64(blocks[i] + super->bytes_used);

	super->bytes_used += m->block_offset;
	meta_writer_destroy(m);

	/* write new index table */
	*startblock = super->bytes_used;
	tblsize = sizeof(blocks[0]) * blkidx;

	ret = write_retry(outfd, blocks, tblsize);
	if (ret < 0) {
		perror("writing index table");
		return -1;
	}

	if ((size_t)ret < tblsize) {
		fputs("index table truncated\n", stderr);
		return -1;
	}

	super->bytes_used += tblsize;
	return 0;
fail:
	meta_writer_destroy(m);
	return -1;
}
