// SPDX-License-Identifier: GPL-2.0
/*
 * Kunit tests for ChromeOS Embedded Controller
 */

#include <kunit/test.h>

#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_device.h>

struct cros_ec_dev_test_priv {
	struct cros_ec_device ec_dev;
};

static struct cros_ec_platform ec_p = {
	.ec_name = "fake",
	.cmd_offset = 0,
};

static void cros_ec_dev_test_platform_data_constantly(struct kunit *test)
{
	struct cros_ec_dev_test_priv *priv = test->priv;
	struct cros_ec_device *ec_dev = &priv->ec_dev;
	struct platform_device *pdev1;

	pdev1 = platform_device_register_data(ec_dev->dev, "cros-ec-dev",
			PLATFORM_DEVID_AUTO, &ec_p, sizeof(struct cros_ec_platform));

	KUNIT_ASSERT_STREQ(test, ec_p.ec_name, "fake");

	platform_device_unregister(pdev1);
}

static void cros_ec_dev_test_release(struct device *dev)
{
}

static int fake_pkt_xfer(struct cros_ec_device *ec, struct cros_ec_command *msg)
{
	if (msg->command == EC_CMD_GET_FEATURES) {
		struct ec_response_get_features *features = (struct ec_response_get_features *) msg->data;
		features->flags[0] = BIT(EC_FEATURE_FINGERPRINT);
	}
	return 0;
}

static int cros_ec_dev_test_init(struct kunit *test)
{
	struct cros_ec_dev_test_priv *priv;
	struct cros_ec_device *ec_dev;

	priv = kunit_kzalloc(test, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	test->priv = priv;

	ec_dev = &priv->ec_dev;
	ec_dev->dev = kunit_kzalloc(test, sizeof(*ec_dev->dev), GFP_KERNEL);
	if (!ec_dev->dev)
		return -ENOMEM;

	ec_dev->dev->init_name = "fake_parent";
	ec_dev->dev->release = cros_ec_dev_test_release;
	ec_dev->pkt_xfer = fake_pkt_xfer;
	ec_dev->proto_version = 3;
	dev_set_drvdata(ec_dev->dev, ec_dev);

	return device_register(ec_dev->dev);
}

static void cros_ec_dev_test_exit(struct kunit *test)
{
	struct cros_ec_dev_test_priv *priv = test->priv;
	struct cros_ec_device *ec_dev = &priv->ec_dev;

	put_device(ec_dev->dev);
}

static struct kunit_case cros_ec_dev_test_cases[] = {
	KUNIT_CASE(cros_ec_dev_test_platform_data_constantly),
	{}
};

static struct kunit_suite cros_ec_dev_test_suite = {
	.name = "cros_ec_dev_test",
	.init = cros_ec_dev_test_init,
	.exit = cros_ec_dev_test_exit,
	.test_cases = cros_ec_dev_test_cases,
};
kunit_test_suite(cros_ec_dev_test_suite);

MODULE_DESCRIPTION("Kunit tests for ChromeOS Embedded Controller");
MODULE_LICENSE("GPL");
