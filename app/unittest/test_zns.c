/**
 * @file test_zns.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief Test Zone Namespace
 * @version 0.1
 * @date 2023-08-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "compiler.h"
#include "libbase.h"
#include "libnvme.h"
#include "test.h"

#define TEST_ZNS_CHECK_MORE		1

struct test_data {
	uint32_t	nsid;

	/* zone descriptor extension size (covert to 1B unit) */
	uint32_t	zdes;
	uint64_t	zsze; /* unit: logical block */

	uint32_t	zdes_format_size;
	/* flags below */
	uint32_t	zdes_format_zero:1;
};

static struct test_data g_test = {0};

static int change_command_set_selected(struct nvme_dev_info *ndev, 
	uint32_t css)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;
	int ret;

	pr_notice("Change the selected I/O Command Sets to %u!\n", css);

	ret = nvme_disable_controller_complete(ndev->fd);
	if (ret < 0)
		return ret;
	
	ret = nvme_create_aq_pair(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE);
	if (ret < 0)
		return ret;

	/* the number of irq equals to (ACQ + IOCQ) */
	ret = nvme_set_irq(ndev->fd, NVME_INT_MSIX, ctrl->nr_cq + 1);
	if (ret < 0)
		return ret;
	ndev->irq_type = NVME_INT_MSIX;
	ndev->nr_irq = ctrl->nr_cq + 1;

	ret = nvme_read_ctrl_cc(ndev->fd, &prop->cc);
	if (ret < 0)
		return ret;
	
	prop->cc &= ~NVME_CC_CSS_MASK;
	prop->cc |= NVME_CC_FOR_CSS(css);
	
	ret = nvme_write_ctrl_cc(ndev->fd, prop->cc);
	if (ret < 0)
		return ret;
	
	ret = nvme_enable_controller(ndev->fd);
	if (ret < 0)
		return ret;

#if IS_ENABLED(TEST_ZNS_CHECK_MORE)
	ret = nvme_read_ctrl_cc(ndev->fd, &prop->cc);
	if (ret < 0)
		return ret;
	else if (NVME_CC_TO_CSS(prop->cc) != css) {
		pr_err("The current effect CSS is %u, expect %u!\n",
			NVME_CC_TO_CSS(prop->cc), css);
		return -EPERM;
	}
#endif
	return 0;
}

static int select_first_command_set_combination(struct nvme_id_ctrl_csc *csc, 
	uint64_t mask)
{
	uint64_t vector;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(csc->vector); i++) {
		vector = le64_to_cpu(csc->vector[i]);
		if ((vector & mask) == mask)
			return i;
	}
	return -EOPNOTSUPP;
}

static int prepare_ctrl_for_zns(struct nvme_dev_info *ndev)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;
	uint32_t csc_idx;
	int ret;

	if (!(NVME_CAP_CSS(prop->cap) & NVME_CAP_CSS_CSI)) {
		pr_warn("Not support one or more I/O command sets!\n");
		return -EOPNOTSUPP;
	}

	ret = nvme_read_ctrl_cc(ndev->fd, &prop->cc);
	if (ret < 0)
		return ret;
	
	if (NVME_CC_TO_CSS(prop->cc) != NVME_CC_CSS_CSI) {
		ret = change_command_set_selected(ndev, NVME_CC_CSS_CSI);
		if (ret < 0)
			return ret;
	}

	if (ctrl->id_ctrl_zns)
		return 0;

	if (!ctrl->id_ctrl_csc) {
		ctrl->id_ctrl_csc = zalloc(sizeof(struct nvme_id_ctrl_csc));
		if (!ctrl->id_ctrl_csc) {
			pr_err("failed to alloc memory!\n");
			return -ENOMEM;
		}

		ret = nvme_identify_ctrl_csc_list(ndev, ctrl->id_ctrl_csc, 0xffff);
		if (ret < 0) {
			pr_err("failed to get CSC list!(%d)\n", ret);
			free(ctrl->id_ctrl_csc);
			ctrl->id_ctrl_csc = NULL;
			return ret;
		}
	}

	ret = select_first_command_set_combination(ctrl->id_ctrl_csc,
		BIT(NVME_CSI_ZNS));
	if (ret < 0)
		return ret; /* auto free csc in @nvme_deinit */
	csc_idx = ret;

	ret = nvme_set_feat_iocs_profile(ndev, NVME_FEAT_SEL_CUR, csc_idx);
	if (ret < 0)
		return ret;

#if IS_ENABLED(TEST_ZNS_CHECK_MORE)
	ret = nvme_get_feat_iocs_profile(ndev, NVME_FEAT_SEL_CUR);
	if (ret < 0)
		return ret;
	else if (ret != csc_idx) {
		pr_err("The current effect CSC idx is %d, expect idx is %u!\n",
			ret, csc_idx);
		return -EPERM;
	}
#endif
	ctrl->id_ctrl_zns = zalloc(sizeof(struct nvme_id_ctrl_zns));
	if (!ctrl->id_ctrl_zns) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = nvme_identify_cs_ctrl(ndev, ctrl->id_ctrl_zns, 
		sizeof(struct nvme_id_ctrl_zns), NVME_CSI_ZNS);
	if (ret < 0) {
		pr_err("failed to get ZNS cmd set identify ctrl data!(%d)\n", ret);
		free(ctrl->id_ctrl_zns);
		ctrl->id_ctrl_zns = NULL;
		return ret;
	}

	return 0;
}

/**
 * @return 0 on success, otherwise a negative errno
 */
static int select_first_zone_namespace(struct nvme_dev_info *ndev, 
	uint32_t *nsid)
{
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t i;
	int ret;

	for (i = 0; i < ns_grp->nr_ns; i++) {
		ret = nvme_ns_support_zns_command_set(&ns_grp->ns[i]);
		if (ret == 1) {
			*nsid = ns_grp->ns[i].nsid;
			return 0;
		}
	}
	return -ENODEV;
}

static int init_test_data(struct nvme_dev_info *ndev, struct test_data *data)
{
	struct nvme_ns_instance *ns = NULL;
	struct nvme_zns_lbafe *lbafe = NULL;
	int ret;

	ret = prepare_ctrl_for_zns(ndev);
	if (ret < 0)
		return ret;
	
	ret = select_first_zone_namespace(ndev, &data->nsid);
	if (ret < 0)
		return ret;
	
	ns = &ndev->ns_grp->ns[data->nsid - 1];
	lbafe = &ns->id_ns_zns->lbafe[ns->fmt_idx];

	data->zsze = le64_to_cpu(lbafe->zsze);
	data->zdes = (uint32_t)lbafe->zdes * 64;

	pr_debug("NSID:%u, ZDSE: %u Byte, ZSZE: 0x%llx blocks\n", 
		data->nsid, data->zdes, data->zsze);
	return 0;
}

static int select_lba_formt_extension(struct nvme_dev_info *ndev, 
	struct test_data *data)
{
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ns_instance *ns = &ns_grp->ns[data->nsid - 1];
	struct nvme_id_ns_zns *id_ns_zns = ns->id_ns_zns;
	struct nvme_zns_lbafe *lbafe = NULL;
	uint32_t dw10;
	uint32_t i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(id_ns_zns->lbafe); i++) {
		lbafe = &id_ns_zns->lbafe[i];

		if (data->zdes_format_zero && !lbafe->zdes)
			break;
		else if (!data->zdes_format_zero && lbafe->zdes) {
			if (!data->zdes_format_size)
				break;
			if (data->zdes_format_size ==
				((uint32_t)lbafe->zdes * 64))
				break;
		}
	}

	if (i >= ARRAY_SIZE(id_ns_zns->lbafe)) {
		pr_err("No matching LBAFE could be found!\n");
		return -EINVAL;
	}

	pr_debug("format NSID:%u with LBAFE:%u\n", data->nsid, i);

	dw10 = NVME_FMT_LBAFL(i) | NVME_FMT_LBAFU(i);
	dw10 &= ~NVME_FMT_SES_MASK;
	dw10 |= NVME_FMT_SES_USER;

	ret = nvme_format_nvm(ndev, data->nsid, 0, dw10);
	if (ret < 0) {
		pr_err("failed to format ns:0x%x!(%d)\n", data->nsid, ret);
		return ret;
	}

	ret = nvme_update_ns_instance(ndev, ns, NVME_EVT_FORMAT_NVM);
	if (ret < 0)
		return ret;

	data->zsze = le64_to_cpu(lbafe->zsze);
	data->zdes = (uint32_t)lbafe->zdes * 64;

	return 0;
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

/**
 * @brief Send zns mgnt recv cmd with report zones action.
 * 
 * @return 0 on success, otherwise a negative errno.
 * 
 * @note Check whether the partical report is working properly.
 */
static int subcase_zra_report_zone_success(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_mgnt_recv_wrapper wrap = {0};
	struct test_data *test = &g_test;
	struct nvme_zone_report *report = NULL;
	uint32_t limit = 32;
	uint32_t nr_entry = rand() % limit + 1;
	uint64_t nr_zone;
	int ret;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = 0;
	wrap.action = NVME_ZRA_ZONE_REPORT;
	wrap.specific = NVME_ZRASF_ZONE_REPORT_ALL;
	wrap.size = sizeof(struct nvme_zone_report) + 
		nr_entry * sizeof(struct nvme_zone_descriptor);

	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	wrap.buf = report;

	ret = nvme_zone_manage_receive(ndev, &wrap);
	if (ret < 0)
		goto free_buf;
	nr_zone = le64_to_cpu(report->nr_zones);

	/* Is partical report work properly? */
	nr_entry = rand() % min_t(uint64_t, nr_zone, limit) + 1;
	wrap.size = sizeof(struct nvme_zone_report) + 
		nr_entry * sizeof(struct nvme_zone_descriptor);

	free(report);
	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	wrap.buf = report;
	wrap.partial_report = 1;

	ret = nvme_zone_manage_receive(ndev, &wrap);
	if (ret < 0)
		goto free_buf;
	nr_zone = le64_to_cpu(report->nr_zones);

	if (nr_zone != nr_entry) {
		pr_err("partial report err!(%llu vs %u)\n", nr_zone, nr_entry);
		ret = -EINVAL;
		goto free_buf;
	}

free_buf:
	free(report);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Send zns mgnt recv cmd with extended report zones action.
 * 
 * @return 0 on success, otherwise a negative errno. 
 */
static int subcase_zra_extend_report_zone_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_mgnt_recv_wrapper wrap = {0};
	struct test_data *test = &g_test;
	struct nvme_zone_report *report = NULL;
	uint32_t limit = 32;
	uint32_t nr_entry = rand() % limit + 1;
	int ret;

	if (!test->zdes) {
		test->zdes_format_zero = 0;
		test->zdes_format_size = 0; /* select size > 0 */

		ret = select_lba_formt_extension(ndev, test);
		if (ret < 0)
			goto out;
	}

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = 0;
	wrap.action = NVME_ZRA_EXT_ZONE_REPORT;
	wrap.specific = NVME_ZRASF_ZONE_REPORT_ALL;
	wrap.size = sizeof(struct nvme_zone_report) + 
		nr_entry * (sizeof(struct nvme_zone_descriptor) + test->zdes);
	
	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	wrap.buf = report;

	ret = nvme_zone_manage_receive(ndev, &wrap);
	if (ret < 0)
		goto free_buf;

free_buf:
	free(report);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Send zns mgnt recv cmd with extended report zones action.
 * 
 * @return 0 on success, otherwise a negative errno. 
 * 
 * @note If the zoned namespace is formatted with a zero Zone Descriptor
 * 	Extension Size, the controller shall abort the command with a
 * 	status code of Invalid Field in Command.
 */
static int subcase_zra_extend_report_zone_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_mgnt_recv_wrapper wrap = {0};
	struct test_data *test = &g_test;
	struct nvme_zone_report *report = NULL;
	struct nvme_completion entry = {0};
	uint32_t limit = 32;
	uint32_t nr_entry = rand() % limit + 1;
	uint16_t cid;
	int ret;

	if (test->zdes) {
		test->zdes_format_zero = 1;
		test->zdes_format_size = 0;

		ret = select_lba_formt_extension(ndev, test);
		if (ret < 0)
			goto out;
	}

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = 0;
	wrap.action = NVME_ZRA_EXT_ZONE_REPORT;
	wrap.specific = NVME_ZRASF_ZONE_REPORT_ALL;
	wrap.size = sizeof(struct nvme_zone_report) + 
		nr_entry * (sizeof(struct nvme_zone_descriptor) + test->zdes);
	
	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	wrap.buf = report;

	ret = nvme_cmd_zone_manage_receive(ndev->fd, &wrap);
	if (ret < 0)
		goto free_buf;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto free_buf;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		ret =  ret < 0 ? ret : -ETIME;
		goto free_buf;
	}

	ret = nvme_valid_cq_entry(&entry, sq->sqid, cid, NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto free_buf;

free_buf:
	free(report);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}


static int case_cmd_zns_manage_receive(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret |= subcase_zra_report_zone_success(tool, sq);
	ret |= subcase_zra_extend_report_zone_success(tool, sq);
	ret |= subcase_zra_extend_report_zone_invalid(tool, sq);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_CMD_SYMBOL(case_cmd_zns_manage_receive, 
	"Send I/O zone managment receive to IOSQ in various scenarios");
