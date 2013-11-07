/*	$NetBSD$ */

/*
 * Copyright (c) 2013 Jeffrey T. Read.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _UFS_EXT2FS_EXT3FS_JOURNAL_H_
#define _UFS_EXT2FS_EXT3FS_JOURNAL_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/buf.h>

/*
 * The standard header for all descriptor blocks.
 */

#define JOURNAL_MAGIC (0xc03b3998U)

#define JOURNAL_TYPE_DESCRIPTOR     (1)
#define JOURNAL_TYPE_COMMIT         (2)
#define JOURNAL_TYPE_SUPERBLOCK_V1  (3)
#define JOURNAL_TYPE_SUPERBLOCK_V2  (4)
#define JOURNAL_TYPE_REVOKE         (5)

struct journal_block_header {
	uint32_t	jbh_magic; /* magic number */
	uint32_t	jbh_block_type; /* type of block */
	uint32_t	jbh_sequence; /* sequence number */
};

/*
 * Journal superblock
 */

struct journal_superblock {
	struct journal_block_header  jsb_header; /* standard block header */
	uint32_t  jsb_block_size;      	/* device block size */
	uint32_t  jsb_max_blocks;      	/* max journal blocks */
	uint32_t  jsb_first_block;	/* first block of log data */
	uint32_t  jsb_sequence;		/* expected first commit ID */
	uint32_t  jsb_log_start;       	/* block no. of log start */
	uint32_t  jsb_errno;		/* error status of journal */
	uint32_t  jsb_feature_compat;	/* compatible features */
	uint32_t  jsb_feature_incompat;	/* inompatible features */
	uint32_t  jsb_feature_rocompat;	/* read-only compat features */

	/* The following fields are only valid in v2 journal superblocks. */
	uint8_t   jsb_uuid[16];		/* journal uuid */
	uint32_t  jsb_num_users;       	/* no. of filesystems using this log */
	uint32_t  jsb_dynsuper_copy;	/* block # of dynamic SB copy */
	uint32_t  jsb_trans_max;       	/* max # journal blocks in trans. */
	uint32_t  jsb_trans_data_max;	/* max # data blocks in trans. */
	uint8_t   jsb_checksum_type;
	uint8_t   jsb_padding1[3];
	uint32_t  jsb_padding2[42];
	uint32_t  jsb_checksum;		/* crc32 of superblock */
	uint8_t   jsb_users[16 * 48];	/* filesystem IDs of users */
};

/*
 * Journal descriptor tag. Describes one of the data blocks following
 * the descriptor block in a journal transaction. A transaction
 * consists of a descriptor block containing one or more of these,
 * followed by the data blocks (one per tag), followed by a commit
 * block (see `struct journal_commit_block' below).
 */

struct journal_descriptor_tag {
	uint32_t  jdt_blockno_low;     	/* low bits of block no. to be updated */
	uint16_t  jdt_checksum;		/* truncated crc of uuid,seq,block */
	uint16_t  jdt_flags;		/* flags for block */
	uint32_t  jdt_blockno_high;	/* high bits of block no. (JBD2) */
};

struct journal_descriptor_tail {
	uint32_t  jdt_checksum;		/* crc32 of uuid and descriptor */
};

#define JOURNAL_TAGSIZE_64BIT	(sizeof(journal_descriptor_tag))
#define JOURNAL_TAGSIZE		(sizeof(journal_descriptor_tag) - \
				     sizeof(uint32_t))

/*
 * The header of the revoke block. The body of the block is a list of
 * block numbers to be revoked from the journal checkpointing.
 */

struct journal_revoke_block_header {
	struct journal_block_header jrb_header;
	uint32_t  jrb_size;		/* Size of revoke data in bytes */
};

struct journal_revoke_block_tail {
	uint32_t  jrb_checksum;		/* crc32 of uuid and descriptor */
};

/*
 * Commit block checksum types
 */

#define JOURNAL_CRC32_CHECKSUM  1
#define JOURNAL_MD5_CHECKSUM    2
#define JOURNAL_SHA1_CHECKSUM   3
#define JOURNAL_CRC32C_CHECKSUM 4 

/* commit block max checksum size in bytes */
#define JOURNAL_COMMIT_MAX_CHECKSUM_SIZE (32)

struct journal_commit_block {
	struct journal_block_header jcb_header;
	uint8_t   jcb_checksum_size;
	uint8_t   jcb_checksum_type;
	uint16_t  jcb_padding;
	uint32_t  jcb_checksum[JOURNAL_COMMIT_MAX_CHECKSUM_SIZE];
	uint64_t  jcb_timestamp_sec;
	uint32_t  jcb_timestamp_nsec;
};

struct vnode;
struct mount;
struct m_ext2fs;
struct journal_transaction;

struct journal {
	struct vnode   *jrn_vp;		/* vnode of journal file */
	struct m_ext2fs *jrn_fs;	/* fs we're journalling */
	/* transaction currently accumulating IO ops */
	struct journal_transaction *jrn_active_transaction;
	/* transaction being committed */
	struct journal_transaction *jrn_commit_transaction;
	/* list of transactions to be checkpointed */
	LIST_HEAD(journal_transaction_head, journal_transaction) jrn_checkpoint_transactions;
	uint32_t  jrn_flags;
	blkcnt_t  jrn_size;
	blkcnt_t  jrn_free;
	daddr_t   jrn_first;
	daddr_t   jrn_last;
	daddr_t   jrn_head;
	daddr_t   jrn_tail;
};

int journal_open(struct mount *, struct journal **);
int journal_open_inode(struct mount *, struct vnode **);
int journal_get_block(struct journal *, daddr_t, buf_t **);
int journal_next_block(struct journal *, buf_t **);
int journal_close(struct journal **);

#endif /* !_UFS_EXT2FS_EXT3FS_JOURNAL_H_ */
