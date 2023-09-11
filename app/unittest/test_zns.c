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

/*
 * FLAGS: R - restore after use, U - update after format
 */
struct test_data {
	uint32_t	nsid;
	uint64_t	nsze; /* U */
	uint32_t	lbads; /* U */

	/* zone descriptor extension size (covert to 1B unit) */
	uint32_t	zdes; /* U */
	uint64_t	zsze; /* U - unit: logical block */

	void		*buf; /* R */
	uint32_t	size; /* R */

	uint32_t	zdes_format_size;
	/* flags below */
	uint32_t	zdes_format_zero:1;
	uint32_t	select_all:1; /* R */
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
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ns_instance *ns = NULL;
	struct nvme_zns_lbafe *lbafe = NULL;
	int ret;

	ret = prepare_ctrl_for_zns(ndev);
	if (ret < 0)
		return ret;
	
	ret = select_first_zone_namespace(ndev, &data->nsid);
	if (ret < 0)
		return ret;

	ret = nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	/* we have checked once, skip the check below */
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);

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

	/* update test data */
	nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);
	data->zsze = le64_to_cpu(lbafe->zsze);
	data->zdes = (uint32_t)lbafe->zdes * 64;

	return 0;
}

/**
 * @brief Select a zone descriptor from the namespace for a specified state
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int select_zone_descriptor(struct nvme_dev_info *ndev, 
	struct nvme_sq_info *sq, struct nvme_zone_descriptor *desc, 
	uint8_t zrasf, uint32_t idx)
{
	struct nvme_zone_mgnt_recv_wrapper wrap = {0};
	struct test_data *test = &g_test;
	struct nvme_zone_report *report = NULL;
	uint64_t nr_zone;
	int ret;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = 0;
	wrap.action = NVME_ZRA_ZONE_REPORT;
	wrap.specific = zrasf;
	wrap.size = sizeof(struct nvme_zone_report) + 
		(idx + 1) * sizeof(struct nvme_zone_descriptor);
	wrap.status = NVME_SC_SUCCESS;

	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}
	wrap.buf = report;
	wrap.partial_report = 1;

	ret = nvme_io_zone_manage_receive(ndev, &wrap);
	if (ret < 0)
		goto free_buf;
	nr_zone = le64_to_cpu(report->nr_zones);

	if (nr_zone <= idx) {
		ret = -ENOENT;
		goto free_buf;
	}
	memcpy(desc, &report->entries[idx], sizeof(*desc));

free_buf:
	free(report);
	return ret;
}

static inline int select_first_zone_descriptor(struct nvme_dev_info *ndev,
	struct nvme_sq_info *sq, struct nvme_zone_descriptor *desc,
	uint8_t zrasf)
{
	return select_zone_descriptor(ndev, sq, desc, zrasf, 0);
}

/**
 * @brief Find a zone descriptor from the namespace
 * 
 * @param bit_mask The status of the zone to be searched 
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int find_zone_descriptor(struct nvme_dev_info *ndev, 
	struct nvme_sq_info *sq, struct nvme_zone_descriptor *desc,
	uint32_t bit_mask, uint32_t idx)
{
	if ((bit_mask & BIT(NVME_ZRASF_ZONE_REPORT_ZSE)) &&
		0 == select_zone_descriptor(ndev, sq, desc, 
			NVME_ZRASF_ZONE_REPORT_ZSE, idx))
		return 0;
	if ((bit_mask & BIT(NVME_ZRASF_ZONE_REPORT_ZSIO)) &&
		0 == select_zone_descriptor(ndev, sq, desc, 
			NVME_ZRASF_ZONE_REPORT_ZSIO, idx))
		return 0;
	if ((bit_mask & BIT(NVME_ZRASF_ZONE_REPORT_ZSEO)) &&
		0 == select_zone_descriptor(ndev, sq, desc, 
			NVME_ZRASF_ZONE_REPORT_ZSEO, idx))
		return 0;
	if ((bit_mask & BIT(NVME_ZRASF_ZONE_REPORT_ZSC)) &&
		0 == select_zone_descriptor(ndev, sq, desc, 
			NVME_ZRASF_ZONE_REPORT_ZSC, idx))
		return 0;
	if ((bit_mask & BIT(NVME_ZRASF_ZONE_REPORT_ZSF)) &&
		0 == select_zone_descriptor(ndev, sq, desc, 
			NVME_ZRASF_ZONE_REPORT_ZSF, idx))
		return 0;
	if ((bit_mask & BIT(NVME_ZRASF_ZONE_REPORT_ZSRO)) &&
		0 == select_zone_descriptor(ndev, sq, desc, 
			NVME_ZRASF_ZONE_REPORT_ZSRO, idx))
		return 0;
	if ((bit_mask & BIT(NVME_ZRASF_ZONE_REPORT_ZSO)) &&
		0 == select_zone_descriptor(ndev, sq, desc, 
			NVME_ZRASF_ZONE_REPORT_ZSO, idx))
		return 0;

	pr_div("failed to get zone descriptor!(0x%x, %u)\n", 
		bit_mask, idx);
	return -ENOENT;
}

static inline int find_first_zone_descriptor(struct nvme_dev_info *ndev,
	struct nvme_sq_info *sq, struct nvme_zone_descriptor *desc,
	uint32_t bit_mask)
{
	return find_zone_descriptor(ndev, sq, desc, bit_mask, 0);
}

static int do_zone_send_action(struct nvme_dev_info *ndev, 
	struct nvme_sq_info *sq, uint64_t slba, uint8_t action, uint16_t status)
{
	struct nvme_zone_mgnt_send_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.action = action;
	wrap.select_all = test->select_all;
	wrap.buf = test->buf;
	wrap.size = test->size;
	wrap.status = status;

	return nvme_io_zone_manage_send(ndev, &wrap);
}

static inline int close_zone(struct nvme_dev_info *ndev, 
	struct nvme_sq_info *sq, uint64_t slba, uint16_t status)
{
	return do_zone_send_action(ndev, sq, slba, NVME_ZONE_CLOSE, status);
}

static inline int finish_zone(struct nvme_dev_info *ndev,
	struct nvme_sq_info *sq, uint64_t slba, uint16_t status)
{
	return do_zone_send_action(ndev, sq, slba, NVME_ZONE_FINISH, status);
}

static inline int open_zone(struct nvme_dev_info *ndev,
	struct nvme_sq_info *sq, uint64_t slba, uint16_t status)
{
	return do_zone_send_action(ndev, sq, slba, NVME_ZONE_OPEN, status);
}

static inline int reset_zone(struct nvme_dev_info *ndev,
	struct nvme_sq_info *sq, uint64_t slba, uint16_t status)
{
	return do_zone_send_action(ndev, sq, slba, NVME_ZONE_RESET, status);
}

static inline int offline_zone(struct nvme_dev_info *ndev,
	struct nvme_sq_info *sq, uint64_t slba, uint16_t status)
{
	return do_zone_send_action(ndev, sq, slba, NVME_ZONE_OFFLINE, status);
}

static inline int set_zone_extension_descriptor(struct nvme_dev_info *ndev,
	struct nvme_sq_info *sq, uint64_t slba, uint16_t status)
{
	return do_zone_send_action(ndev, sq, slba, NVME_ZONE_SET_DESC_EXT, status);
}

static int create_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->nr_entry;
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
	csq_wrap.elements = sq->nr_entry;
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
 * @brief Send zns mgnt send cmd to NS which is write protected.
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the zoned namespace containing the specified zone is write
 * 	protected, then the controller shall abort the command with a
 * 	status code of Namespace is Write Protected.
 */
static int subcase_zsa_namespace_write_protect(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	struct test_data *test = &g_test;
	int recovery = 0;
	int ret, tmp;

	ret = nvme_ctrl_support_write_protect(ndev->ctrl);
	if (ret == 0) {
		pr_warn("device not support write protect! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	} else if (ret < 0) {
		pr_err("failed to check capability!(%d)\n", ret);
		goto out;
	}

	ret = nvme_get_feat_write_protect(ndev, test->nsid, NVME_FEAT_SEL_CUR);
	if (ret < 0)
		goto out;

	if (ret == NVME_NS_NO_WRITE_PROTECT) {
		ret = nvme_set_feat_write_protect(ndev, test->nsid, 
			NVME_FEAT_SEL_CUR, NVME_NS_WRITE_PROTECT);
		if (ret < 0) {
			pr_err("failed to set nsid(%u) to write protect "
				"state!(%d)\n", test->nsid, ret);
			goto out;
		}
		recovery = 1;
	}

	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSE);
	if (ret < 0) {
		pr_warn("failed to get ZSE descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out_recovery;
	}

	ret = open_zone(ndev, sq, le64_to_cpu(desc.zslba), 
		NVME_SC_NS_WRITE_PROTECTED);
	if (ret < 0)
		goto out_recovery;

out_recovery:
	if (recovery) {
		/*
		 * If use @ret to receive return value, may destroy the origin 
		 * value of @ret.(eg: @ret<0, new=0 ==> @ret=0, lost errno)
		 */
		tmp = nvme_set_feat_write_protect(ndev, test->nsid, 
			NVME_FEAT_SEL_CUR, NVME_NS_NO_WRITE_PROTECT);
		if (tmp < 0) {
			pr_err("failed to set nsid(%u) to no write protect "
				"state!(%d)\n", test->nsid, ret);
			ret |= tmp;
		}
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Send zns mgnt send cmd with wrong "slba" field.
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the command SLBA field doesn't specify the starting logical
 * 	block for a zone in the specified zoned namespace and the Select
 * 	All bit is cleared to '0', then the controller shall abort the 
 * 	command with a status code of Invalid Field in Command.
 */
static int subcase_zsa_slba_invalid(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSE);
	if (ret < 0) {
		pr_warn("failed to get ZSE descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = open_zone(ndev, sq, le64_to_cpu(desc.zslba) + 1, 
		NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Set Zone Descriptor Extension action with size is zero
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the zone descriptor extension size is 0h, then the controller
 * 	shall abort the command with a status code of Invalid Field in
 * 	Command.
 */
static int subcase_zsa_set_zde_size0(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	if (test->zdes) {
		test->zdes_format_zero = 1;
		test->zdes_format_size = 0;

		ret = select_lba_formt_extension(ndev, test);
		if (ret < 0)
			goto out;
	}

	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSE);
	if (ret < 0) {
		pr_warn("failed to get ZSE descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	test->buf = tool->wbuf;
	test->size = 64; /* zone descriptor extension size */

	ret = set_zone_extension_descriptor(ndev, sq, le64_to_cpu(desc.zslba),
		NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto restore;

restore:
	/* Restore the original after use to avoid impact on the next subcase */
	test->buf = NULL;
	test->size = 0;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Close Zone action
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_zsa_close_zone_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc,
		BIT(NVME_ZRASF_ZONE_REPORT_ZSEO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO));
	if (ret == 0)
		goto close;
	
	pr_warn("failed to get ZSEO/ZSIO descriptor! try ZSE...\n");

	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSE);
	if (ret < 0) {
		pr_warn("failed to get ZSE descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = open_zone(ndev, sq, le64_to_cpu(desc.zslba), NVME_SC_SUCCESS);
	if (ret < 0) {
		pr_err("failed to open an empty zone!(%d)\n", ret);
		goto out;
	}

close:
	ret = close_zone(ndev, sq, le64_to_cpu(desc.zslba), NVME_SC_SUCCESS);
	if (ret < 0) {
		pr_err("failed to close an opened zone!(%d)\n", ret);
		goto out;
	}

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Close Zone action on zone which state is invalid
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the specified zone is ZSE/ZSF/ZSRO/ZSO, and Select All bit is 0,
 * 	then the controller shall abort the command with a status code 
 * 	of Invalid Zone State Transition.
 */
static int subcase_zsa_close_zone_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc, 
		BIT(NVME_ZRASF_ZONE_REPORT_ZSE) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSF) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSRO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSO));
	if (ret < 0) {
		pr_warn("failed to get ZSE/ZSF/ZSRO/ZSO descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = close_zone(ndev, sq, le64_to_cpu(desc.zslba), 
		NVME_SC_ZONE_INVALID_TRANSITION);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Finish Zone action
 * 
 * @return 0 on success, otherwise a negative errno 
 */
static int subcase_zsa_finish_zone_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc, 
		BIT(NVME_ZRASF_ZONE_REPORT_ZSE) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSEO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSC));
	if (ret < 0) {
		pr_warn("failed to get ZSE/ZSIO/ZSEO/ZSC descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = finish_zone(ndev, sq, le64_to_cpu(desc.zslba), NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}
/**
 * @brief Do Finish Zone action on zone which state is invalid
 * 
 * @return 0 on success, otherwise a negative errno 
 * 
 * @note If the specified zone is ZSRO/ZSO, and Select All bit is 0,
 * 	then the controller shall abort the command with a status code
 * 	of Invalid Zone State Transition.
 */
static int subcase_zsa_finish_zone_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc,
		BIT(NVME_ZRASF_ZONE_REPORT_ZSRO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSO));
	if (ret < 0) {
		pr_warn("failed to get ZSRO/ZSO descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = finish_zone(ndev, sq, le64_to_cpu(desc.zslba), 
		NVME_SC_ZONE_INVALID_TRANSITION);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Open Zone action
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_zsa_open_zone_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc, 
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSC) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSE));
	if (ret < 0) {
		pr_warn("failed to get ZSIO/ZSC/ZSE descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = open_zone(ndev, sq, le64_to_cpu(desc.zslba), NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Open Zone action on zone which state is invalid
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the specified zone is ZSF/ZSRO/ZSO, and Select All bit is 0,
 * 	then the controller shall  abort the command with a status code
 * 	of Invalid Zone State Transition.
 */
static int subcase_zsa_open_zone_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc,
		BIT(NVME_ZRASF_ZONE_REPORT_ZSF) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSRO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSO));
	if (ret < 0) {
		pr_warn("failed to get ZSF/ZSRO/ZSO descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = open_zone(ndev, sq, le64_to_cpu(desc.zslba), 
		NVME_SC_ZONE_INVALID_TRANSITION);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Reset Zone action
 * 
 * @return 0 on success, otherwise a negative errno 
 */
static int subcase_zsa_reset_zone_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc, 
		BIT(NVME_ZRASF_ZONE_REPORT_ZSF) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSC) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSEO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO));
	if (ret < 0) {
		pr_warn("failed to get ZSF/ZSC/ZSEO/ZSIO descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = reset_zone(ndev, sq, le64_to_cpu(desc.zslba), NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Reset Zone action on zone which state is invalid
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the specified zone is ZSRO/ZSO, and Select All bit is 0,
 * 	then the controller shall abort the command with a status code
 * 	of Invalid Zone State Transition.
 */
static int subcase_zsa_reset_zone_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc,
		BIT(NVME_ZRASF_ZONE_REPORT_ZSRO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSO));
	if (ret < 0) {
		pr_warn("failed to get ZSRO/ZSO descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = reset_zone(ndev, sq, le64_to_cpu(desc.zslba), 
		NVME_SC_ZONE_INVALID_TRANSITION);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Offline Zone action
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_zsa_offline_zone_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSRO);
	if (ret < 0) {
		pr_warn("failed to get ZSRO descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = offline_zone(ndev, sq, le64_to_cpu(desc.zslba), NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Do Offline Zone action on zone which state is invalid
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the specified zone is ZSE/ZSIO/ZSEO/ZSC/ZSF, and Select All 
 * 	bit is 0, then the controller shall abort the command with a 
 * 	status code of Invalid Zone State Transition.
 */
static int subcase_zsa_offline_zone_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc, 
		BIT(NVME_ZRASF_ZONE_REPORT_ZSE) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSEO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSC) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSF));
	if (ret < 0) {
		pr_warn("failed to get ZSE/ZSIO/ZSEO/ZSC/ZSF descriptor! "
			"skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = offline_zone(ndev, sq, le64_to_cpu(desc.zslba), 
		NVME_SC_ZONE_INVALID_TRANSITION);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Set Zone Descriptor Extension action
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_zsa_set_zde_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	if (!test->zdes) {
		test->zdes_format_zero = 0;
		test->zdes_format_size = 0; /* select size > 0 */

		ret = select_lba_formt_extension(ndev, test);
		if (ret < 0)
			goto out;
	}

	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSE);
	if (ret < 0) {
		pr_warn("failed to get ZSE descriptor! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	test->buf = tool->wbuf;
	test->size = test->zdes;
	BUG_ON(test->size > tool->wbuf_size);
	fill_data_with_random(test->buf, test->size);

	ret = set_zone_extension_descriptor(ndev, sq, le64_to_cpu(desc.zslba),
		NVME_SC_SUCCESS);
	if (ret < 0)
		goto restore;

restore:
	/* Restore the original after use to avoid impact on the next subcase */
	test->buf = NULL;
	test->size = 0;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Set Zone Descriptor Extension action on zone which state is invalid
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the specified zone isn't ZSE, and Select All bit is 0, then 
 * 	the controller shall abort the command with a status code of
 * 	Invalid Zone State Transition.
 */
static int subcase_zsa_set_zde_with_sel0_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	if (!test->zdes) {
		test->zdes_format_zero = 0;
		test->zdes_format_size = 0; /* select size > 0 */

		ret = select_lba_formt_extension(ndev, test);
		if (ret < 0)
			goto out;
	}

	ret = find_first_zone_descriptor(ndev, sq, &desc, 
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO) | 
		BIT(NVME_ZRASF_ZONE_REPORT_ZSEO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSC) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSF) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSRO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSO));
	if (ret == 0)
		goto set_ext_desc;
	
	pr_warn("failed to get ZSIO/ZSEO/ZSC/ZSF/ZSRO/ZSO descriptor! "
		"try to ZSE...\n");
	
	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSE);
	if (ret < 0) {
		pr_err("failed to get ZSE descriptor!(%d)\n", ret);
		goto out;
	}

	ret = open_zone(ndev, sq, le64_to_cpu(desc.zslba), NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;

set_ext_desc:
	test->buf = tool->wbuf;
	test->size = test->zdes;
	BUG_ON(test->size > tool->wbuf_size);
	fill_data_with_random(test->buf, test->size);

	ret = set_zone_extension_descriptor(ndev, sq, le64_to_cpu(desc.zslba),
		NVME_SC_ZONE_INVALID_TRANSITION);
	if (ret < 0)
		goto restore;

restore:
	/* Restore the original after use to avoid impact on the next subcase */
	test->buf = NULL;
	test->size = 0;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Set Zone Descriptor Extension action with Select All bit is 1.
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the Select All bit is 1, then the controller shall abort the
 * 	command with a status of Invalid Field in Command
 */
static int subcase_zsa_set_zde_with_sel1_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_zone_descriptor desc = {0};
	int ret;

	if (!test->zdes) {
		test->zdes_format_zero = 0;
		test->zdes_format_size = 0; /* select size > 0 */

		ret = select_lba_formt_extension(ndev, test);
		if (ret < 0)
			goto out;
	}

	ret = select_first_zone_descriptor(ndev, sq, &desc, 
		NVME_ZRASF_ZONE_REPORT_ZSE);
	if (ret < 0) {
		pr_warn("failed to get ZSE descriptor! skip...\n");
		goto out;
	}

	test->buf = tool->wbuf;
	test->size = test->zdes;
	test->select_all = 1;

	ret = set_zone_extension_descriptor(ndev, sq, le64_to_cpu(desc.zslba),
		NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto restore;

restore:
	/* Restore the original after use to avoid impact on the next subcase */
	test->buf = NULL;
	test->size = 0;
	test->select_all = 0;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
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
	wrap.status = NVME_SC_SUCCESS;

	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	wrap.buf = report;

	ret = nvme_io_zone_manage_receive(ndev, &wrap);
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

	ret = nvme_io_zone_manage_receive(ndev, &wrap);
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
	wrap.status = NVME_SC_SUCCESS;
	
	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	wrap.buf = report;

	ret = nvme_io_zone_manage_receive(ndev, &wrap);
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
	uint32_t limit = 32;
	uint32_t nr_entry = rand() % limit + 1;
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
	wrap.status = NVME_SC_INVALID_FIELD;
	
	report = zalloc(wrap.size);
	if (!report) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	wrap.buf = report;

	ret = nvme_io_zone_manage_receive(ndev, &wrap);
	if (ret < 0)
		goto free_buf;
free_buf:
	free(report);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Send zone append command
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_append_success(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_zone_descriptor desc = {0};
	struct nvme_zone_append_wrapper wrap = {0};
	uint32_t limit = min_t(uint32_t, tool->wbuf_size / test->lbads, 0x10000);
	uint64_t zcap, zslba, wp, remain;
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc,
		BIT(NVME_ZRASF_ZONE_REPORT_ZSE) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSEO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSC));
	if (ret < 0) {
		pr_warn("failed to get ZSE/ZSIO/ZSEO/ZSC descriptor!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}
	zcap = le64_to_cpu(desc.zcap);
	zslba = le64_to_cpu(desc.zslba);
	wp = le64_to_cpu(desc.wp);
	BUG_ON(wp < zslba || (wp - zslba) > zcap);

	remain = zcap - (wp - zslba);

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.zslba = zslba;
	wrap.nlb = rand() % min_t(uint64_t, remain, limit) + 1;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;
	wrap.status = NVME_SC_SUCCESS;
	
	ret = nvme_io_zone_append(ndev, &wrap);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Send zone append command with invalid zone type
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the zone specified by the zone append command is not a 
 * 	sequential write required zone, then the command shall be
 * 	aborted with a status code of Invalid Field in Command.
 */
static int subcase_append_invalid_ztype(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	return -EOPNOTSUPP;
}

/**
 * @brief Send zone append command with invalid zslba
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the ZSLBA field in the zone append command does not specify
 * 	the lowest logical block for a zone, then the command shall be
 * 	aborted with a status code of Invalid Field in Command.
 */
static int subcase_append_invalid_zslba(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_zone_descriptor desc = {0};
	struct nvme_zone_append_wrapper wrap = {0};
	uint32_t limit = min_t(uint32_t, tool->wbuf_size / test->lbads, 0x10000);
	uint64_t zcap, zslba, wp, remain;
	int ret;

	ret = find_first_zone_descriptor(ndev, sq, &desc,
		BIT(NVME_ZRASF_ZONE_REPORT_ZSE) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSIO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSEO) |
		BIT(NVME_ZRASF_ZONE_REPORT_ZSC));
	if (ret < 0) {
		pr_warn("failed to get ZSE/ZSIO/ZSEO/ZSC descriptor!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}
	zcap = le64_to_cpu(desc.zcap);
	zslba = le64_to_cpu(desc.zslba);
	wp = le64_to_cpu(desc.wp);
	BUG_ON(wp < zslba || (wp - zslba) > zcap);

	remain = zcap - (wp - zslba);

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	/* inject error: set invalid zslba */
	wrap.zslba = zslba + 1;
	wrap.nlb = rand() % min_t(uint64_t, remain, limit) + 1;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;
	wrap.status = NVME_SC_INVALID_FIELD;
	
	ret = nvme_io_zone_append(ndev, &wrap);
	if (ret < 0)
		goto out;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

static int case_cmd_zns_manage_send(struct nvme_tool *tool)
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
	
	ret |= subcase_zsa_namespace_write_protect(tool, sq);
	ret |= subcase_zsa_slba_invalid(tool, sq);
	ret |= subcase_zsa_set_zde_size0(tool, sq);
	ret |= subcase_zsa_close_zone_success(tool, sq);
	ret |= subcase_zsa_close_zone_invalid(tool, sq);
	ret |= subcase_zsa_finish_zone_success(tool, sq);
	ret |= subcase_zsa_finish_zone_invalid(tool, sq);
	ret |= subcase_zsa_open_zone_success(tool, sq);
	ret |= subcase_zsa_open_zone_invalid(tool, sq);
	ret |= subcase_zsa_reset_zone_success(tool, sq);
	ret |= subcase_zsa_reset_zone_invalid(tool, sq);
	ret |= subcase_zsa_offline_zone_success(tool, sq);
	ret |= subcase_zsa_offline_zone_invalid(tool, sq);
	ret |= subcase_zsa_set_zde_success(tool, sq);
	ret |= subcase_zsa_set_zde_with_sel0_invalid(tool, sq);
	ret |= subcase_zsa_set_zde_with_sel1_invalid(tool, sq);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_CMD_SYMBOL(case_cmd_zns_manage_send, 
	"Send I/O zone managment send command to IOSQ in various scenarios");

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
	"Send I/O zone managment receive command to IOSQ in various scenarios");

static int case_cmd_zns_append(struct nvme_tool *tool)
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

	ret |= subcase_append_success(tool, sq);
	ret |= subcase_append_invalid_ztype(tool, sq);
	ret |= subcase_append_invalid_zslba(tool, sq);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_CMD_SYMBOL(case_cmd_zns_append, 
	"Send I/O zone append command to IOSQ in various scenarios");
