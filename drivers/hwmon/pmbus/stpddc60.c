// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Hardware monitoring driver for the STPDDC60 controller.
 *
 * Copyright (c) 2020 Flex AB
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/pmbus.h>
#include "pmbus.h"

enum chips { stpddc60 };

static const struct i2c_device_id stpddc60_id[] = {
	{"stpddc60", stpddc60},
	{"bmr481", stpddc60},
	{}
};
MODULE_DEVICE_TABLE(i2c, stpddc60_id);

static struct pmbus_driver_info stpddc60_info = {
	.pages = 1,
	.func[0] = PMBUS_HAVE_VOUT | PMBUS_HAVE_STATUS_VOUT
		| PMBUS_HAVE_VIN | PMBUS_HAVE_STATUS_INPUT
		| PMBUS_HAVE_TEMP | PMBUS_HAVE_STATUS_TEMP
		| PMBUS_HAVE_IOUT | PMBUS_HAVE_STATUS_IOUT
		| PMBUS_HAVE_POUT,
};

/*
 * Convert VID value to milli-volt
 */
static long stpddc60_vid2mv(int val)
{
	long rv = 0;

	if (val >= 0x01)
		rv = 250 + (val - 1) * 5;

	return rv;
}

/*
 * Convert milli-volt to linear
 */
static int stpddc60_mv2l(long mv)
{
	int rv;

	rv = (mv << 8) / 1000;

	return rv;
}

static int stpddc60_read_byte_data(struct i2c_client *client, int page, int reg)
{
	int ret;

	if (page > 0)
		return -ENXIO;

	switch (reg) {
	case PMBUS_VOUT_MODE:
		ret = 0x18;
		break;
	default:
		ret = -ENODATA;
		break;
	}

	return ret;
}

static int stpddc60_read_word_data(struct i2c_client *client, int page,
				 int phase, int reg)
{
	int ret;

	if (page > 0)
		return -ENXIO;

	switch (reg) {
	case PMBUS_READ_VOUT:
		ret = pmbus_read_word_data(client, page, phase, reg);
		if(ret < 0)
			goto abort;
		ret = stpddc60_mv2l(stpddc60_vid2mv(ret));
		break;
	case PMBUS_VOUT_OV_FAULT_LIMIT:
	case PMBUS_VOUT_UV_FAULT_LIMIT:
		ret = pmbus_read_word_data(client, page, phase, reg);
		if(ret < 0)
			goto abort;
		ret &= 0x07ff;
		break;
	default:
		ret = -ENODATA;
		break;
	}

abort:
	return ret;
}

static int stpddc60_write_word_data(struct i2c_client *client, int page, int reg,
				  u16 word)
{
	int ret;

	if (page > 0)
		return -ENXIO;

	switch (reg) {
	case PMBUS_VOUT_OV_FAULT_LIMIT:
		dev_notice(&client->dev, "Vout overvoltage limit is readonly\n");
		ret = -EACCES;
		break;
	case PMBUS_VOUT_UV_FAULT_LIMIT:
		dev_notice(&client->dev, "Vout undervoltage limit is readonly\n");
		ret = -EACCES;
		break;
	default:
		ret = -ENODATA;
		break;
	}

	return ret;
}

static int stpddc60_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int status;
	u8 device_id[I2C_SMBUS_BLOCK_MAX + 1];
	const struct i2c_device_id *mid;
	struct pmbus_driver_info *info = &stpddc60_info;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_BYTE_DATA
				     | I2C_FUNC_SMBUS_BLOCK_DATA))
		return -ENODEV;

	status = i2c_smbus_read_block_data(client, PMBUS_MFR_MODEL, device_id);
	if (status < 0) {
		dev_err(&client->dev, "Failed to read Manufacturer Model\n");
		return status;
	}
	for (mid = stpddc60_id; mid->name[0]; mid++) {
		if (!strncasecmp(mid->name, device_id, strlen(mid->name)))
			break;
	}
	if (!mid->name[0]) {
		dev_err(&client->dev, "Unsupported device\n");
		return -ENODEV;
	}

	info->read_byte_data = stpddc60_read_byte_data;
	info->read_word_data = stpddc60_read_word_data;
	info->write_word_data = stpddc60_write_word_data;

	return pmbus_do_probe(client, info);
}

static struct i2c_driver stpddc60_driver = {
	.driver = {
		   .name = "stpddc60",
		   },
	.probe = stpddc60_probe,
	.remove = pmbus_do_remove,
	.id_table = stpddc60_id,
};

module_i2c_driver(stpddc60_driver);

MODULE_AUTHOR("Erik Rosen <erik.rosen@metormote.com>");
MODULE_DESCRIPTION("PMBus driver for ST STPDDC60");
MODULE_LICENSE("GPL");