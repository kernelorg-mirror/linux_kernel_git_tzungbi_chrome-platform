/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KUNIT_KPROBE_STUB_H
#define _KUNIT_KPROBE_STUB_H

struct kunit;

#define kunit_activate_kprobe_stub(test, func, replacement) do { \
	typecheck_fn(typeof(&func), replacement); \
	__kunit_activate_kprobe_stub(test, #func, func, replacement); \
} while (0)

void __kunit_activate_kprobe_stub(struct kunit *test,
				  const char *name,
				  void *real_fn_addr,
				  void *replacement_addr);

void kunit_deactivate_kprobe_stub(struct kunit *test, void *real_fn_addr);

#endif  /* _KUNIT_KPROBE_STUB_H */
