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

#include "sizes.h"
#include "compiler.h"
#include "libnvme.h"

#define NVME_TOOL_CQ_ENTRY_SIZE		SZ_1M /* CQES(16) * elements(64K) */
#define NVME_TOOL_SQ_BUF_SIZE		SZ_4M /* SQES(64) * elements(64K) */
#define NVME_TOOL_CQ_BUF_SIZE		SZ_1M /* CQES(16) * elements(64K) */
#define NVME_TOOL_RW_BUF_SIZE		SZ_2M
#define NVME_TOOL_RW_META_SIZE		SZ_1M

#define NVME_SEC_CASE(group)		__section(".nvme.case."#group)
#define NVME_SEC_AUTOCASE(group)	__section(".nvme.autocase."#group)

#define __NVME_CASE_SYMBOL(_fname, _desc, _group)	\
static struct nvme_case __used				\
	NVME_SEC_CASE(_group)				\
	_nvme_case_##_fname = {				\
		.name	= #_fname,			\
		.desc	= _desc,			\
		.func	= _fname			\
	}

/* Group: case will placed at the head of the list  */
#define NVME_CASE_HEAD_SYMBOL(fname, desc)	__NVME_CASE_SYMBOL(fname, desc, 0)
/* Group: default */
#define NVME_CASE_SYMBOL(fname, desc)		__NVME_CASE_SYMBOL(fname, desc, 1)
/* Group: command */
#define NVME_CASE_CMD_SYMBOL(fname, desc)	__NVME_CASE_SYMBOL(fname, desc, 2)
/* Group: queue */
#define NVME_CASE_QUEUE_SYMBOL(fname, desc)	__NVME_CASE_SYMBOL(fname, desc, 3)
/* Group: power managment */
#define NVME_CASE_PM_SYMBOL(fname, desc)	__NVME_CASE_SYMBOL(fname, desc, 4)
/* Group: meta data */
#define NVME_CASE_META_SYMBOL(fname, desc)	__NVME_CASE_SYMBOL(fname, desc, 5)

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
};

typedef int (*case_func_t)(struct nvme_tool *tool);

struct nvme_case {
	const char	*name;
	const char	*desc;
	case_func_t	func;
	void		*data; /* Rsvd, for section align */
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

#endif /* !_APP_TEST_H_ */

