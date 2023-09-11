/**
 * @file test_crc.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "compiler.h"
#include "sizes.h"
#include "libbase.h"
#include "libcrc.h"

static int __unused test_crc8_maxim(void)
{
	uint8_t data[8] = {0};
	uint8_t crc;

	data[0] = 0x11;
	data[1] = 0x22;
	data[2] = 0x33;
	data[3] = 0x44;
	crc = crc8_maxim(data, 4);
	pr_debug("CRC: %02x\n", crc);

	data[0] = 0x31;
	data[1] = 0x32;
	data[2] = 0x33;
	data[3] = 0x34;
	crc = crc8_maxim(data, 4);
	pr_debug("CRC: %02x\n", crc);
	return 0;
}

static int __unused test_crc16_custom(void)
{
	struct crc_config cfg = {0};
	uint8_t data[32] = {0};
	uint16_t crc;

	cfg.name = "CRC16";
	cfg.width = 16;
	cfg.poly.gen16 = 0x8bb7;
	cfg.init.crc16 = 0xffff;
	cfg.xorout.xor16 = 0;
	cfg.refin = 1;
	cfg.refout = 1;

	data[0] = 0x01;
	data[1] = 0x02;
	crc = crc16_calculate(&cfg, data, 2);
	pr_debug("CRC: %04x\n", crc);
	return 0;
}

/**
 * @brief Test SCSI SBC-3 CRC16
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note
 * 	1. Refer to "SCSI Block Commands - 3, Revision 33, Table 18"
 * 	2. Refer to "NVM Express NVM Command Set Specification R1.0b - 
 * 	ch5.2.1.1 16b Guard Protection Information"
 * 	   The formula used to calculate the CRC-16 is defined in SBC-3
 */
static int __unused test_crc16_scsi_sbc3(void)
{
	uint8_t data[32] = {0};
	uint16_t crc;
	uint32_t i;

	/* 32 bytes each set to 00h => 0000h */
	crc = crc16_t10_dif(data, sizeof(data));
	if (crc != 0) {
		pr_err("32 bytes each set to 00h => 0x%04x\n", crc);
		return -EPERM;
	}
	pr_debug("32 bytes each set to 00h => 0x%04x\n", crc);

	/* 32 bytes each set to FFh => A293h */
	memset(data, 0xff, sizeof(data));

	crc = crc16_t10_dif(data, sizeof(data));
	if (crc != 0xa293) {
		pr_err("32 bytes each set to FFh => 0x%04x\n", crc);
		return -EPERM;
	}
	pr_debug("32 bytes each set to FFh => 0x%04x\n", crc);

	/* 32 bytes of an incrementing pattern from 00h to 1Fh => 0224h */
	for (i = 0; i < ARRAY_SIZE(data); i++)
		data[i] = i;

	crc = crc16_t10_dif(data, sizeof(data));
	if (crc != 0x0224) {
		pr_err("32 bytes of an incrementing pattern from 00h to 1Fh"
			" => 0x%04x\n", crc);
		return -EPERM;
	}
	pr_debug("32 bytes of an incrementing pattern from 00h to 1Fh"
		" => 0x%04x\n", crc);

	/* 2 bytes each set to FFh followed by 30 bytes set to 00h => 21B8h */
	memset(data, 0, sizeof(data));
	data[0] = 0xff;
	data[1] = 0xff;

	crc = crc16_t10_dif(data, sizeof(data));
	if (crc != 0x21b8) {
		pr_err("2 bytes each set to FFh followed by 30 bytes set to 00h"
			" => 0x%04x\n", crc);
		return -EPERM;
	}
	pr_debug("2 bytes each set to FFh followed by 30 bytes set to 00h"
		" => 0x%04x\n", crc);

	/* 32 bytes of a decrementing pattern from FFh to E0h => A0B7h */
	for (i = 0; i < ARRAY_SIZE(data); i++)
		data[i] = 0xff - i;
	
	crc = crc16_t10_dif(data, sizeof(data));
	if (crc != 0xa0b7) {
		pr_err("32 bytes of a decrementing pattern from FFh to E0h"
			" => 0x%04x\n", crc);
		return -EPERM;
	}
	pr_debug("32 bytes of a decrementing pattern from FFh to E0h"
		" => 0x%04x\n", crc);
	return 0;
}

/**
 * @brief Test CRC-32C (Castagnoli)
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note
 * 	1. Refer to "NVM Express Managment Interface Revision 1.2b - 
 * 	ch3.1.1.1 Message Integrity Check"
 * 	2. Refer to "NVM Command Set Specification Revision 1.0b - Figure 117"
 */
static int __unused test_crc32_castagnoli(void)
{
	uint8_t *data = NULL;
	uint32_t crc;
	uint32_t i;
	int ret = -EPERM;

	data = zalloc(SZ_4K);
	if (!data) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	/* 4 KiB bytes each byte cleared to 00h => 98F94189h */
	crc = crc32_castagnoli(data, SZ_4K);
	if (crc != 0x98f94189) {
		pr_err("4 KiB bytes each byte cleared to 00h => 0x%08x\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes each byte cleared to 00h => 0x%08x\n", crc);

	/* 4 KiB bytes each byte set to FFh => 25C1FE13h */
	memset(data, 0xff, SZ_4K);

	crc = crc32_castagnoli(data, SZ_4K);
	if (crc != 0x25c1fe13) {
		pr_err("4 KiB bytes each byte set to FFh => 0x%08x\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes each byte set to FFh => 0x%08x\n", crc);

	/*
	 * 4 KiB bytes of an incrementing byte pattern from 00h to FFh,
	 * repeating => 9C71FE32h
	 */
	for (i = 0; i < SZ_4K; i++)
		data[i] = i & 0xff;

	crc = crc32_castagnoli(data, SZ_4K);
	if (crc != 0x9c71fe32) {
		pr_err("4 KiB bytes of an incrementing ... => 0x%08x\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes of an incrementing ... => 0x%08x\n", crc);

	/*
	 * 4 KiB bytes of a decrementing pattern from FFh to 00h, repeating
	 * => 214941A8h
	 */
	for (i = 0; i < SZ_4K; i++)
		data[i] = 0xff - (i & 0xff);
	
	crc = crc32_castagnoli(data, SZ_4K);
	if (crc != 0x214941a8) {
		pr_err("4 KiB bytes of a decrementing ... => 0x%08x\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes of a decrementing ... => 0x%08x\n", crc);

	ret = 0;
out:
	free(data);
	return ret;
}

/**
 * @brief Test NVM Express 64b CRC
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note Refer to "NVM Command Set Specification Revision 1.0b - Figure 121"
 */
static int __unused test_crc64_nvme64bcrc(void)
{
	uint8_t *data = NULL;
	uint64_t crc;
	uint32_t i;
	int ret = -EPERM;

	data = zalloc(SZ_4K);
	if (!data) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	/* 4 KiB bytes each byte cleared to 00h => 6482D367_EB22B64Eh */
	crc = crc64_nvme64bcrc(data, SZ_4K);
	if (crc != 0x6482d367eb22b64e) {
		pr_err("4 KiB bytes each byte cleared to 00h => 0x%016llx\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes each byte cleared to 00h => 0x%016llx\n", crc);

	/* 4 KiB bytes each byte set to FFh => C0DDBA73_02ECA3ACh */
	memset(data, 0xff, SZ_4K);

	crc = crc64_nvme64bcrc(data, SZ_4K);
	if (crc != 0xc0ddba7302eca3ac) {
		pr_err("4 KiB bytes each byte set to FFh => 0x%016llx\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes each byte set to FFh => 0x%016llx\n", crc);

	/*
	 * 4 KiB bytes of an incrementing byte pattern from 00h to FFh,
	 * repeating => 3E729F5F_6750449Ch
	 */
	for (i = 0; i < SZ_4K; i++)
		data[i] = i & 0xff;

	crc = crc64_nvme64bcrc(data, SZ_4K);
	if (crc != 0x3e729f5f6750449c) {
		pr_err("4 KiB bytes of an incrementing ... => 0x%016llx\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes of an incrementing ... => 0x%016llx\n", crc);

	/*
	 * 4 KiB bytes of a decrementing pattern from FFh to 00h, repeating
	 * => 9A2DF64B_8E9E517Eh
	 */
	for (i = 0; i < SZ_4K; i++)
		data[i] = 0xff - (i & 0xff);
	
	crc = crc64_nvme64bcrc(data, SZ_4K);
	if (crc != 0x9a2df64b8e9e517e) {
		pr_err("4 KiB bytes of a decrementing ... => 0x%016llx\n", crc);
		goto out;
	}
	pr_debug("4 KiB bytes of a decrementing ... => 0x%016llx\n", crc);

	ret = 0;
out:
	free(data);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret;

	ret = test_crc8_maxim();
	ret = test_crc16_scsi_sbc3();
	ret = test_crc16_custom();
	ret = test_crc32_castagnoli();
	ret = test_crc64_nvme64bcrc();

	return ret;
}
