/**
 * @file crc.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-07
 * 
 * @copyright Copyright (c) 2023
 * 
 * @note 
 * 	1. Refer to "https://reveng.sourceforge.io/crc-catalogue/all.htm"
 * 	2. Refer to "http://www.metools.info/code/c15.html"
 */

#include <stdio.h>
#include <stdint.h>

#include "compiler.h"
#include "libbase.h"
#include "libcrc.h"

struct crc_instance {
	char		name[32];
	uint32_t	width;
	union {
		uint8_t		gen8;
		uint16_t	gen16;
		uint32_t	gen32;
		uint64_t	gen64;
	} poly; /**< generator polynomial */

	void		*table;
	uint32_t	nr_entry;
};

static uint8_t g_crc8_table[256] = {0};
static uint16_t g_crc16_table[256] = {0};
static uint32_t g_crc32_table[256] = {0};
static uint64_t g_crc64_table[256] = {0};

static struct crc_instance g_crc8 = {
	.name		= "CRC8",
	.width		= 8,
	.table		= g_crc8_table,
	.nr_entry	= ARRAY_SIZE(g_crc8_table),
};

static struct crc_instance g_crc16 = {
	.name		= "CRC16",
	.width		= 16,
	.table		= g_crc16_table,
	.nr_entry	= ARRAY_SIZE(g_crc16_table),
};

static struct crc_instance g_crc32 = {
	.name		= "CRC32",
	.width		= 32,
	.table		= g_crc32_table,
	.nr_entry	= ARRAY_SIZE(g_crc32_table),
};

static struct crc_instance g_crc64 = {
	.name		= "CRC64",
	.width		= 64,
	.table		= g_crc64_table,
	.nr_entry	= ARRAY_SIZE(g_crc64_table),
};

/**
 * @brief Initialize CRC-8 table
 * 
 * @param table An array for holding CRC values
 * @param idx Subtract 1 from the number of @table array members 
 * @param poly Generator polynomial
 */
static void crc8_init_table(uint8_t *table, uint8_t idx, uint8_t poly)
{
	uint8_t remainder;
	uint32_t i;
	uint8_t bit;

	for (i = 0; i <= idx; i++) {
		remainder = i; /* left shitf (crc_width - 8) bit */

		for (bit = 0; bit < 8; bit++) {
			if (remainder & BIT(7))
				remainder = (remainder << 1) ^ poly;
			else
				remainder <<= 1;

			table[i] = remainder;
		}
	}
}

/**
 * @brief Calculate CRC-8 value based on the configuration
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-8 value
 */
uint8_t crc8_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size)
{
	struct crc_instance *crc8 = &g_crc8;
	uint8_t *table = crc8->table;
	uint8_t remainder = cfg->init.crc8;
	uint32_t i;
	uint8_t byte;

	if (unlikely(!crc8->poly.gen8 || crc8->poly.gen8 != cfg->poly.gen8)) {
		crc8_init_table(crc8->table, crc8->nr_entry - 1,
			cfg->poly.gen8);
		crc8->poly.gen8 = cfg->poly.gen8;
	}

	for (i = 0; i < size; i++) {
		if (cfg->refin)
			/* right shitf (crc_width - 8) bit */
			byte = remainder ^ reverse_8bits(data[i]);
		else
			byte = remainder ^ data[i];
		
		remainder = table[byte];
	}

	if (cfg->refout)
		remainder = reverse_8bits(remainder);
	
	return remainder ^ cfg->xorout.xor8;
}

/**
 * @brief Calculate CRC-8 value by using CRC-8/MAXIM
 * 
 * 	Name	: "CRC-8/MAXIM" or "CRC-8/MAXIM-DOW"
 * 	Width	: 8
 * 	Poly	: 31h
 * 	Init	: 00h
 * 	RefIn	: True
 * 	RefOut	: True
 * 	XorOut	: 00h
 * 	Check	: A1h
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-8 value
 */
uint8_t crc8_maxim(uint8_t *data, uint32_t size)
{
	struct crc_config cfg = {0};

	cfg.name = "CRC-8/MAXIM";
	cfg.width = 8;
	cfg.poly.gen8 = 0x31;
	cfg.refin = 1;
	cfg.refout = 1;

	return crc8_calculate(&cfg, data, size);
}

/**
 * @brief Initialize CRC-16 table
 * 
 * @param table An array for holding CRC values
 * @param idx Subtract 1 from the number of @table array members 
 * @param poly Generator polynomial, eg: 0x8bb7
 * 	G(x) = x^16 + x^15 + x^11 + x^9 + x^8 + x^7 + x^5 + x^4 +x^2 + x + 1
 */
static void crc16_init_table(uint16_t *table, uint16_t idx, uint16_t poly)
{
	uint16_t remainder;
	uint32_t i;
	uint8_t bit;

	for (i = 0; i <= idx; i++) {
		remainder = i << 8; /* left shitf (crc_width - 8) bit */

		for (bit = 0; bit < 8; bit++) {
			if (remainder & BIT(15))
				remainder = (remainder << 1) ^ poly;
			else
				remainder <<= 1;
		}
		table[i] = remainder;
	}
}

/**
 * @brief Calculate CRC-16 value based on the configuration
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-16 value
 */
uint16_t crc16_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size)
{
	struct crc_instance *crc16 = &g_crc16;
	uint16_t *table = crc16->table;
	uint16_t remainder = cfg->init.crc16;
	uint32_t i;
	uint8_t byte;

	if (unlikely(!crc16->poly.gen16 || crc16->poly.gen16 != cfg->poly.gen16)) {
		crc16_init_table(crc16->table, crc16->nr_entry - 1, 
			cfg->poly.gen16);
		crc16->poly.gen16 = cfg->poly.gen16;
	}

	for (i = 0; i < size; i++) {
		if (cfg->refin)
			/* right shitf (crc_width - 8) bit */
			byte = (remainder >> 8) ^ reverse_8bits(data[i]);
		else
			byte = (remainder >> 8) ^ data[i];
		
		remainder = table[byte] ^ (remainder << 8);
	}

	if (cfg->refout)
		remainder = reverse_16bits(remainder);
	
	return remainder ^ cfg->xorout.xor16;
}

/**
 * @brief Calculate CRC-16 value by using CRC-16/USB
 * 
 *  Name	: "CRC-16/USB"
 *  Width	: 16
 * 	Poly	: 8005h
 *  Init	: FFFFh
 *  RefIn	: True
 *  RefOut	: True
 *  XorOut	: FFFFh
 *  Check	: 
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-16 value
 */
uint16_t crc16_usb(uint8_t *data, uint32_t size)
{
	struct crc_config cfg = {0};

	cfg.name = "CRC-16/USB";
	cfg.width = 16;
	cfg.poly.gen16 = 0x8005;
	cfg.init.crc16 = U16_MAX;
	cfg.xorout.xor16 = U16_MAX;
	cfg.refin = 1;
	cfg.refout = 1;

	return crc16_calculate(&cfg, data, size);
}

/**
 * @brief Calculate CRC-16 value by using CRC-16/T10-DIF
 * 
 * 	Name	: "CRC-16/T10-DIF"
 * 	Width	: 16
 * 	Poly	: 8BB7h
 * 	Init	: 0000h
 * 	RefIn	: False
 * 	RefOut	: False
 * 	XorOut	: 0000h
 * 	Check	: D0DBh
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-16 value
 * @note Refer to "SCSI Block Commands - 3, Revision 33, Table 17"
 */
uint16_t crc16_t10_dif(uint8_t *data, uint32_t size)
{
	struct crc_config cfg = {0};

	cfg.name = "CRC-16/T10-DIF";
	cfg.width = 16;
	cfg.poly.gen16 = 0x8bb7;

	return crc16_calculate(&cfg, data, size);
}

/**
 * @brief Initialize CRC-32 table
 * 
 * @param table An array for holding CRC values
 * @param idx Subtract 1 from the number of @table array members 
 * @param poly Generator polynomial
 */
static void crc32_init_table(uint32_t *table, uint32_t idx, uint32_t poly)
{
	uint32_t remainder;
	uint32_t i;
	uint8_t bit;

	for (i = 0; i <= idx; i++) {
		remainder = i << 24; /* left shitf (crc_width - 8) bit */

		for (bit = 0; bit < 8; bit++) {
			if (remainder & BIT(31))
				remainder = (remainder << 1) ^ poly;
			else
				remainder <<= 1;
		}
		table[i] = remainder;
	}
}

/**
 * @brief Calculate CRC-32 value based on the configuration
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-32 value
 */
uint32_t crc32_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size)
{
	struct crc_instance *crc32 = &g_crc32;
	uint32_t *table = crc32->table;
	uint32_t remainder = cfg->init.crc32;
	uint32_t i;
	uint8_t byte;

	if (unlikely(!crc32->poly.gen32 || crc32->poly.gen32 != cfg->poly.gen32)) {
		crc32_init_table(crc32->table, crc32->nr_entry - 1, 
			cfg->poly.gen32);
		crc32->poly.gen32 = cfg->poly.gen32;
	}

	for (i = 0; i < size; i++) {
		if (cfg->refin)
			/* right shitf (crc_width - 8) bit */
			byte = (remainder >> 24) ^ reverse_8bits(data[i]);
		else
			byte = (remainder >> 24) ^ data[i];
		
		remainder = table[byte] ^ (remainder << 8);
	}

	if (cfg->refout)
		remainder = reverse_32bits(remainder);
	
	return remainder ^ cfg->xorout.xor32;
}

/**
 * @brief Calculate CRC-32 value by using CRC-32C (Castagnoli)
 * 
 * 	Name	: "CRC-32C"
 * 	Width	: 32
 * 	Poly	: 1EDC6F41h
 * 	Init	: FFFFFFFFh
 * 	RefIn	: True
 * 	RefOut	: True
 * 	XorOut	: FFFFFFFFh
 * 	Check	: E3069283h
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-32 value
 * @note Refer to "NVM Express Managment Interface Revision 1.2b - ch3.1.1.1"
 */
uint32_t crc32_castagnoli(uint8_t *data, uint32_t size)
{
	struct crc_config cfg = {0};

	cfg.name = "CRC-32C";
	cfg.width = 32;
	cfg.poly.gen32 = 0x1edc6f41;
	cfg.init.crc32 = U32_MAX;
	cfg.xorout.xor32 = U32_MAX;
	cfg.refin = 1;
	cfg.refout = 1;

	return crc32_calculate(&cfg, data, size);
}

/**
 * @brief Initialize CRC-64 table
 * 
 * @param table An array for holding CRC values
 * @param idx Subtract 1 from the number of @table array members 
 * @param poly Generator polynomial
 */
static void crc64_init_table(uint64_t *table, uint64_t idx, uint64_t poly)
{
	uint64_t remainder;
	uint64_t i;
	uint8_t bit;

	for (i = 0; i <= idx; i++) {
		remainder = i << 56; /* left shitf (crc_width - 8) bit */

		for (bit = 0; bit < 8; bit++) {
			if (remainder & BIT_ULL(63))
				remainder = (remainder << 1) ^ poly;
			else
				remainder <<= 1;
		}
		table[i] = remainder;
	}
}

/**
 * @brief Calculate CRC-64 value based on the configuration
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-64 value
 */
uint64_t crc64_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size)
{
	struct crc_instance *crc64 = &g_crc64;
	uint64_t *table = crc64->table;
	uint64_t remainder = cfg->init.crc64;
	uint32_t i;
	uint8_t byte;

	if (unlikely(!crc64->poly.gen64 || crc64->poly.gen64 != cfg->poly.gen64)) {
		crc64_init_table(crc64->table, crc64->nr_entry - 1,
			cfg->poly.gen64);
		crc64->poly.gen64 = cfg->poly.gen64;
	}

	for (i = 0; i < size; i++) {
		if (cfg->refin)
			/* right shitf (crc_width - 8) bit */
			byte = (remainder >> 56) ^ reverse_8bits(data[i]);
		else
			byte = (remainder >> 56) ^ data[i];
		
		remainder = table[byte] ^ (remainder << 8);
	}

	if (cfg->refout)
		remainder = reverse_64bits(remainder);
	
	return remainder ^ cfg->xorout.xor64;
}

/**
 * @brief Calculate CRC-64 value by using NVM Express 64b CRC
 * 
 * 	Name	: "NVM Express 64b CRC"
 * 	Width	: 64
 * 	Poly	: AD93D235_94C93659h
 * 	Init	: FFFFFFFF_FFFFFFFFh
 * 	RefIn	: True
 * 	RefOut	: True
 * 	XorOut	: FFFFFFFF_FFFFFFFFh
 * 	Check	: 11199E50_6128D175h
 * 
 * @param data Data to be calculated
 * @param size @data size
 * @return The calculated CRC-64 value
 * @note Refer to "NVM Command Set Specification Revision 1.0b - ch5.2.1.3.4"
 */
uint64_t crc64_nvme64bcrc(uint8_t *data, uint32_t size)
{
	struct crc_config cfg = {0};

	cfg.name = "NVM Express 64b CRC";
	cfg.width = 64;
	cfg.poly.gen64 = 0xad93d23594c93659;
	cfg.init.crc64 = U64_MAX;
	cfg.xorout.xor64 = U64_MAX;
	cfg.refin = 1;
	cfg.refout = 1;

	return crc64_calculate(&cfg, data, size);
}
