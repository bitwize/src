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

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/extattr.h>
#include <sys/param.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_dinode.h>
#include <ufs/ext2fs/ext3fs_journal.h>

#if defined(_KERNEL)
#include <sys/systm.h>
#else
#include <string.h>
#endif

static bool is_journal_block(void *);
static int journal_open_inode(struct mount *, struct vnode **,
    struct journal_superblock **);
static void e3fs_jsb_bswap(struct journal_superblock *,
    struct journal_superblock *);
/* static int journal_find_log_start(struct journal *); */
static void journal_invalidate_superblock(struct journal *);
static int journal_init(struct journal *);
static int journal_descriptor_count_blocks(struct journal *, void *);
static int journal_next_transaction(struct journal *, daddr_t, daddr_t *);


static bool
is_journal_block(void *data)
{
	struct journal_block_header *jbh = 
	    (struct journal_block_header *)data;
	return (J_BSWAP32(jbh->jbh_magic) == JOURNAL_MAGIC);
}

uint32_t
journal_block_type(void *data)
{
	struct journal_block_header *jbh = 
	    (struct journal_block_header *)data;
	return J_BSWAP32(jbh->jbh_block_type);
}

uint32_t
journal_block_sequence(void *data)
{
	struct journal_block_header *jbh = 
	    (struct journal_block_header *)data;
	return J_BSWAP32(jbh->jbh_sequence);
}

static int
journal_open_inode(struct mount *mp, struct vnode **vpp,
		   struct journal_superblock **sbpp)
{
	struct buf *jb_buf;
	void *jb_data;
	struct ufsmount *ump = (struct ufsmount *)mp->mnt_data;
	struct m_ext2fs *fs = ump->um_e2fs;
	struct journal_superblock *jsb;

	int result = VFS_VGET(mp, EXT2_JOURNALINO, vpp);
	if(result != 0) {
		*vpp = NULL;
		return result;
	}
	result = bread(*vpp, 0, (int)fs->e2fs_bsize, NOCRED, B_MODIFY, &jb_buf);
	if(result != 0) {
		vput(*vpp);
		*vpp = NULL;
		return result;
	}
	jb_data = jb_buf->b_data;
	if(!is_journal_block(jb_data)) {
		brelse(jb_buf, 0);
		vput(*vpp);
		*vpp = NULL;
		return EINVAL;
	}
	jsb = (struct journal_superblock *)jb_data;
	if(J_BSWAP32(jsb->jsb_header.jbh_block_type) != JOURNAL_TYPE_SUPERBLOCK_V1 &&
	   J_BSWAP32(jsb->jsb_header.jbh_block_type) != JOURNAL_TYPE_SUPERBLOCK_V2)
	{
		brelse(jb_buf, 0);
		vput(*vpp);
		*vpp = NULL;
		return EINVAL;
	}
	*sbpp = (struct journal_superblock *)kmem_zalloc(
		sizeof(struct journal_superblock), KM_SLEEP);
	if(*sbpp == NULL) {
		brelse(jb_buf, 0);
		vput(*vpp);
		return ENOMEM;
	}
	e3fs_jsb_bswap(jsb, *sbpp);
	brelse(jb_buf, 0);
	VOP_UNLOCK(*vpp);
	return 0;
}

int
journal_open(struct mount *mp,struct journal **jpp)
{
	int error;
	struct ufsmount *ump = (struct ufsmount *)mp->mnt_data;
	struct m_ext2fs *fs = ump->um_e2fs;

	*jpp = kmem_zalloc(sizeof(struct journal), KM_SLEEP);
	error = journal_open_inode(mp, &((*jpp)->jrn_vp),
				   &((*jpp)->jrn_sb));
	if(error != 0) {
		kmem_free(*jpp, sizeof(struct journal));
		*jpp = NULL;
		return error;
	}
	(*jpp)->jrn_fs = fs;
	error = journal_init(*jpp);
	if(error != 0) {
		journal_close(*jpp);
		return error;
	}
	return 0;
}

int
journal_close(struct journal *jp)
{
	if(jp->jrn_vp != NULL) {
		vrele(jp->jrn_vp);
	}
	if(jp->jrn_sb != NULL) {
		kmem_free(jp->jrn_sb, sizeof(struct journal_superblock));
	}
	kmem_free(jp, sizeof(struct journal));
	return 0;
}

static void
e3fs_jsb_bswap(struct journal_superblock *old,
	       struct journal_superblock *new)
{
	new->jsb_header.jbh_magic      = J_BSWAP32(old->jsb_header.jbh_magic);
	new->jsb_header.jbh_block_type = J_BSWAP32(old->jsb_header.jbh_block_type);
	new->jsb_header.jbh_sequence   = J_BSWAP32(old->jsb_header.jbh_sequence);
	new->jsb_block_size            = J_BSWAP32(old->jsb_block_size);
	new->jsb_max_blocks            = J_BSWAP32(old->jsb_max_blocks);
	new->jsb_first_block           = J_BSWAP32(old->jsb_first_block);
	new->jsb_sequence              = J_BSWAP32(old->jsb_sequence);
	new->jsb_log_start             = J_BSWAP32(old->jsb_log_start);
	new->jsb_feature_compat        = J_BSWAP32(old->jsb_feature_compat);
	new->jsb_feature_incompat      = J_BSWAP32(old->jsb_feature_incompat);
	new->jsb_feature_rocompat      = J_BSWAP32(old->jsb_feature_rocompat);
	new->jsb_num_users             = J_BSWAP32(old->jsb_num_users);
	new->jsb_dynsuper_copy         = J_BSWAP32(old->jsb_dynsuper_copy);
	new->jsb_trans_max             = J_BSWAP32(old->jsb_trans_max);
	new->jsb_trans_data_max        = J_BSWAP32(old->jsb_trans_data_max);
	new->jsb_checksum_type         = old->jsb_checksum_type;
	new->jsb_checksum              = J_BSWAP32(old->jsb_checksum);

	memcpy(new->jsb_uuid,old->jsb_uuid,16);
	memcpy(new->jsb_users,old->jsb_users,16 * 48);
}

int
journal_get_block(struct journal *jp, daddr_t blockno, buf_t **out_buf)
{
	int result;
	result = bread(jp->jrn_vp, blockno,(int)jp->jrn_fs->e2fs_bsize,
		       NOCRED, B_MODIFY, out_buf);
	if(result != 0) {
		*out_buf = NULL;
		return result;
	}
	return 0;
}

size_t
journal_tag_size(struct journal *jp)
{
	size_t size =
	    (jp->jrn_sb->jsb_feature_incompat | JOURNALF_INCOMPAT_CHECKSUM_V2) ?
		sizeof(uint16_t) :
		0;
	size += (jp->jrn_sb->jsb_feature_incompat | JOURNALF_INCOMPAT_64BIT) ?
		JOURNAL_TAGSIZE_64BIT :
		JOURNAL_TAGSIZE;
	return size;
}

static void
journal_invalidate_superblock(struct journal *jp)
{
	if(jp->jrn_sb != NULL)
	{
		kmem_free(jp->jrn_sb, sizeof(struct journal_superblock));
		jp->jrn_sb = NULL;
	}
}

static int
journal_init(struct journal *jp)
{
	daddr_t first, last;
	daddr_t w;
	
	first      = jp->jrn_sb->jsb_first_block;
	last       = jp->jrn_sb->jsb_max_blocks;
	if(last - first < JOURNAL_MIN_BLOCKS)
	{
		journal_invalidate_superblock(jp);
		printf("journal is too small\n");
		return EINVAL;
	}
	jp->jrn_first      = first;
	jp->jrn_last       = last;
	jp->jrn_max_blocks = jp->jrn_sb->jsb_max_blocks;
	jp->jrn_block_size = jp->jrn_sb->jsb_block_size;
	jp->jrn_log_start  = jp->jrn_sb->jsb_log_start;
	/* jrn_log_start == 0 means "journal is flushed,
	   volume is up to date" */
#ifdef DEBUG_EXT2
	printf("difference: %ld\n", (long)((char *)&(jp->jrn_sb->jsb_max_blocks) -
					   (char *)&(jp->jrn_sb->jsb_block_size)));
#endif		
	if(jp->jrn_log_start == 0) {
#ifdef DEBUG_EXT2
		printf("journal flushed; no need to commit xacts\n");
#endif		
	} else {
		w = jp->jrn_log_start;
		while(!journal_next_transaction(jp, w, &w)) {}
	}
	//journal_find_log_end(jp);
	return 0;
}

static __inline daddr_t
journal_skip(struct journal *jp, daddr_t start, int n_blocks)
{
	daddr_t result = start + n_blocks;
	while(result > jp->jrn_last) {
		result -= (jp->jrn_last - jp->jrn_first);
	}
	return result;
}

static int
journal_descriptor_count_blocks(struct journal *jp, void *data)
{
	uint16_t flags;
	struct journal_descriptor_tag *tag;
	char *c_data = (char *)data;
	int data_index = 0;
	size_t stride = journal_tag_size(jp);
	int count = 1;
	int max_size = jp->jrn_block_size;
	if(jp->jrn_sb->jsb_feature_incompat |
	   JOURNALF_INCOMPAT_CHECKSUM_V2) {
		max_size -= sizeof(struct journal_descriptor_tail);
	}
	c_data += sizeof(struct journal_block_header);
	while(data_index + stride <= max_size) {
		tag = (struct journal_descriptor_tag *)(&(c_data[data_index]));
		flags = J_BSWAP16(tag->jdt_flags);
		if(flags & JOURNAL_TAG_LAST_ENTRY) {
			return count;
		}
		data_index += stride;
		if(!(flags & JOURNAL_TAG_SAME_UUID)) {
			data_index += 16;
		}
		count++;
	}
	return count;
}

static int
journal_next_transaction(struct journal *jp, daddr_t start, daddr_t *out_next)
{
	int result;
	struct buf *jb_buf;
	void *jb_data;
	while(1) {
#ifdef DEBUG_EXT2
		printf("at journal block %lld\n", (long long int)start);
#endif
		
		result = bread(jp->jrn_vp, start, jp->jrn_sb->jsb_block_size,
			       NOCRED, B_MODIFY, &jb_buf);
		if(result != 0) {
			return result;
		}
		jb_data = jb_buf->b_data;
		if(!is_journal_block(jb_data)) {
			brelse(jb_buf, 0);
			return EINVAL;
		}
		if(journal_block_type(jb_data) == JOURNAL_TYPE_DESCRIPTOR) {
			int n = journal_descriptor_count_blocks(jp, jb_data);
			brelse(jb_buf, 0);
#ifdef DEBUG_EXT2
			printf("found a descriptor journal block; skipping %d blocks\n", n);
#endif
			start = journal_skip(jp, start + 1, n);
			continue;
		}
		else if(journal_block_type(jb_data) == JOURNAL_TYPE_COMMIT ||
			journal_block_type(jb_data) == JOURNAL_TYPE_REVOKE) {
#ifdef DEBUG_EXT2
			printf("found a %s journal block\n",
			       (journal_block_type(jb_data) ==
				JOURNAL_TYPE_COMMIT) ?
			       "commit" :
			       "revoke");
#endif
			brelse(jb_buf, 0);
			*out_next = journal_skip(jp, start, 1);
			return 0;
		}
		else {
			brelse(jb_buf, 0);
#ifdef DEBUG_EXT2
			printf("found a journal block of unknown type!\n");
#endif
			return EINVAL;
		}
	}
}
