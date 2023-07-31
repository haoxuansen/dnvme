/**
 * @file test_cmd.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"
#include "test_metrics.h"

struct source_range {
	uint64_t	slba;
	uint32_t	nlb;
};

/*
 * @note
 *	I: Required to initialize
 * 	R: Return value
 */
struct test_cmd_copy {
	uint64_t	slba; 		/* I: target */
	uint16_t	status; 	/* R: save status code */

	uint8_t		desc_fmt;	/* I: descriptor format */
	void		*desc;
	uint32_t	size;

	uint32_t	ranges;		/* I */
	struct source_range	entry[0];	/* I */
};

static bool is_support_copy(uint16_t oncs)
{
	return (oncs & NVME_CTRL_ONCS_COPY) ? true : false;
}

static int create_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->size;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	ccq_wrap.contig = 1;

	ret = nvme_create_iocq(ndev, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->size;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	ret = nvme_create_iosq(ndev, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	return 0;
}

static int delete_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	int ret;

	ret = nvme_delete_iosq(ndev, sq->sqid);
	if (ret < 0) {
		pr_err("failed to delete iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	ret = nvme_delete_iocq(ndev, cq->cqid);
	if (ret < 0) {
		pr_err("failed to delete iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	return 0;
}

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint16_t control)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;
	wrap.control = control;

	BUG_ON(wrap.size > tool->rbuf_size);

	return nvme_io_read(ndev, &wrap);
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint16_t control)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	uint32_t i;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;
	wrap.control = control;

	BUG_ON(wrap.size > tool->wbuf_size);

	for (i = 0; i < (wrap.size / 4); i+= 4) {
		*(uint32_t *)(wrap.buf + i) = (uint32_t)rand();
	}

	return nvme_io_write(ndev, &wrap);
}

static int send_io_compare_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	BUG_ON(wrap.size > tool->rbuf_size);

	return nvme_io_compare(ndev, &wrap);
}

static int copy_desc_fmt0_init(struct nvme_copy_desc_fmt0 *desc, 
	struct source_range *range, uint32_t num)
{
	uint32_t i;

	memset(desc, 0, sizeof(*desc) * num);
	for (i = 0; i < num; i++) {
		desc[i].slba = cpu_to_le64(range[i].slba);
		desc[i].length = cpu_to_le16(range[i].nlb);
	}
	return 0;
}

static int copy_desc_fmt1_init(struct nvme_copy_desc_fmt1 *desc,
	struct source_range *range, uint32_t num)
{
	uint32_t i;

	memset(desc, 0, sizeof(*desc) * num);
	for (i = 0; i < num; i++) {
		desc[i].slba = cpu_to_le64(range[i].slba);
		desc[i].length = cpu_to_le16(range[i].nlb);
	}
	return 0;
}

static int copy_desc_init(struct test_cmd_copy *copy)
{
	void *desc;
	int ret;

	switch (copy->desc_fmt) {
	case NVME_COPY_DESC_FMT_32B:
	default:
		ret = posix_memalign(&desc, CONFIG_UNVME_RW_BUF_ALIGN, 
			sizeof(struct nvme_copy_desc_fmt0) * copy->ranges);
		if (ret) {
			pr_err("failed to alloc copy descriptor!\n");
			return -ENOMEM;
		}
		copy_desc_fmt0_init(desc, copy->entry, copy->ranges);
		copy->size = sizeof(struct nvme_copy_desc_fmt0) * copy->ranges;
		break;
	
	case NVME_COPY_DESC_FMT_40B:
		ret = posix_memalign(&desc, CONFIG_UNVME_RW_BUF_ALIGN,
			sizeof(struct nvme_copy_desc_fmt1) * copy->ranges);
		if (ret) {
			pr_err("failed to alloc copy descriptor!\n");
			return -ENOMEM;
		}
		copy_desc_fmt1_init(desc, copy->entry, copy->ranges);
		copy->size = sizeof(struct nvme_copy_desc_fmt1) * copy->ranges;
		break;
	}

	copy->desc = desc;
	return 0;
}

static void copy_desc_exit(struct test_cmd_copy *copy)
{
	if (!copy || !copy->desc)
		return;

	free(copy->desc);
	copy->desc = NULL;
	copy->size = 0;
}

static int send_io_copy_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	struct test_cmd_copy *copy)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_copy_wrapper wrap = {0};
	uint32_t i;
	uint16_t cid;
	int ret;

	/*
	 * Avoid the read portion of a copy operation attemps to access
	 * a deallocated or unwritten logical block.
	 */
	for (i = 0; i < copy->ranges; i++) {
		ret = send_io_write_cmd(tool, sq, copy->entry[i].slba, 
			copy->entry[i].nlb, 0);
		if (ret < 0) {
			pr_err("failed to write NLB(%u) to SLBA(0x%lx)!(%d)\n",
				copy->entry[i].nlb, copy->entry[i].slba, ret);
			return ret;
		}
	}

	ret = copy_desc_init(copy);
	if (ret < 0)
		return ret;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = copy->slba;
	wrap.ranges = copy->ranges - 1;
	wrap.desc_fmt = copy->desc_fmt;
	wrap.desc = copy->desc;
	wrap.size = copy->size;

	ret = nvme_cmd_io_copy(ndev->fd, &wrap);
	if (ret < 0) {
		pr_err("failed to submit copy command!(%d)\n", ret);
		goto out;
	}
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, 1, tool->entry, 
		tool->entry_size);
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	if (tool->entry[0].command_id != cid) {
		pr_err("CID:%u vs %u\n", cid, tool->entry[0].command_id);
		ret = -EINVAL;
		goto out;
	}
	copy->status = NVME_CQE_STATUS_TO_STATE(tool->entry[0].status);
out:
	copy_desc_exit(copy);
	return ret;
}

int case_cmd_io_read(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 255 + 1;
	int ret;

	pr_debug("READ LBA:0x%llx + %u\n", slba, nlb);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_read_cmd(tool, sq, slba, nlb, 0);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out;
	}
out:
	ret = delete_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

int case_cmd_io_read_with_fua(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 255 + 1;
	int ret;

	pr_debug("READ LBA:0x%llx + %u\n", slba, nlb);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_read_cmd(tool, sq, slba, nlb, NVME_RW_FUA);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out;
	}
out:
	ret = delete_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

int case_cmd_io_write(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 255 + 1;
	int ret;

	pr_debug("WRITE LBA:0x%llx + %u\n", slba, nlb);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb, 0);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	ret = delete_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

int case_cmd_io_write_with_fua(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 255 + 1;
	int ret;

	pr_debug("WRITE LBA:0x%llx + %u\n", slba, nlb);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb, NVME_RW_FUA);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	ret = delete_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

int case_cmd_io_compare(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	uint64_t slba = 0;
	uint32_t nlb = 8;
	uint32_t size;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb, 0);
	ret |= send_io_read_cmd(tool, sq, slba, nlb, 0);
	ret |= send_io_compare_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to compare data!(%d)\n", ret);
		goto out;
	}

	/* check again */
	size = ndev->nss[0].lbads * nlb;
	ret = memcmp(tool->wbuf, tool->rbuf, size);
	if (ret != 0) {
		pr_err("failed to compare read/write buffer!\n");
		ret = -EIO;
		goto out;
	}
out:
	ret = delete_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * @brief Copy command shall process success
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_copy_success(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_id_ns *id_ns = &ndev->nss[0].id_ns;
	struct test_cmd_copy *copy = NULL;
	uint32_t sum = 0;
	uint32_t entry;
	uint32_t i;
	uint32_t mcl;
	uint16_t msrc;
	uint16_t mssrl;
	uint16_t limit = 256;
	int ret = -ENOMEM;

	msrc = (uint16_t)id_ns->msrc + 1;
	mssrl = le16_to_cpu(id_ns->mssrl);
	mcl = le32_to_cpu(id_ns->mcl);

	entry = rand() % msrc + 1; /* 1~msrc */

	copy = calloc(1, sizeof(struct test_cmd_copy) +
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		goto out;
	}

	copy->ranges = entry;
	copy->slba = rand() % (ndev->nss[0].nsze / 4) + (ndev->nss[0].nsze / 4);
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	do {
		if (sum)
			pr_warn("NLB total is larger than MCL! try again?");

		for (i = 0, sum = 0; i < copy->ranges; i++) {
			copy->entry[i].slba = rand() % (ndev->nss[0].nsze / 4);
			copy->entry[i].nlb = rand() % min(mssrl, limit) + 1;
			sum += copy->entry[i].nlb;
		}
	} while (mcl < sum);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;
	
	if (copy->status != NVME_SC_SUCCESS) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SUCCESS, copy->status);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to invalid descriptor format
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the Copy Descriptor Format specified in the Descriptor Format
 *  field is not supported by the controller, then the command shall
 *  be aborted with a status code of Invalid Field in Command.
 */
static int subcase_copy_invalid_desc_format(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_cmd_copy *copy = NULL;
	int ret = -ENOMEM;

	copy = calloc(1, sizeof(struct test_cmd_copy) + 
				sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		goto out;
	}
	copy->ranges = 1;
	copy->slba = rand() % (ndev->nss[0].nsze / 4) + (ndev->nss[0].nsze / 4);
	/* set invlid descriptor format */
	copy->desc_fmt = 0xff;

	copy->entry[0].slba = rand() % (ndev->nss[0].nsze / 4);
	copy->entry[0].nlb = 1;

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_INVALID_FIELD) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_INVALID_FIELD, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to mismatch descriptor format
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note The command shall be aborted with the status code of Invalid 
 *  Namespace or Format if one of the following condtion is met.
 * 	1) 16b Guard PI(Protection Info) && Descriptor Format != 0h
 * 	2) 32b or 64b Guard PI && Descriptor Format != 1h
 */
static int __unused subcase_copy_mismatch_desc_format(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	return -EOPNOTSUPP;
}

/**
 * @brief Copy command shall process failed due to invalid range num
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the number of Source Range entries is greater than the 
 *  value in the MSRC field, then the Copy command shall be aborted
 *  with a status code of Command Size Limit Exceeded.
 */
static int subcase_copy_invalid_range_num(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_id_ns *id_ns = &ndev->nss[0].id_ns;
	struct test_cmd_copy *copy = NULL;
	uint32_t sum = 0;
	uint32_t entry;
	uint32_t i;
	uint32_t mcl;
	uint16_t mssrl;
	uint16_t limit = 256;
	int ret = -EOPNOTSUPP;

	/*
	 * NR field in copy command is 8-bit width, the max value is 0xff.
	 * If MSRC is 0xff, we can't set NR larger!
	 */
	if (id_ns->msrc == 0xff) {
		pr_warn("Max source range count is 0xff! skip...\n");
		goto out;
	}
	mssrl = le16_to_cpu(id_ns->mssrl);
	mcl = le32_to_cpu(id_ns->mcl);

	/* set invalid range num */
	entry = (uint32_t)id_ns->msrc + 2;

	copy = calloc(1, sizeof(struct test_cmd_copy) +
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	copy->ranges = entry;
	copy->slba = rand() % (ndev->nss[0].nsze / 4) + (ndev->nss[0].nsze / 4);
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	do {
		if (sum)
			pr_warn("NLB total is larger than MCL! try again?");

		for (i = 0, sum = 0; i < copy->ranges; i++) {
			copy->entry[i].slba = rand() % (ndev->nss[0].nsze / 4);
			copy->entry[i].nlb = rand() % min(mssrl, limit) + 1;
			sum += copy->entry[i].nlb;
		}
	} while (mcl < sum);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_SIZE_EXCEEDED) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SIZE_EXCEEDED, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to invalid NLB
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If a valid Source Range Entry specifies a Number of 
 *  Logical Blocks field that is greater than the value in the 
 *  MSSRL field, the the Copy command shall be aborted with a
 *  status code of Command Size Limit Exceeded.
 */
static int subcase_copy_invalid_nlb_single(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_id_ns *id_ns = &ndev->nss[0].id_ns;
	struct test_cmd_copy *copy = NULL;
	uint32_t sum = 0;
	uint32_t inject;
	uint32_t dw0;
	uint32_t entry;
	uint32_t i;
	uint32_t mcl;
	uint16_t msrc;
	uint16_t mssrl;
	uint16_t limit = 256;
	int ret = -EOPNOTSUPP;

	msrc = (uint16_t)id_ns->msrc + 1;
	mssrl = le16_to_cpu(id_ns->mssrl);
	mcl = le32_to_cpu(id_ns->mcl);

	/*
	 * NLB field in copy descriptor is 16-bit width, the max value 
	 * is 0xffff. If MSSRL is 0xffff, we can't set NLB larger!
	 */
	if (mssrl == 0xffff) {
		pr_warn("Max single source range length is 0xffff! skip...\n");
		goto out;
	}

	entry = rand() % msrc + 1; /* 1~msrc */

	copy = calloc(1, sizeof(struct test_cmd_copy) + 
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	copy->ranges = entry;
	copy->slba = rand() % (ndev->nss[0].nsze / 4) + (ndev->nss[0].nsze / 4);
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	do {
		if (sum)
			pr_warn("NLB total is larger than MCL! try again?");

		for (i = 0, sum = 0; i < copy->ranges; i++) {
			copy->entry[i].slba = rand() % (ndev->nss[0].nsze / 4);
			copy->entry[i].nlb = rand() % min(mssrl, limit) + 1;
			sum += copy->entry[i].nlb;
		}
	} while (mcl < sum);

	inject = rand() % entry;
	/* set invalid NLB in random source range */
	copy->entry[inject].nlb = mssrl + 1;

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_SIZE_EXCEEDED) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SIZE_EXCEEDED, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

	/*
	 * Dword 0 of the completion queue entry contains the number of 
	 * the lowest numbered Source Range entry that was not successfully
	 * copied.
	 */
	dw0 = le32_to_cpu(tool->entry[0].result.u32);
	if (dw0 != inject) {
		pr_err("expect dw0 is %u, actual dw0 is %u!\n", inject, dw0);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to invalid NLB sum
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the sum of all Number of Logical Blocks fields in all
 *  Source Range entries is greater than the value in the MCL field,
 *  then the Copy command shall be aborted with a status of Command
 *  Size Limit Exceeded.
 */
static int subcase_copy_invalid_nlb_sum(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_id_ns *id_ns = &ndev->nss[0].id_ns;
	struct test_cmd_copy *copy = NULL;
	uint32_t sum;
	uint32_t entry;
	uint32_t i;
	uint32_t mcl;
	uint16_t msrc;
	uint16_t mssrl;
	int ret = -EOPNOTSUPP;

	msrc = (uint16_t)id_ns->msrc + 1;
	mssrl = le16_to_cpu(id_ns->mssrl);
	mcl = le32_to_cpu(id_ns->mcl);

	sum = (uint32_t)msrc * (uint32_t)mssrl;
	if (sum <= mcl) {
		pr_warn("(MSRC + 1) * MSSRL <= MCL : 0x%x vs 0x%x! skip...\n", 
			sum, mcl);
		goto out;
	}
	entry = msrc;

	copy = calloc(1, sizeof(struct test_cmd_copy) + 
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	copy->ranges = entry;
	copy->slba = rand() % (ndev->nss[0].nsze / 4) + (ndev->nss[0].nsze / 4);
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	for (i = 0; i < copy->ranges; i++) {
		copy->entry[i].slba = rand() % (ndev->nss[0].nsze / 4);
		/* ensure the sum of NLB greater than MCL */
		copy->entry[i].nlb = mssrl;
	}

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_SIZE_EXCEEDED) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SIZE_EXCEEDED, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

int case_cmd_io_copy(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_id_ctrl *id_ctrl = &ndev->id_ctrl;
	struct nvme_id_ns *id_ns = &ndev->nss[0].id_ns;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret;

	if (!is_support_copy(le16_to_cpu(id_ctrl->oncs))) {
		pr_warn("controller not support copy command!\n");
		return -EOPNOTSUPP;
	}

	if (!le16_to_cpu(id_ns->mssrl) || !le32_to_cpu(id_ns->mcl)) {
		pr_err("MSSRL:0x%x or MCL:0x%x is invalid!\n", 
			le16_to_cpu(id_ns->mssrl), le32_to_cpu(id_ns->mcl));
		return -EINVAL;
	}

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret |= subcase_copy_success(tool, sq);
	ret |= subcase_copy_invalid_desc_format(tool, sq);
	ret |= subcase_copy_invalid_range_num(tool, sq);
	ret |= subcase_copy_invalid_nlb_single(tool, sq);
	ret |= subcase_copy_invalid_nlb_sum(tool, sq);

	nvme_display_subcase_result();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
