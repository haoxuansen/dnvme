/**
 * @file libcrc.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIB_CRC_H_
#define _UAPI_LIB_CRC_H_

/* refer to "crc.c" */
struct crc_config {
	char		*name;
	uint32_t	width;
	union {
		uint8_t		gen8;
		uint16_t	gen16;
		uint32_t	gen32;
		uint64_t	gen64;
	} poly; /**< Generator polynomial */

	union {
		uint8_t		crc8;
		uint16_t	crc16;
		uint32_t	crc32;
		uint64_t	crc64;
	} init; /**< Initial CRC value */

	union {
		uint8_t		xor8;
		uint16_t	xor16;
		uint32_t	xor32;
		uint64_t	xor64;
	} xorout;

	uint32_t	refin:1; /**< Reverse input value(each byte) */
	uint32_t	refout:1; /**< Reverse output value(CRC) */
};

uint8_t crc8_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size);
uint8_t crc8_maxim(uint8_t *data, uint32_t size);

uint16_t crc16_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size);
uint16_t crc16_t10_dif(uint8_t *data, uint32_t size);

uint32_t crc32_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size);
uint32_t crc32_castagnoli(uint8_t *data, uint32_t size);

uint64_t crc64_calculate(struct crc_config *cfg, uint8_t *data, uint32_t size);
uint64_t crc64_nvme64bcrc(uint8_t *data, uint32_t size);

#endif /* !_UAPI_LIB_CRC_H_ */

