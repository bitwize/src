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

bool is_journal_block(void *);
int journal_open_inode(struct mount *, struct vnode **,
    struct journal_superblock **);
void e3fs_jsb_bswap(struct journal_superblock *, struct journal_superblock *);

bool
is_journal_block(void *data)
{
	struct journal_block_header *jbh = 
	    (struct journal_block_header *)data;
	return (J_BSWAP32(jbh->jbh_magic) == JOURNAL_MAGIC);
}

int
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
	result = bread(*vpp,0,(int)fs->e2fs_bsize, NOCRED, B_MODIFY, &jb_buf);
	if(result != 0) {
		brelse(jb_buf, 0);
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
		return ENOMEM;
	}
	e3fs_jsb_bswap(jsb, *sbpp);
	brelse(jb_buf, 0);
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
	(*jpp)->jrn_max_blocks = (*jpp)->jrn_sb->jsb_max_blocks;
	(*jpp)->jrn_block_size = (*jpp)->jrn_sb->jsb_block_size;
	return 0;
}

int
journal_close(struct journal *jp)
{
	if(jp->jrn_vp != NULL) {
		vput(jp->jrn_vp);
	}
	kmem_free(jp->jrn_sb, sizeof(struct journal_superblock));
	kmem_free(jp, sizeof(struct journal));
	return 0;
}

void
e3fs_jsb_bswap(struct journal_superblock *old,
	       struct journal_superblock *new)
{
	new->jsb_header.jbh_magic = J_BSWAP32(old->jsb_header.jbh_magic);
	new->jsb_header.jbh_block_type = J_BSWAP32(old->jsb_header.jbh_block_type);
	new->jsb_header.jbh_sequence = J_BSWAP32(old->jsb_header.jbh_sequence);
	new->jsb_block_size = J_BSWAP32(old->jsb_block_size);
	new->jsb_max_blocks = J_BSWAP32(old->jsb_max_blocks);
	new->jsb_first_block = J_BSWAP32(old->jsb_first_block);
	new->jsb_sequence = J_BSWAP32(old->jsb_sequence);
	new->jsb_feature_compat = J_BSWAP32(old->jsb_feature_compat);
	new->jsb_feature_incompat = J_BSWAP32(old->jsb_feature_incompat);
	new->jsb_feature_rocompat = J_BSWAP32(old->jsb_feature_rocompat);
	memcpy(new->jsb_uuid,old->jsb_uuid,16);
	new->jsb_num_users = J_BSWAP32(old->jsb_num_users);
	new->jsb_dynsuper_copy = J_BSWAP32(old->jsb_dynsuper_copy);
	new->jsb_trans_max = J_BSWAP32(old->jsb_trans_max);
	new->jsb_trans_data_max = J_BSWAP32(old->jsb_trans_data_max);
	new->jsb_checksum_type = old->jsb_checksum_type;
	new->jsb_checksum = J_BSWAP32(old->jsb_checksum);
	memcpy(new->jsb_users,old->jsb_users,16 * 48);
}
