// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Hardware monitoring driver for PIM4328, PIM4820 and PIM4006
 *
 * Copyright (c) 2020 Flex AB
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/pmbus.h>
#include "pmbus.h"

enum chips { pim4328, pim4820, pim4006 };

struct pim4328_data {
	enum chips id;
	struct pmbus_driver_info info;
};
#define to_pim4328_data(x)  container_of(x, struct pim4328_data, info)

/* PIM4328 */
#define PIM4328_MFR_READ_VINA		0xd3
#define PIM4328_MFR_READ_VINB		0xd4
#define PIM4328_MFR_STATUS_BITS    	0xd5

/* PIM4006 */
#define PIM4328_MFR_READ_IINA		0xd6
#define PIM4328_MFR_READ_IINB		0xd7
#define PIM4328_MFR_FET_CHECKSTATUS     0xd9

/* PIM 4820 */
#define PIM4328_MFR_READ_STATUS    	0xd0

static const struct i2c_device_id pim4328_id[] = {
	{"pim4328", pim4328},
	{"pim4820", pim4820},
	{"pim4006", pim4006},
	{"pim4106", pim4006},
	{"pim4206", pim4006},
	{"pim4306", pim4006},
	{"pim4406", pim4006},
	{"bmr455", pim4328},
	{}
};
MODULE_DEVICE_TABLE(i2c, pim4328_id);

static int pim4328_read_word_data(struct i2c_client *client, int page,
				 int phase, int reg)
{
	const struct pmbus_driver_info *info = pmbus_get_driver_info(client);
	struct pim4328_data *data = to_pim4328_data(info);
	int ret, status;

	if (page > 0)
		return -ENXIO;

	switch (reg) {
	case PMBUS_STATUS_WORD:
		ret = pmbus_read_byte_data(client, page, PMBUS_STATUS_BYTE);
		if (ret >= 0) {
			if (data->id == pim4006) {
				status = pmbus_read_word_data(client, page, 0xff,
						PIM4328_MFR_FET_CHECKSTATUS);
				if (status > 0 && (status & 0x0030))
					ret |= 0x08;
			}
			else if (data->id == pim4328) {
				status = pmbus_read_byte_data(client, page,
					PIM4328_MFR_STATUS_BITS);
				if (status > 0) {
					if (status & 0x04)
						ret |= 0x08;
					if (status & 0x40)
						ret |= 0x80;
				}
			}
			else if (data->id == pim4820) {
				status = pmbus_read_byte_data(client, page,
					PIM4328_MFR_READ_STATUS);
				if (status > 0) {
					if (status & 0x05)
						ret |= 0x2001;
					if (status & 0x02)
						ret |= 0x0008;
					if (status & 0x40)
						ret |= 0x0004;
				}
			}
		}
		break;
	case PMBUS_READ_VIN:
		if (phase != 0xff) {
			ret = pmbus_read_word_data(client, page, phase,
				phase == 0 ? PIM4328_MFR_READ_VINA : PIM4328_MFR_READ_VINB);
		}
		else
			ret = -ENODATA;
		break;
	case PMBUS_READ_IIN:
		if (phase != 0xff) {
			ret = pmbus_read_word_data(client, page, phase,
				phase == 0 ? PIM4328_MFR_READ_IINA : PIM4328_MFR_READ_IINB);
		}
		else
			ret = -ENODATA;
		break;
	default:
		ret = -ENODATA;
	}

	return ret;
}

static int pim4328_read_byte_data(struct i2c_client *client, int page, int reg)
{
	const struct pmbus_driver_info *info = pmbus_get_driver_info(client);
	struct pim4328_data *data = to_pim4328_data(info);
	int ret, status;

	if (page > 0)
		return -ENXIO;

	switch (reg) {
	case PMBUS_STATUS_BYTE:
		ret = pim4328_read_word_data(client, page, 0xff, PMBUS_STATUS_WORD);
		if (ret > 0)
			ret &= 0xff;
		break;
	case PMBUS_STATUS_INPUT:
		if (data->id == pim4820) {
			ret = 0;
			status = pmbus_read_byte_data(client, page,
				PIM4328_MFR_READ_STATUS);
			if (status > 0) {
				if (status & 0x01)
					ret |= 0x80;
				if (status & 0x02)
					ret |= 0x10;
				if (status & 0x04)
					ret |= 0x04;
			}
		}
		else {
			ret = -ENXIO;
		}
		break;
	default:
		ret = -ENODATA;
	}

	return ret;
}

static int pim4328_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int status;
	u8 device_id[I2C_SMBUS_BLOCK_MAX + 1];
	const struct i2c_device_id *mid;
	struct pim4328_data *data;
	struct pmbus_driver_info *info;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_BYTE_DATA
				     | I2C_FUNC_SMBUS_BLOCK_DATA))
		goto abort;

	data = devm_kzalloc(&client->dev, sizeof(struct pim4328_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	status = i2c_smbus_read_block_data(client, PMBUS_MFR_MODEL, device_id);
	if (status < 0) {
		dev_err(&client->dev, "Failed to read Manufacturer Model\n");
		goto abort;
	}
	for (mid = pim4328_id; mid->name[0]; mid++) {
		if (!strncasecmp(mid->name, device_id, strlen(mid->name)))
			break;
	}
	if (!mid->name[0]) {
		dev_err(&client->dev, "Unsupported device\n");
		goto abort;
	}

	if (id->driver_data != mid->driver_data)
		dev_notice(&client->dev,
			   "Device mismatch: Configured %s, detected %s\n",
			   id->name, mid->name);

	data->id = mid->driver_data;
	info = &data->info;
	info->pages = 1;
	info->read_byte_data = pim4328_read_byte_data;
	info->read_word_data = pim4328_read_word_data;

	switch(data->id) {
		case pim4820:
			info->format[PSC_VOLTAGE_IN] = direct;
			info->func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_TEMP
				| PMBUS_HAVE_IIN | PMBUS_HAVE_STATUS_INPUT;
			break;
		case pim4328:
			info->phases[0] = 2;
			info->format[PSC_VOLTAGE_IN] = direct;
			info->func[0] = PMBUS_HAVE_VCAP | PMBUS_HAVE_VIN
				| PMBUS_HAVE_TEMP | PMBUS_HAVE_IOUT;
			info->pfunc[0] = PMBUS_HAVE_VIN;
			info->pfunc[1] = PMBUS_HAVE_VIN;
			break;
		case pim4006:
			info->phases[0] = 2,
			info->format[PSC_VOLTAGE_IN] = linear;
			info->func[0] = PMBUS_PHASE_VIRTUAL | PMBUS_HAVE_VIN
				| PMBUS_HAVE_TEMP | PMBUS_HAVE_IOUT;
			info->pfunc[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_IIN;
			info->pfunc[1] = PMBUS_HAVE_VIN | PMBUS_HAVE_IIN;
			break;
		default:
			goto abort;
	}
	
	if (info->format[PSC_VOLTAGE_IN] == direct) {
		if (!i2c_check_functionality(client->adapter,
				     	I2C_FUNC_SMBUS_BLOCK_PROC_CALL))
			goto abort;

		if (info->func[0] & PMBUS_HAVE_VCAP) {
			status = pmbus_read_coefficients(client, info,
							PSC_VOLTAGE_OUT,
							PMBUS_READ_VCAP);
			if (status < 0) {
				dev_err(&client->dev,
					"Failed to read coefficients for PMBUS_READ_VCAP\n");
				goto abort;
			}
		}
		if (info->func[0] & PMBUS_HAVE_VIN) {
			status = pmbus_read_coefficients(client, info,
							PSC_VOLTAGE_IN,
							PMBUS_READ_VIN);
			if (status < 0) {
				dev_err(&client->dev,
					"Failed to read coefficients for PMBUS_READ_VIN\n");
				goto abort;
			}
		}
		if (info->func[0] & PMBUS_HAVE_IIN) {
			status = pmbus_read_coefficients(client, info,
							PSC_CURRENT_IN,
							PMBUS_READ_IIN);
			if (status < 0) {
				dev_err(&client->dev,
					"Failed to read coefficients for PMBUS_READ_IIN\n");
				goto abort;
			}
		}
		if (info->func[0] & PMBUS_HAVE_IOUT) {
			status = pmbus_read_coefficients(client, info,
							PSC_CURRENT_OUT,
							PMBUS_READ_IOUT);
			if (status < 0) {
				dev_err(&client->dev,
					"Failed to read coefficients for PMBUS_READ_IOUT\n");
				goto abort;
			}
		}
		if (info->func[0] & PMBUS_HAVE_TEMP) {
			status = pmbus_read_coefficients(client, info,
							PSC_TEMPERATURE,
							PMBUS_READ_TEMPERATURE_1);
			if (status < 0) {
				dev_err(&client->dev,
					"Failed to read coefficients for PMBUS_READ_TEMPERATURE_1\n");
				goto abort;
			}
		}
	}

	return pmbus_do_probe(client, info);
abort:
	return -ENODEV;
}

static struct i2c_driver pim4328_driver = {
	.driver = {
		   .name = "pim4328",
		   },
	.probe = pim4328_probe,
	.remove = pmbus_do_remove,
	.id_table = pim4328_id,
};

module_i2c_driver(pim4328_driver);

MODULE_AUTHOR("Erik Rosen <erik.rosen@metormote.com>");
MODULE_DESCRIPTION("PMBus driver for PIM4328, PIM4820 and PIM4006 power interface modules");
MODULE_LICENSE("GPL");