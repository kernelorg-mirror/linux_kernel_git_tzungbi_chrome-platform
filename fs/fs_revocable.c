// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2025 Google LLC
 *
 * File operation replacement with Revocable
 */

#include <linux/cleanup.h>
#include <linux/fs_revocable.h>
#include <linux/poll.h>
#include <linux/revocable.h>

struct fs_revocable_replacement {
	const struct fs_revocable_operations *frops;
	const struct file_operations *orig_fops;
	struct file_operations fops;
	struct revocable **revs;
	size_t num_revs;
};

static int fs_revocable_try_access(struct file *filp)
{
	struct fs_revocable_replacement *rr = filp->f_rr;

	return rr->frops->try_access(rr->revs, rr->num_revs,
				     filp->private_data);
}

static void fs_revocable_withdraw_access(struct fs_revocable_replacement *rr)
{
	for (size_t i = 0; i < rr->num_revs; ++i)
		revocable_withdraw_access(rr->revs[i]);
}

DEFINE_FREE(fs_revocable_replacement, struct fs_revocable_replacement *,
	    if (_T) fs_revocable_withdraw_access(_T))

static ssize_t fs_revocable_read(struct file *filp, char __user *buffer,
				 size_t length, loff_t *offset)
{
	struct fs_revocable_replacement *rr
			__free(fs_revocable_replacement) = filp->f_rr;
	int ret;

	ret = fs_revocable_try_access(filp);
	if (ret)
		return ret;
	return rr->orig_fops->read(filp, buffer, length, offset);
}

static __poll_t fs_revocable_poll(struct file *filp, poll_table *wait)
{
	struct fs_revocable_replacement *rr
			__free(fs_revocable_replacement) = filp->f_rr;
	int ret;

	ret = fs_revocable_try_access(filp);
	if (ret)
		return ret;
	return rr->orig_fops->poll(filp, wait);
}

static long fs_revocable_unlocked_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	struct fs_revocable_replacement *rr
			__free(fs_revocable_replacement) = filp->f_rr;
	int ret;

	ret = fs_revocable_try_access(filp);
	if(ret)
		return ret;
	return rr->orig_fops->unlocked_ioctl(filp, cmd, arg);
}

static int fs_revocable_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct fs_revocable_replacement *rr = filp->f_rr;

	if (!rr->orig_fops->release)
		goto recover;

	ret = fs_revocable_try_access(filp);
	if(ret)
		goto recover;

	ret = rr->orig_fops->release(inode, filp);

	fs_revocable_withdraw_access(rr);
recover:
	filp->f_op = rr->orig_fops;
	filp->f_rr = NULL;

	for (size_t i = 0; i < rr->num_revs; ++i)
		revocable_free(rr->revs[i]);
	kfree(rr->revs);
	kfree(rr);

	return ret;
}

/**
 * fs_revocable_replace() - Replace the file operations to be revocable-aware.
 *
 * Should be used only from ->open() instances.
 */
int fs_revocable_replace(struct file *filp,
			 const struct fs_revocable_operations *frops,
			 struct revocable_provider **rps, size_t num_rps)
{
	struct fs_revocable_replacement *rr;
	size_t i;

	rr = kzalloc(sizeof(*rr), GFP_KERNEL);
	if (!rr)
		return -ENOMEM;
	filp->f_rr = rr;

	rr->frops = frops;
	rr->revs = kcalloc(num_rps, sizeof(*rr->revs), GFP_KERNEL);
	if (!rr->revs)
		goto free_rr;
	for (i = 0; i < num_rps; ++i) {
		rr->revs[i] = revocable_alloc(rps[i]);
		if (!rr->revs[i])
			goto free_revs;
	}
	rr->num_revs = num_rps;
	rr->orig_fops = filp->f_op;

	memcpy(&rr->fops, filp->f_op, sizeof(rr->fops));
	rr->fops.release = fs_revocable_release;

	if (rr->fops.read)
		rr->fops.read = fs_revocable_read;
	if (rr->fops.poll)
		rr->fops.poll = fs_revocable_poll;
	if (rr->fops.unlocked_ioctl)
		rr->fops.unlocked_ioctl = fs_revocable_unlocked_ioctl;

	filp->f_op = &rr->fops;
	return 0;
free_revs:
	if (i) {
		while (--i)
			revocable_free(rr->revs[i]);
	}
	kfree(rr->revs);
free_rr:
	kfree(rr);
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(fs_revocable_replace);
