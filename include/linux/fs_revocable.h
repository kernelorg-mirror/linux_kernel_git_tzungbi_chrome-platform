/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2025 Google LLC
 */

#ifndef __LINUX_FS_REVOCABLE_H
#define __LINUX_FS_REVOCABLE_H

#include <linux/fs.h>
#include <linux/revocable.h>

struct fs_revocable_operations {
	int (*try_access)(struct revocable **revs, size_t num_revs, void *data);
};

int fs_revocable_replace(struct file *filp,
			 const struct fs_revocable_operations *frops,
			 struct revocable_provider **rps, size_t num_rps);

#endif /* __LINUX_FS_REVOCABLE_H */

