// SPDX-License-Identifier: GPL-2.0
/**
 * ubi-uclass.c - UBI partition and volume block device uclass driver
 *
 * Copyright (C) 2023 Sam Edwards <CFSworks@gmail.com>
 */

#define LOG_CATEGORY UCLASS_UBI

#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <ubi_uboot.h>

int ubi_dm_bind(unsigned int index)
{
	struct udevice *dev;
	int ret;
	char name[16];
	const char *name_dup;
	struct ubi_device *ubi = ubi_devices[index];
	const struct mtd_info *mtd = ubi->mtd;

	/* MTD partitions are not in DM; navigate to the real MTD device */
	if (mtd->parent)
		mtd = mtd->parent;

	snprintf(name, sizeof(name), "ubi%u", index);
	name_dup = strdup(name);
	ret = device_bind(mtd->dev, DM_DRIVER_GET(ubi), name_dup, ubi,
			  ofnode_null(), &dev);
	if (ret) {
		free((void *)name_dup);
		return ret;
	}

	device_set_name_alloced(dev);

	return 0;
}

int ubi_dm_unbind_all(void)
{
	int ret;
	struct uclass *uc;
	struct udevice *dev;
	struct udevice *next;

	ret = uclass_get(UCLASS_UBI, &uc);
	if (ret)
		return ret;

	uclass_foreach_dev_safe(dev, next, uc) {
		ret = device_remove(dev, DM_REMOVE_NORMAL);
		if (ret)
			return ret;

		ret = device_unbind(dev);
		if (ret)
			return ret;
	}

	return 0;
}

U_BOOT_DRIVER(ubi) = {
	.id		= UCLASS_UBI,
	.name		= "ubi_dev",
};

UCLASS_DRIVER(ubi) = {
	.id		= UCLASS_UBI,
	.name		= "ubi",
};
