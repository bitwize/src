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
#include <sys/bswap.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_dinode.h>
#include <ufs/ext2fs/ext3fs_journal.h>


bool is_journal_block(void *);
int journal_open_inode(struct mount *, struct vnode **);

bool
is_journal_block(void *data)
{
	struct journal_block_header *jbh = 
	    (struct journal_block_header *)data;
#if BYTE_ORDER == BIG_ENDIAN
	return (jbh->jbh_magic == JOURNAL_MAGIC);
#else
	return (bswap32(jbh->jbh_magic) == JOURNAL_MAGIC);
#endif
}

int
journal_open_inode(struct mount *mp, struct vnode **vpp)
{
	struct buf *jb_buf;
	void *jb_data;
	struct ufsmount *ump = (struct ufsmount *)mp->mnt_data;
	struct m_ext2fs *fs = ump->um_e2fs;
	int result = VFS_VGET(mp, EXT2_JOURNALINO, vpp);
	if(result != 0) {
		*vpp = NULL;
		return result;
	}
	result = bread(*vpp,0,(int)fs->e2fs_bsize, NOCRED, B_MODIFY, &jb_buf);
	if(result != 0) {
		vrele(*vpp);
		brelse(jb_buf, 0);
		*vpp = NULL;
		return result;
	}
	jb_data = jb_buf->b_data;
	if(!is_journal_block(jb_data)) {
		vrele(*vpp);
		brelse(jb_buf, 0);
		*vpp = NULL;
		return EINVAL;
	}
	brelse(jb_buf, 0);
	return 0;
}
