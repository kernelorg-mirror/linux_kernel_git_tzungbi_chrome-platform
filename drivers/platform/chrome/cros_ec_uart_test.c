// SPDX-License-Identifier: GPL-2.0
/*
 * Kunit tests for ChromeOS Embedded Controller UART interface.
 */
#include <kunit/test.h>
#include <kunit/ftrace_stub.h>

#include <linux/acpi.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/sched.h>
#include <linux/serdev.h>

#include "cros_ec.h"

struct cros_ec_uart_test_priv {
	struct class *class;

	struct device ctrl_dev;
	struct acpi_device ctrl_adev;
	struct serdev_controller *ctrl;

	struct device dev;
	struct acpi_device adev;
	struct serdev_device *sdev;
};

static int fake_acpi_dev_get_resources(struct acpi_device *adev, struct list_head *list,
		int (*preproc)(struct acpi_resource *, void *),
		void *preproc_data)
{
	struct kunit *test = current->kunit_test;

	kunit_info(test, "%s\n", __func__);
	return 0;
}

static int fake_acpi_dev_gpio_irq_wake_get_by(struct acpi_device *adev, const char *name, int index,
					      bool *wake_capable)
{
	struct kunit *test = current->kunit_test;

	kunit_info(test, "%s\n", __func__);
	return 100;
}

static int fake_cros_ec_register(struct cros_ec_device *ec_dev)
{
	struct kunit *test = current->kunit_test;

	kunit_info(test, "%s\n", __func__);
	return 0;
}

static void fake_cros_ec_unregister(struct cros_ec_device *ec_dev)
{
	struct kunit *test = current->kunit_test;

	kunit_info(test, "%s\n", __func__);
}

static void cros_ec_uart_test_basic(struct kunit *test)
{
}

static int ctrl_open(struct serdev_controller *ctrl)
{
	return 0;
}

static const struct serdev_controller_ops ctrl_ops = {
	.open = ctrl_open,
};

static void dummy_ktype_release(struct kobject *kobj)
{
}

static const struct kobj_type dummy_ktype = {
	.release = dummy_ktype_release,
};

static int find_target_driver(struct device_driver *drv, void *data)
{
	struct device_driver **pdrv = data;

	if (strcmp(drv->name, "cros-ec-uart") == 0)
		*pdrv = drv;
	return 0;
}

static int cros_ec_uart_test_init(struct kunit *test)
{
	struct cros_ec_uart_test_priv *priv;
	int ret;
	struct device_driver *drv;
	enum probe_type orig;
	static struct acpi_hardware_id hwid = {
		.id = "GOOG0019",
	};

	kunit_activate_ftrace_stub(test, acpi_dev_get_resources, fake_acpi_dev_get_resources);
	kunit_activate_ftrace_stub(test, acpi_dev_gpio_irq_wake_get_by,
				   fake_acpi_dev_gpio_irq_wake_get_by);
	kunit_activate_ftrace_stub(test, cros_ec_register, fake_cros_ec_register);
	kunit_activate_ftrace_stub(test, cros_ec_unregister, fake_cros_ec_unregister);

	priv = kunit_kzalloc(test, sizeof(*priv), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, priv);
	test->priv = priv;

	priv->class = class_create("dummy_class");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, priv->class);

	priv->ctrl_dev.class = priv->class;
	dev_set_name(&priv->ctrl_dev, "dummy_ctrl_dev");
	ret = device_register(&priv->ctrl_dev);
	if (ret)
		put_device(&priv->ctrl_dev);
	KUNIT_ASSERT_EQ(test, ret, 0);

	mutex_init(&priv->ctrl_adev.physical_node_lock);
	INIT_LIST_HEAD(&priv->ctrl_adev.physical_node_list);
	fwnode_init(&priv->ctrl_adev.fwnode, &acpi_device_fwnode_ops);
	device_initialize(&priv->ctrl_adev.dev);
	ACPI_COMPANION_SET(&priv->ctrl_dev, &priv->ctrl_adev);

	priv->ctrl = serdev_controller_alloc(&priv->ctrl_dev, 0);
	KUNIT_ASSERT_NOT_NULL(test, priv->ctrl);
	priv->ctrl->ops = &ctrl_ops;
	priv->ctrl->serdev = (void *)true;
	KUNIT_ASSERT_EQ(test, serdev_controller_add(priv->ctrl), 0);
	priv->ctrl->serdev = NULL;

	mutex_init(&priv->adev.physical_node_lock);
	INIT_LIST_HEAD(&priv->adev.physical_node_list);
	fwnode_init(&priv->adev.fwnode, &acpi_device_fwnode_ops);
	device_initialize(&priv->adev.dev);

	INIT_LIST_HEAD(&priv->adev.pnp.ids);
	INIT_LIST_HEAD(&hwid.list);
	list_add_tail(&hwid.list, &priv->adev.pnp.ids);

	priv->adev.pnp.type.hardware_id = 1;
	priv->adev.status.present = 1;
	priv->adev.dev.kobj.ktype = &dummy_ktype;
	ret = kobject_add(&priv->adev.dev.kobj, NULL, "dummy_kobj");
	KUNIT_ASSERT_GE(test, ret, 0);

	priv->sdev = serdev_device_alloc(priv->ctrl);
	KUNIT_ASSERT_NOT_NULL(test, priv->sdev);
	ACPI_COMPANION_SET(&priv->sdev->dev, &priv->adev);
	KUNIT_ASSERT_NOT_NULL(test, ACPI_COMPANION(&priv->sdev->dev));

	/*
	 * Asynchronous probe makes troubles with ftrace_stub (see kunit_stub_trampoline() in
	 * lib/kunit/ftrace_stub.c).  Turn it off.
	 */
	ret = bus_for_each_drv(priv->ctrl->dev.bus, NULL, &drv, find_target_driver);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_NOT_NULL(test, drv);
	orig = drv->probe_type;
	drv->probe_type = PROBE_FORCE_SYNCHRONOUS;

	ret = serdev_device_add(priv->sdev);

	/* Restore the probe_type. */
	drv->probe_type = orig;

	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void cros_ec_uart_test_exit(struct kunit *test)
{
	struct cros_ec_uart_test_priv *priv = test->priv;

	serdev_device_remove(priv->sdev);

	kobject_put(&priv->adev.dev.kobj);

	serdev_controller_remove(priv->ctrl);
	device_del(&priv->ctrl_dev);

	class_destroy(priv->class);

	kunit_deactivate_ftrace_stub(test, acpi_dev_get_resources);
	kunit_deactivate_ftrace_stub(test, acpi_dev_gpio_irq_wake_get_by);
	kunit_deactivate_ftrace_stub(test, cros_ec_register);
	kunit_deactivate_ftrace_stub(test, cros_ec_unregister);
}

static struct kunit_case cros_ec_uart_test_cases[] = {
	KUNIT_CASE(cros_ec_uart_test_basic),
	{}
};

static struct kunit_suite cros_ec_uart_test_suite = {
	.name = "cros_ec_uart_test",
	.init = cros_ec_uart_test_init,
	.exit = cros_ec_uart_test_exit,
	.test_cases = cros_ec_uart_test_cases,
};

kunit_test_suite(cros_ec_uart_test_suite);

MODULE_LICENSE("GPL");
