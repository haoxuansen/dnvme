/**
 * @file test_e2e_protect.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief Test end-to-end data protection
 * @version 0.1
 * @date 2023-09-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libcrc.h"
#include "libnvme.h"
#include "test.h"

/*
 * Flags:
 * 	C: config before use
 * 	U: update after format
 */
struct test_data {
	uint32_t	nsid;
	uint64_t	nsze; /* U */
	uint32_t	lbads; /* U */
	uint16_t	meta_size; /* U */

	uint16_t	app_tag;
	uint16_t	app_tag_mask;
	uint64_t	storage_tag;
	uint64_t	ref_tag;

	/* Namespace Metadata Capabilities */
	uint32_t	is_sup_meta_extlba:1;
	uint32_t	is_sup_meta_ptr:1;
	/* Data Protection Capability */
	uint32_t	is_sup_16b_guard:1;
	uint32_t	is_sup_32b_guard:1;
	uint32_t	is_sup_64b_guard:1;
	uint32_t	is_sup_last_bytes:1;
	uint32_t	is_sup_first_bytes:1;
	/* Namespace Protection Setting */
	uint32_t	is_pi_first:1;
	/* Protection Information Check */
	uint32_t	is_set_pract:1;
	uint32_t	is_not_resident:1; ///< metadata not resident in host buffer

	uint32_t	is_use_meta_ptr:1; /* NOTE: Not support yet.*/
	/* Namesapce Format Option */
	uint32_t	is_match_pif:1; /* C */
	uint32_t	is_match_sts:1; /* C */
	uint32_t	is_match_ms:3; /* C: Metadata size */
#define TEST_MATCH_MS_EQUAL		1
#define TEST_MATCH_MS_SMALLER		2
#define TEST_MATCH_MS_LARGER		3

	uint8_t		cur_pif; /* U */
	uint8_t		cur_sts; /* U: storage tag size */
	uint8_t		cur_rts; /* U: Reference tag size */

	uint8_t		cfg_pif; /* C */
	uint8_t		cfg_sts; /* C */
	uint16_t	cfg_ms; /* C */
};

static struct test_data g_test = {0};

static void test_data_reset_format_fields(struct test_data *data)
{
	data->is_match_pif = 0;
	data->is_match_sts = 0;
	data->is_match_ms = 0;

	data->cfg_pif = 0;
	data->cfg_sts = 0;
	data->cfg_ms = 0;
}

static void test_data_reset_rw_fields(struct test_data *data)
{
	data->is_set_pract = 0;
	data->is_not_resident = 0;
}

static int get_reference_tag_size(uint8_t pif, uint8_t sts)
{
	uint8_t ref_tag_size;

	switch (pif) {
	case NVME_ELBAF_PIF_TYPE1:
		if(sts > NVME_PI_TYPE1_ST_MAX_BIT_SIZE) {
			pr_err("PIF1 storage tag size(%u) invalid!\n", sts);
			return -EINVAL;
		}
		ref_tag_size = NVME_PI_TYPE1_TAG_BIT_SIZE - sts;
		break;

	case NVME_ELBAF_PIF_TYPE2:
		if (sts < NVME_PI_TYPE2_ST_MIN_BIT_SIZE ||
			sts > NVME_PI_TYPE2_ST_MAX_BIT_SIZE) {
			pr_err("PIF2 storage tag size(%u) invalid!\n", sts);
			return -EINVAL;
		}
		ref_tag_size = NVME_PI_TYPE2_TAG_BIT_SIZE - sts;
		break;

	case NVME_ELBAF_PIF_TYPE3:
		if (sts > NVME_PI_TYPE3_ST_MAX_BIT_SIZE) {
			pr_err("PIF3 storage tag size(%u) invalid!\n", sts);
			return -EINVAL;
		}
		ref_tag_size = NVME_PI_TYPE3_TAG_BIT_SIZE - sts;
		break;

	default:
		pr_err("PI format(%u) is unkonw!\n", pif);
		return -EINVAL;
	}
	return ref_tag_size;
}

static void prepare_random_tag(struct test_data *data)
{
	data->app_tag = (uint32_t)rand() & 0xffff;
	data->app_tag_mask = (uint32_t)rand() & 0xffff;
	data->storage_tag = (uint64_t)rand() << 32 | (uint64_t)rand();
	data->ref_tag = (uint64_t)rand() << 32 | (uint64_t)rand();
}

static int prepare_ctroller(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_feat_host_behavior *behavior = tool->wbuf;
	int ret;

	if (nvme_version(ndev) < NVME_VS(1, 4, 0)) {
		pr_warn("version:0x%x not support E2E protect! skip...\n", 
			nvme_version(ndev));
		return -EOPNOTSUPP;
	}

	memset(behavior, 0, sizeof(struct nvme_feat_host_behavior));
	behavior->lbafee = 1;

	ret = nvme_set_feat_host_behavior(ndev, NVME_FEAT_SEL_CUR, behavior);
	if (ret < 0)
		return ret;

	ret = nvme_identify_ctrl(ndev, ctrl->id_ctrl); /* update data */
	if (ret < 0)
		return ret;

	ret = nvme_ctrl_support_ext_lba_formats(ctrl);
	if (ret == 0) {
		pr_warn("device not support extended LBA formats! skip...\n");
		return -EOPNOTSUPP;
	} else if (ret < 0) {
		pr_err("failed to check capability!(%d)\n", ret);
		return ret;
	}

	return 0;
}

static int prepare_namespace(struct nvme_tool *tool, struct test_data *data)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ns_instance *ns = &ns_grp->ns[data->nsid];
	uint32_t elbaf;
	uint8_t dpc;
	uint8_t dps;
	uint8_t mc;
	int ret;

	/* Check protection information capability */
	dpc = nvme_id_ns_dpc(ns_grp, data->nsid);
	if (dpc & NVME_NS_DPC_PI_TYPE1)
		data->is_sup_16b_guard = 1;
	if (dpc & NVME_NS_DPC_PI_TYPE2)
		data->is_sup_32b_guard = 1;
	if (dpc & NVME_NS_DPC_PI_TYPE3)
		data->is_sup_64b_guard = 1;
	if (dpc & NVME_NS_DPC_PI_FIRST)
		data->is_sup_first_bytes = 1;
	if (dpc & NVME_NS_DPC_PI_LAST)
		data->is_sup_last_bytes = 1;
	
	if (!data->is_sup_16b_guard && !data->is_sup_32b_guard &&
		!data->is_sup_64b_guard) {
		pr_warn("all PI types are not support! skip...\n");
		return -EOPNOTSUPP;
	}
	if (!data->is_sup_first_bytes && !data->is_sup_last_bytes) {
		pr_warn("all PI position are not support! skip...\n");
		return -EOPNOTSUPP;
	}

	elbaf = le32_to_cpu(ns->id_ns_nvm->elbaf[ns->fmt_idx].dw0);
	data->cur_pif = NVME_ELBAF_PIF(elbaf);
	data->cur_sts = NVME_ELBAF_STS(elbaf);

	ret = get_reference_tag_size(data->cur_pif, data->cur_sts);
	if (ret < 0)
		return ret;
	data->cur_rts = ret;

	dps = nvme_id_ns_dps(ns_grp, data->nsid);
	if (dps & NVME_NS_DPS_PI_FIRST)
		data->is_pi_first = 1;

	/* Check metadata capability */
	mc = nvme_id_ns_mc(ns_grp, data->nsid);
	if (mc & NVME_MC_EXTENDED_LBA)
		data->is_sup_meta_extlba = 1;
	if (mc & NVME_MC_METADATA_PTR)
		data->is_sup_meta_ptr = 1;

	if (!data->is_sup_meta_extlba && !data->is_sup_meta_ptr) {
		pr_warn("all metadata xfer ways are not support! skip...\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int init_test_data(struct nvme_tool *tool, struct test_data *data)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ns_instance *ns;
	int ret;

	/* use first active namespace as default */
	data->nsid = le32_to_cpu(ns_grp->act_list[0]);
	ns = &ns_grp->ns[data->nsid - 1];

	if (!ns->id_ns_nvm) {
		pr_warn("NVM command set identify namespace data not exist!\n");
		return -EOPNOTSUPP;
	}

	ret = prepare_ctroller(tool);
	if (ret < 0)
		return ret;

	ret = prepare_namespace(tool, data);
	if (ret < 0)
		return ret;

	prepare_random_tag(data);

	ret = nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	/* we have checked once, skip the check below */
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);
	data->meta_size = (uint16_t)nvme_id_ns_ms(ns_grp, data->nsid);

	return 0;
}

static int select_ext_lba_format(struct nvme_dev_info *ndev, 
	struct test_data *data)
{
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ns_instance *ns = &ns_grp->ns[data->nsid - 1];
	struct nvme_id_ns *id_ns = ns->id_ns;
	struct nvme_id_ns_nvm *id_ns_nvm = ns->id_ns_nvm;
	uint32_t elbaf;
	uint32_t i;
	uint32_t dw10;
	uint16_t ms;
	uint8_t dps;
	uint8_t matched = 0;
	int ret;

	for (i = 0; i < ARRAY_SIZE(id_ns_nvm->elbaf); i++) {
		elbaf = le32_to_cpu(id_ns_nvm->elbaf[i].dw0);

		if (data->is_match_pif && 
			data->cfg_pif != NVME_ELBAF_PIF(elbaf))
			continue;
		if (data->is_match_sts &&
			data->cfg_sts != NVME_ELBAF_STS(elbaf))
			continue;
		if (data->is_match_ms) {
			ms = le16_to_cpu(id_ns->lbaf[i].ms);
			switch (data->is_match_ms) {
			case TEST_MATCH_MS_EQUAL:
				if (data->cfg_ms == ms)
					matched = 1;
				break;
			case TEST_MATCH_MS_SMALLER:
				if (ms < data->cfg_ms)
					matched = 1;
				break;
			case TEST_MATCH_MS_LARGER:
				if (ms > data->cfg_ms)
					matched = 1;
				break;
			default:
				pr_err("is_match_ms:%u is unknown!\n",
					data->is_match_ms);
				return -EINVAL;
			}
			if (!matched)
				continue;
		}

		break; /* match successful */
	}

	if (i >= ARRAY_SIZE(id_ns_nvm->elbaf)) {
		pr_err("No matching ELBAF could be found!\n");
		return -EINVAL;
	}

	pr_debug("format NSID:%u with ELBAF:%u\n", data->nsid, i);

	dw10 = NVME_FMT_LBAFL(i) | NVME_FMT_LBAFU(i);
	dw10 &= ~NVME_FMT_SES_MASK;
	dw10 |= NVME_FMT_SES_USER;

	ret = nvme_format_nvm(ndev, data->nsid, 0, dw10);
	if (ret < 0) {
		pr_err("failed to format ns:0x%x!(%d)\n", data->nsid, ret);
		return ret;
	}
	data->cur_pif = data->cfg_pif;
	data->cur_sts = data->cfg_sts;

	ret = get_reference_tag_size(data->cur_pif, data->cur_sts);
	if (ret < 0)
		return ret;
	data->cur_rts = ret;

	ret = nvme_update_ns_instance(ndev, ns, NVME_EVT_FORMAT_NVM);
	if (ret < 0)
		return ret;

	dps = nvme_id_ns_dps(ns_grp, data->nsid);
	if ((dps & NVME_NS_DPS_PI_MASK) != (data->cfg_pif + 1)) {
		pr_err("PIT:%u mismatch PIF:%u after format!\n",
			dps & NVME_NS_DPS_PI_MASK, data->cfg_pif);
		return -EPERM;
	}

	/* update test data */
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);
	nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	data->meta_size = (uint16_t)nvme_id_ns_ms(ns_grp, data->nsid);

	return 0;
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

static void fill_cmd_tags(struct nvme_rwc_wrapper *wrap, struct test_data *data)
{
	uint64_t ref_tag_mask;

	switch (data->cur_pif) {
	case NVME_ELBAF_PIF_TYPE1:
		ref_tag_mask = (data->cur_rts == 32) ? U32_MAX :
			(U32_MAX << (32 - data->cur_rts)) >> (32 - data->cur_rts);
		if (data->cur_sts == 32)
			wrap->dw14 = lower_32_bits(data->ref_tag);
		else
			wrap->dw14 = (lower_32_bits(data->ref_tag) & ref_tag_mask) |
				(lower_32_bits(data->storage_tag) << data->cur_rts);
		break;

	case NVME_ELBAF_PIF_TYPE2:
		ref_tag_mask = (data->cur_rts == 64) ? U64_MAX :
			(U64_MAX << (64 - data->cur_rts)) >> (64 - data->cur_rts);

		if (data->cur_rts == 32) {
			wrap->dw14 = lower_32_bits(data->ref_tag);
			wrap->dw3 = lower_32_bits(data->storage_tag);
			wrap->dw2 = upper_32_bits(data->storage_tag) & 0xffff;
		} else if (data->cur_rts == 64) {
			wrap->dw14 = lower_32_bits(data->ref_tag);
			wrap->dw3 = upper_32_bits(data->ref_tag);
			wrap->dw2 = data->storage_tag & 0xffff;
		} else if (data->cur_rts < 32) {
			wrap->dw14 = (lower_32_bits(data->ref_tag) & ref_tag_mask) |
				(lower_32_bits(data->storage_tag) << data->cur_rts);
			wrap->dw3 = lower_32_bits(data->storage_tag >> 
				(32 - data->cur_rts));
			wrap->dw2 = upper_32_bits(data->storage_tag >> 
				(32 - data->cur_rts)) & 0xffff;
		} else { /* cur_rts > 32 && cur_rts < 64 */
			wrap->dw14 = lower_32_bits(data->ref_tag);
			wrap->dw3 = (upper_32_bits(data->ref_tag) & 
				upper_32_bits(ref_tag_mask)) | 
				(lower_32_bits(data->storage_tag) << 
				(data->cur_rts - 32));
			wrap->dw2 = lower_32_bits(data->storage_tag >> 
				(64 - data->cur_rts)) & 0xffff;
		}
		break;
	
	case NVME_ELBAF_PIF_TYPE3:
		ref_tag_mask = (U64_MAX << (64 - data->cur_rts)) >> 
			(64 - data->cur_rts);
		
		if (data->cur_rts == 32) {
			wrap->dw14 = lower_32_bits(data->ref_tag);
			wrap->dw3 = lower_32_bits(data->storage_tag) & 0xffff;
		} else if (data->cur_rts < 32) {
			wrap->dw14 = (lower_32_bits(data->ref_tag) & ref_tag_mask) |
				(lower_32_bits(data->storage_tag) << data->cur_rts);
			wrap->dw3 = lower_32_bits(data->storage_tag >> 
				(32 - data->cur_rts)) & 0xffff;
		} else { /* cur_rts > 32 && cur_rts <= 48 */
			wrap->dw14 = lower_32_bits(data->ref_tag);
			wrap->dw3 = ((upper_32_bits(data->ref_tag) & 
				upper_32_bits(ref_tag_mask)) | 
				(lower_32_bits(data->storage_tag) << 
				(data->cur_rts - 32))) & 0xffff;
		}
		break;
	}
	pr_debug("orgin storage tag:0x%016llx, reference tag:0x%016llx\n",
		data->storage_tag, data->ref_tag);
	pr_debug("storage tag size:%u, reference tag size:%u\n",
		data->cur_sts, data->cur_rts);
	pr_debug("cdw2:0x%08x, cdw3:0x%08x, cdw14:0x%08x\n", 
		wrap->dw2, wrap->dw3, wrap->dw14);
}

static void fill_16b_guard_protect_info(struct nvme_rwc_wrapper *wrap, 
	struct test_data *data, uint32_t meta_oft)
{
	uint16_t app_tag;
	uint16_t crc;
	uint32_t buf_oft;
	uint32_t i;
	uint8_t *pi;

	app_tag = wrap->apptag & (~wrap->appmask); /* clear mask bit */

	for (i = 0; i < wrap->nlb; i++) {
		buf_oft = i * (data->lbads + data->meta_size);
		crc = crc16_t10_dif(wrap->buf + buf_oft, data->lbads + meta_oft);

		pi = wrap->buf + buf_oft + meta_oft;
		/* Guard */
		pi[0] = crc >> 8;
		pi[1] = crc & 0xff;
		/* Application Tag */
		pi[2] = app_tag >> 8;
		pi[3] = app_tag & 0xff;
		/* Storage and Reference Space */
		pi[4] = (wrap->dw14 >> 24) & 0xff;
		pi[5] = (wrap->dw14 >> 16) & 0xff;
		pi[6] = (wrap->dw14 >> 8) & 0xff;
		pi[7] = wrap->dw14 & 0xff;
	}
}

static void fill_32b_guard_protect_info(struct nvme_rwc_wrapper *wrap,
	struct test_data *data, uint32_t meta_oft)
{
	uint16_t app_tag;
	uint32_t crc;
	uint32_t buf_oft;
	uint32_t i;
	uint8_t *pi;

	app_tag = wrap->apptag & (~wrap->appmask); /* clear mask bit */

	for (i = 0; i < wrap->nlb; i++) {
		buf_oft = i * (data->lbads + data->meta_size);
		crc = crc32_castagnoli(wrap->buf + buf_oft, data->lbads + meta_oft);

		pi = wrap->buf + buf_oft + meta_oft;
		/* Guard */
		pi[0] = (crc >> 24) & 0xff;
		pi[1] = (crc >> 16) & 0xff;
		pi[2] = (crc >> 8) & 0xff;
		pi[3] = crc & 0xff;
		/* Application Tag */
		pi[4] = app_tag >> 8;
		pi[5] = app_tag & 0xff;
		/* Storage and Reference Space */
		pi[6] = (wrap->dw2 >> 8) & 0xff;
		pi[7] = wrap->dw2 & 0xff;
		pi[8] = (wrap->dw3 >> 24) & 0xff;
		pi[9] = (wrap->dw3 >> 16) & 0xff;
		pi[10] = (wrap->dw3 >> 8) & 0xff;
		pi[11] = wrap->dw3 & 0xff;
		pi[12] = (wrap->dw14 >> 24) & 0xff;
		pi[13] = (wrap->dw14 >> 16) & 0xff;
		pi[14] = (wrap->dw14 >> 8) & 0xff;
		pi[15] = wrap->dw14 & 0xff;
	}
}

static void fill_64b_guard_protect_info(struct nvme_rwc_wrapper *wrap,
	struct test_data *data, uint32_t meta_oft)
{
	uint16_t app_tag;
	uint64_t crc;
	uint32_t buf_oft;
	uint32_t i;
	uint8_t *pi;

	app_tag = wrap->apptag & (~wrap->appmask); /* clear mask bit */

	for (i = 0; i < wrap->nlb; i++) {
		buf_oft = i * (data->lbads + data->meta_size);
		crc = crc64_nvme64bcrc(wrap->buf + buf_oft, data->lbads + meta_oft);

		pi = wrap->buf + buf_oft + meta_oft;
		/* Guard */
		pi[0] = (crc >> 56) & 0xff;
		pi[1] = (crc >> 48) & 0xff;
		pi[2] = (crc >> 40) & 0xff;
		pi[3] = (crc >> 32) & 0xff;
		pi[4] = (crc >> 24) & 0xff;
		pi[5] = (crc >> 16) & 0xff;
		pi[6] = (crc >> 8) & 0xff;
		pi[7] = crc & 0xff;
		/* Application Tag */
		pi[8] = app_tag >> 8;
		pi[9] = app_tag & 0xff;
		/* Storage and Reference Space */
		pi[10] = (wrap->dw3 >> 8) & 0xff;
		pi[11] = wrap->dw3 & 0xff;
		pi[12] = (wrap->dw14 >> 24) & 0xff;
		pi[13] = (wrap->dw14 >> 16) & 0xff;
		pi[14] = (wrap->dw14 >> 8) & 0xff;
		pi[15] = wrap->dw14 & 0xff;
	}
}

static void fill_guard_protect_info(struct nvme_rwc_wrapper *wrap, 
	struct test_data *data)
{
	switch (data->cur_pif) {
	case NVME_ELBAF_PIF_TYPE1:
		BUG_ON(data->meta_size < 8);

		if (data->meta_size == 8 || data->is_pi_first)
			fill_16b_guard_protect_info(wrap, data, 0);
		else
			fill_16b_guard_protect_info(wrap, data, 
				data->meta_size - 8);
		break;
	
	case NVME_ELBAF_PIF_TYPE2:
		BUG_ON(data->meta_size < 16);

		if (data->meta_size == 16 || data->is_pi_first)
			fill_32b_guard_protect_info(wrap, data, 0);
		else
			fill_32b_guard_protect_info(wrap, data, 
				data->meta_size - 16);
		break;
	
	case NVME_ELBAF_PIF_TYPE3:
		BUG_ON(data->meta_size < 16);

		if (data->meta_size == 16 || data->is_pi_first)
			fill_64b_guard_protect_info(wrap, data, 0);
		else
			fill_64b_guard_protect_info(wrap, data, 
				data->meta_size - 16);
		break;
	}
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.control = NVME_RW_PRINFO_PRCHK_STC | NVME_RW_PRINFO_PRCHK_APP |
		NVME_RW_PRINFO_PRCHK_GUARD;
	if (test->is_set_pract)
		wrap.control |= NVME_RW_PRINFO_PRACT;

	wrap.apptag = test->app_tag;
	wrap.appmask = test->app_tag_mask;
	fill_cmd_tags(&wrap, test);

	wrap.buf = tool->wbuf;
	if (test->is_not_resident)
		wrap.size = wrap.nlb * test->lbads;
	else
		wrap.size = wrap.nlb * (test->lbads + test->meta_size);

	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	if (!test->is_set_pract)
		fill_guard_protect_info(&wrap, test);

	return nvme_io_write(ndev, &wrap);
}

/**
 * @brief 16b guard protection information with PRACT=0
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_16b_guard_action0(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_16b_guard) {
		pr_warn("Not support 16b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE1) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE1;
		test->is_match_pif = 1;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief 16b guard PI with PRACT=1 && metadata size == PI size
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_16b_guard_action1_size_equal(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_16b_guard) {
		pr_warn("Not support 16b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE1 || test->meta_size != 8) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE1;
		test->is_match_pif = 1;
		test->cfg_ms = 8;
		test->is_match_ms = TEST_MATCH_MS_EQUAL;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);
	test->is_set_pract = 1;
	test->is_not_resident = 1;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief 16b guard PI with PRACT=1 && metadata size > PI size
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_16b_guard_action1_size_larger(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_16b_guard) {
		pr_warn("Not support 16b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE1 || test->meta_size <= 8) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE1;
		test->is_match_pif = 1;
		test->cfg_ms = 8;
		test->is_match_ms = TEST_MATCH_MS_LARGER;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);
	test->is_set_pract = 1;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief 32b guard protection information with PRACT=0
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_32b_guard_action0(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_32b_guard) {
		pr_warn("Not support 32b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE2) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE2;
		test->is_match_pif = 1;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/*
	 * 32b Guard Protection Information shall only be available to 
	 * namespaces that have an LBA size greater than or equal to 4KiB.
	 */
	if (test->lbads < SZ_4K) {
		pr_err("LBA size:0x%x < 4KiB!\n", test->lbads);
		return -EPERM;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief 32b guard PI with PRACT=1 && metadata size == PI size
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_32b_guard_action1_size_equal(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_32b_guard) {
		pr_warn("Not support 32b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE2 || test->meta_size != 16) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE2;
		test->is_match_pif = 1;
		test->cfg_ms = 16;
		test->is_match_ms = TEST_MATCH_MS_EQUAL;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/*
	 * 32b Guard Protection Information shall only be available to 
	 * namespaces that have an LBA size greater than or equal to 4KiB.
	 */
	if (test->lbads < SZ_4K) {
		pr_err("LBA size:0x%x < 4KiB!\n", test->lbads);
		return -EPERM;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);
	test->is_set_pract = 1;
	test->is_not_resident = 1;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief 32b guard PI with PRACT=1 && metadata size > PI size
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_32b_guard_action1_size_larger(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_32b_guard) {
		pr_warn("Not support 32b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE2 || test->meta_size <= 16) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE2;
		test->is_match_pif = 1;
		test->cfg_ms = 16;
		test->is_match_ms = TEST_MATCH_MS_LARGER;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/*
	 * 32b Guard Protection Information shall only be available to 
	 * namespaces that have an LBA size greater than or equal to 4KiB.
	 */
	if (test->lbads < SZ_4K) {
		pr_err("LBA size:0x%x < 4KiB!\n", test->lbads);
		return -EPERM;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);
	test->is_set_pract = 1;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;	
}

/**
 * @brief 64b guard protection information with PRACT=0
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_64b_guard_action0(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_64b_guard) {
		pr_warn("Not support 64b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE3) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE3;
		test->is_match_pif = 1;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/*
	 * 64b Guard Protection Information shall only be available to 
	 * namespaces that have an LBA size greater than or equal to 4KiB.
	 */
	if (test->lbads < SZ_4K) {
		pr_err("LBA size:0x%x < 4KiB!\n", test->lbads);
		return -EPERM;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief 64b guard PI with PRACT=1 && metadata size == PI size
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_64b_guard_action1_size_equal(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_64b_guard) {
		pr_warn("Not support 64b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE3 || test->meta_size != 16) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE3;
		test->is_match_pif = 1;
		test->cfg_ms = 16;
		test->is_match_ms = TEST_MATCH_MS_EQUAL;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/*
	 * 64b Guard Protection Information shall only be available to 
	 * namespaces that have an LBA size greater than or equal to 4KiB.
	 */
	if (test->lbads < SZ_4K) {
		pr_err("LBA size:0x%x < 4KiB!\n", test->lbads);
		return -EPERM;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);
	test->is_set_pract = 1;
	test->is_not_resident = 1;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief 64b guard PI with PRACT=1 && metadata size > PI size
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_pi_64b_guard_action1_size_larger(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	if (!test->is_sup_64b_guard) {
		pr_warn("Not support 64b guard PI type!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (test->cur_pif != NVME_ELBAF_PIF_TYPE3 || test->meta_size <= 16) {
		test_data_reset_format_fields(test);
		test->cfg_pif = NVME_ELBAF_PIF_TYPE3;
		test->is_match_pif = 1;
		test->cfg_ms = 16;
		test->is_match_ms = TEST_MATCH_MS_LARGER;

		ret = select_ext_lba_format(ndev, test);
		if (ret < 0)
			goto out;
	}

	/*
	 * 64b Guard Protection Information shall only be available to 
	 * namespaces that have an LBA size greater than or equal to 4KiB.
	 */
	if (test->lbads < SZ_4K) {
		pr_err("LBA size:0x%x < 4KiB!\n", test->lbads);
		return -EPERM;
	}

	/* ensure "slba + nlb < nsze" */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;

	test_data_reset_rw_fields(test);
	test->is_set_pract = 1;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

static int case_e2e_data_protect(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	int ret;

	ret = init_test_data(tool, test);
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

	ret |= subcase_pi_16b_guard_action0(tool, sq);
	ret |= subcase_pi_16b_guard_action1_size_equal(tool, sq);
	ret |= subcase_pi_16b_guard_action1_size_larger(tool, sq);
	ret |= subcase_pi_32b_guard_action0(tool, sq);
	ret |= subcase_pi_32b_guard_action1_size_equal(tool, sq);
	ret |= subcase_pi_32b_guard_action1_size_larger(tool, sq);
	ret |= subcase_pi_64b_guard_action0(tool, sq);
	ret |= subcase_pi_64b_guard_action1_size_equal(tool, sq);
	ret |= subcase_pi_64b_guard_action1_size_larger(tool, sq);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_SYMBOL(case_e2e_data_protect, 
	"Test end-to-end protection mechanism in various scenarios");
