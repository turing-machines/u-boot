// SPDX-License-Identifier: GPL-2.0
/**
 * ubi-uclass.c - UBI partition and volume block device uclass driver
 *
 * Copyright (C) 2023 Sam Edwards <CFSworks@gmail.com>
 */

#define LOG_CATEGORY UCLASS_UBI

#include <common.h>
#include <blk.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <ubi_uboot.h>

static ulong ubi_bread(struct udevice *dev, lbaint_t lba, lbaint_t blkcnt,
		       void *dst)
{
	int err, lnum;
	struct blk_desc *blk = dev_get_uclass_plat(dev);
	struct ubi_device *ubi = dev_get_plat(dev->parent);
	struct ubi_volume *vol = ubi->volumes[blk->devnum];
	lbaint_t lba_per_peb = vol->usable_leb_size / blk->blksz;
	lbaint_t lba_off, lba_len, total = 0;

	while (blkcnt) {
		lnum = lba / lba_per_peb;
		lba_off = lba % lba_per_peb;
		lba_len = lba_per_peb - lba_off;
		if (lba_len > blkcnt)
			lba_len = blkcnt;

		err = ubi_eba_read_leb(ubi, vol, lnum, dst,
				       lba_off << blk->log2blksz,
				       lba_len << blk->log2blksz, 0);
		if (err) {
			pr_err("UBI read error %x\n", err);
			break;
		}

		lba += lba_len;
		blkcnt -= lba_len;
		dst += lba_len << blk->log2blksz;
		total += lba_len;
	}

	return total;
}

static const struct blk_ops ubi_block_ops = {
	.read 	= ubi_bread,
};

U_BOOT_DRIVER(ubi_block) = {
	.name	= "ubi_block",
	.id	= UCLASS_BLK,
	.ops	= &ubi_block_ops,
};

static bool is_power_of_two(unsigned int x)
{
	return (x & -x) == x;
}

static unsigned int choose_blksz_for_volume(const struct ubi_volume *vol)
{
	/*
	 * U-Boot assumes a power-of-two blksz; however, UBI LEBs are
	 * very often not suitably sized. To solve this, we divide the
	 * LEBs into a whole number of LBAs per LEB, such that each LBA
	 * addresses a power-of-two-sized block. To choose the blksz,
	 * we either:
	 * 1) Use the volume alignment, if it's a non-unity power of
	 *    two. The LEB size is a multiple of this alignment, and it
	 *    allows the user to force a particular blksz if needed for
	 *    their use case.
	 * 2) Otherwise, find the greatest power-of-two factor of the
	 *    LEB size.
	 */
	if (vol->alignment > 1 && is_power_of_two(vol->alignment))
		return vol->alignment;

	unsigned int blksz = 1;
	while ((vol->usable_leb_size & blksz) == 0)
		blksz <<= 1;
	return blksz;
}

static int ubi_post_bind(struct udevice *dev)
{
	int i;
	int ret;
	unsigned int blksz;
	lbaint_t lba;
	struct udevice *blkdev;
	struct ubi_device *ubi = dev_get_plat(dev);

	for (i = 0; i < ubi->vtbl_slots; i++) {
		struct ubi_volume *vol = ubi->volumes[i];
		if (!vol || vol->vol_id >= UBI_INTERNAL_VOL_START ||
		    vol->vol_type != UBI_STATIC_VOLUME)
			continue;

		if (vol->updating || vol->upd_marker) {
			pr_err("** UBI volume %d (\"%s\") midupdate; ignored\n",
			       vol->vol_id, vol->name);
			continue;
		}

		blksz = choose_blksz_for_volume(vol);
		lba = DIV_ROUND_UP((unsigned long long)vol->used_bytes, blksz);

		pr_debug("UBI volume %d (\"%s\"): %lu blocks, %d bytes each\n",
			 vol->vol_id, vol->name, lba, blksz);

		ret = blk_create_device(dev, "ubi_block", vol->name, UCLASS_UBI,
					vol->vol_id, blksz, lba, &blkdev);
		if (ret)
			return ret;
	}

	return 0;
}

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
	.post_bind	= ubi_post_bind,
};
