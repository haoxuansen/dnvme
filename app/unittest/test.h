/**
 * @file test.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_TEST_H_
#define _APP_TEST_H_

#include <stdint.h>
#include <stdarg.h>

#include "compiler.h"
#include "libbase.h"
#include "libnvme.h"
#include "libjson.h"

#define NVME_TOOL_RW_BUF_ALIGN		8

#define NVME_TOOL_CQ_ENTRY_SIZE		SZ_1M /* CQES(16) * elements(64K) */
#define NVME_TOOL_SQ_BUF_SIZE		SZ_4M /* SQES(64) * elements(64K) */
#define NVME_TOOL_CQ_BUF_SIZE		SZ_1M /* CQES(16) * elements(64K) */
#define NVME_TOOL_RW_BUF_SIZE		SZ_4M
#define NVME_TOOL_RW_META_SIZE		SZ_1M

#define NVME_TOOL_LOG_FILE_PATH		"./log"

/* Support case group from 0~9 */
#define NVME_SEC_CASE(group)		__section(".nvme.case."#group)
#define NVME_SEC_AUTOCASE(group)	__section(".nvme.autocase."#group)

#define __NVME_CASE_SYMBOL(_fname, _desc, _group)	\
static struct case_data __used				\
	_nvme_case_data_##_fname = {0};			\
static struct nvme_case __used				\
	NVME_SEC_CASE(_group)				\
	_nvme_case_##_fname = {				\
		.name	= #_fname,			\
		.desc	= _desc,			\
		.func	= _fname,			\
		.data	= &_nvme_case_data_##_fname,	\
	}

/* Group: case will placed at the head of the list  */
#define NVME_CASE_HEAD_SYMBOL(fname, desc)	__NVME_CASE_SYMBOL(fname, desc, 0)
/* Group: default */
#define NVME_CASE_SYMBOL(fname, desc)		__NVME_CASE_SYMBOL(fname, desc, 1)

#define __NVME_AUTOCASE_SYMBOL(_fname, _group)		\
static unsigned long __used				\
	NVME_SEC_AUTOCASE(_group)			\
	_nvme_autocase_##_fname	= (unsigned long)&_nvme_case_##_fname

/* Group: default */
#define NVME_AUTOCASE_SYMBOL(fname)		__NVME_AUTOCASE_SYMBOL(fname, 1)


struct nvme_usr_param {
	const char	*devpath;
};

struct nvme_tool {
	struct nvme_usr_param	param;
	struct nvme_dev_info	*ndev;

	struct nvme_completion	*entry;
	uint32_t		entry_size;

	void		*rbuf;
	uint32_t	rbuf_size;
	void		*wbuf;
	uint32_t	wbuf_size;

	/* For discontiguous queue */
	void		*sq_buf;
	uint32_t	sq_buf_size;
	void		*cq_buf;
	uint32_t	cq_buf_size;

	/* For SGL metadata */
	void		*meta_rbuf;
	uint32_t	meta_rbuf_size;
	void		*meta_wbuf;
	uint32_t	meta_wbuf_size;

	struct json_node	*report; /**< For auto generate test report */
};

struct config_rwc_cmd {
	void		*buf;
	uint32_t	size;
	uint32_t	nlb;
	uint16_t	dspec;
	uint8_t 	flags;
	uint16_t	control;
	uint32_t	dtype:4;

	uint32_t	use_nlb:1;
};

struct config_verify_cmd {
	uint8_t 	flags;
	uint32_t	nlb;
	uint32_t 	prinfo:4;
	
	uint32_t	use_nlb:1;
};

struct config_fused_cmd {
	uint8_t		flags;
	void		*buf1; /**< for first command */
	void		*buf2; /**< for second command */
	uint32_t	buf1_size;
	uint32_t	buf2_size;
	uint32_t	nlb;

	uint32_t	use_nlb:1;
};

struct config_copy_cmd {
	uint32_t	nr_desc;
	uint16_t	dspec;
	uint8_t 	flags;
	uint8_t 	fmt;
	uint32_t 	prinfor:4;
	uint32_t 	prinfow:4;
	uint32_t	dtype:4;
	
	uint32_t	use_desc:1;
	uint32_t	use_fmt:1;
};

struct config_zone_append_cmd {
	void		*buf;
	uint32_t	size;
	uint16_t	piremap:1;
};

struct case_config_effect {
	uint32_t	nsid;

	uint32_t	check_none:1;
	uint32_t	inject_nsid:1;
	struct {
		struct config_rwc_cmd	read;
		struct config_rwc_cmd	write;
		struct config_rwc_cmd	compare;
		struct config_verify_cmd	verify;
		struct config_copy_cmd	copy;
		struct config_fused_cmd	fused;
		struct config_zone_append_cmd	zone_append;
	} cmd;
	struct {
		uint32_t	nsid;
	} inject;
};

struct case_config {
	struct json_node		*origin;
	struct case_config_effect	*effect; /**< Init and access in case context */
};

struct case_report {
	struct json_node	*root;
	struct json_node	*child; /**< Current selected subcase */

	unsigned int		ctx:2;
#define UT_RPT_CTX_CASE		0
#define UT_RPT_CTX_SUBCASE	1
	unsigned int		rcd_pause:1; /**< Pause record data */
};

struct case_data {
	struct case_config	cfg;
	struct case_report	rpt;
	struct nvme_tool	*tool;
};

typedef int (*case_func_t)(struct nvme_tool *tool, struct case_data *data);

struct nvme_case {
	const char		*name;
	const char		*desc;
	case_func_t		func;
	struct case_data	*data; /* Rsvd, for section align */
};

extern struct nvme_tool *g_nvme_tool;
extern struct nvme_case __start_nvme_case[];
extern struct nvme_case __stop_nvme_case[];
extern unsigned long __start_nvme_autocase[];
extern unsigned long __stop_nvme_autocase[];

void nvme_record_case_result(const char *name, int result);
void nvme_record_subcase_result(const char *name, int result);

int nvme_display_case_report(void);
int nvme_display_subcase_report(void);

void nvme_display_test_result(int result, const char *desc);

#include "common/case.h"
#include "common/cmd.h"
#include "common/queue.h"
#include "common/record.h"

#endif /* !_APP_TEST_H_ */

