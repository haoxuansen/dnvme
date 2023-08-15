/**
 * @file property.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief NVM Controller Properities
 * @version 0.1
 * @date 2023-01-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_NVME_PROPERTY_H_
#define _UAPI_NVME_PROPERTY_H_

/**
 * @brief The property map for the controller
 */
enum {
	NVME_REG_CAP	= 0x0000,	/* Controller Capabilities */
	NVME_REG_VS	= 0x0008,	/* Version */
	NVME_REG_INTMS	= 0x000c,	/* Interrupt Mask Set */
	NVME_REG_INTMC	= 0x0010,	/* Interrupt Mask Clear */
	NVME_REG_CC	= 0x0014,	/* Controller Configuration */
	NVME_REG_CSTS	= 0x001c,	/* Controller Status */
	NVME_REG_NSSR	= 0x0020,	/* NVM Subsystem Reset */
	NVME_REG_AQA	= 0x0024,	/* Admin Queue Attributes */
	NVME_REG_ASQ	= 0x0028,	/* Admin SQ Base Address */
	NVME_REG_ACQ	= 0x0030,	/* Admin CQ Base Address */
	NVME_REG_CMBLOC	= 0x0038,	/* Controller Memory Buffer Location */
	NVME_REG_CMBSZ	= 0x003c,	/* Controller Memory Buffer Size */
	NVME_REG_BPINFO	= 0x0040,	/* Boot Partition Information */
	NVME_REG_BPRSEL	= 0x0044,	/* Boot Partition Read Select */
	NVME_REG_BPMBL	= 0x0048,	/* Boot Partition Memory Buffer
					 * Location
					 */
	NVME_REG_CMBMSC = 0x0050,	/* Controller Memory Buffer Memory
					 * Space Control
					 */
	NVME_REG_PMRCAP	= 0x0e00,	/* Persistent Memory Capabilities */
	NVME_REG_PMRCTL	= 0x0e04,	/* Persistent Memory Region Control */
	NVME_REG_PMRSTS	= 0x0e08,	/* Persistent Memory Region Status */
	NVME_REG_PMREBS	= 0x0e0c,	/* Persistent Memory Region Elasticity
					 * Buffer Size
					 */
	NVME_REG_PMRSWTP = 0x0e10,	/* Persistent Memory Region Sustained
					 * Write Throughput
					 */
	NVME_REG_PMRMSCL = 0x0e14,	/* Persistent Memory Region Memory
					 * Space Control Lower
					 */
	NVME_REG_PMRMSCU = 0x0e18,	/* Persistent Memory Region Memory
					 * Space Control Upper
					 */
	NVME_REG_DBS	= 0x1000,	/* SQ 0 Tail Doorbell */
};

/* NVME_REG_CAP */
#define NVME_CAP_MQES(cap)		((cap) & 0xffff)
/* bit[16] Configuous Queues Required */
#define NVME_CAP_CQR(cap)		(((cap) >> 16) & 0x1)

#define NVME_CAP_AMS(cap)		(((cap) >> 17) & 0x3)
enum {
	NVME_CAP_AMS_WRRUPC	= 1 << 0,
	NVME_CAP_AMS_VENDOR	= 1 << 1,
};

#define NVME_CAP_TIMEOUT(cap)		(((cap) >> 24) & 0xff)
#define NVME_CAP_STRIDE(cap)		(((cap) >> 32) & 0xf)
#define NVME_CAP_NSSRC(cap)		(((cap) >> 36) & 0x1)

#define NVME_CAP_CSS(cap)		(((cap) >> 37) & 0xff)
enum {
	NVME_CAP_CSS_NVM	= 1 << 0,
	NVME_CAP_CSS_CSI	= 1 << 6,
};

#define NVME_CAP_BPS(cap)		(((cap) >> 45) & 0x1)

#define NVME_CAP_CPS(cap)		(((cap) >> 46) & 0x3)
enum {
	NVME_CAP_CPS_UNKNOWN	= 0,
	NVME_CAP_CPS_CTRL	= 1,
	NVME_CAP_CPS_DOMAIN	= 2,
	NVME_CAP_CPS_SUBSYSTEM	= 3,
};

#define NVME_CAP_MPSMIN(cap)		(((cap) >> 48) & 0xf)
#define NVME_CAP_MPSMAX(cap)		(((cap) >> 52) & 0xf)
#define NVME_CAP_PMRS(cap)		(((cap) >> 56) & 0x1)
#define NVME_CAP_CMBS(cap)		(((cap) >> 57) & 0x1)
#define NVME_CAP_NSSS(cap)		(((cap) >> 58) & 0x1)
#define NVME_CAP_CRMS(cap)		(((cap) >> 59) & 0x3)
enum {
	NVME_CAP_CRMS_CRWMS	= 1 << 0,
	NVME_CAP_CRMS_CRIMS	= 1 << 1,
};

/* NVME_REG_VS */
#define NVME_VS_TER(vs)			((vs) & 0xff) /* tertiary */
#define NVME_VS_MNR(vs)			(((vs) >> 8) & 0xff) /* minor */
#define NVME_VS_MJR(vs)			(((vs) >> 16) & 0xffff) /* major */

#define NVME_VS(major, minor, tertiary) \
	(((major) << 16) | ((minor) << 8) | (tertiary))

/* NVME_REG_INTMS */
/* NVME_REG_INTMC */

/* NVME_REG_CC */
#define NVME_CC_ENABLE			BIT(0)
/* bit[6:4] I/O Command Set Selected */
#define NVME_CC_FOR_CSS(css)		(((css) & 0x7) << 4)
#define NVME_CC_TO_CSS(cc)		(((cc) >> 4) & 0x7)
#define NVME_CC_CSS_NVM			0
#define NVME_CC_CSS_CSI			6
#define NVME_CC_CSS_ADMIN		7

#define NVME_CC_FOR_MPS(mps)		(((mps) & 0xf) << 7)
#define NVME_CC_TO_MPS(cc)		(((cc) >> 7) & 0xf)
#define NVME_CC_AMS_SHIFT		11
#define NVME_CC_SHN_SHIFT		14
#define NVME_CC_FOR_IOSQES(sqes)	(((sqes) & 0xf) << 16)
#define NVME_CC_TO_IOSQES(cc)		(((cc) >> 16) & 0xf)
#define NVME_CC_IOSQES_MASK		NVME_CC_FOR_IOSQES(0xf)
#define NVME_CC_FOR_IOCQES(cqes)	(((cqes) & 0xf) << 20)
#define NVME_CC_TO_IOCQES(cc)		(((cc) >> 20) & 0xf)
#define NVME_CC_IOCQES_MASK		NVME_CC_FOR_IOCQES(0xf)

/**
 * @brief Arbitration Mechanism Selected
 */
enum {
	NVME_CC_AMS_RR		= 0 << NVME_CC_AMS_SHIFT,
	NVME_CC_AMS_WRRU	= 1 << NVME_CC_AMS_SHIFT,
	NVME_CC_AMS_VS		= 7 << NVME_CC_AMS_SHIFT,
};

/**
 * @brief Shutdown Notification
 */
enum {
	NVME_CC_SHN_NONE	= 0 << NVME_CC_SHN_SHIFT,
	NVME_CC_SHN_NORMAL	= 1 << NVME_CC_SHN_SHIFT,
	NVME_CC_SHN_ABRUPT	= 2 << NVME_CC_SHN_SHIFT,
	NVME_CC_SHN_MASK	= 3 << NVME_CC_SHN_SHIFT,
};

/* NVME_REG_CSTS */
#define NVME_CSTS_RDY			BIT(0)
#define NVME_CSTS_CFS			BIT(1)
#define NVME_CSTS_SHST_SHIFT		2
#define NVME_CSTS_NSSRO			BIT(4)
#define NVME_CSTS_PP			BIT(5)
#define NVME_CSTS_ST			BIT(6)

/**
 * @brief Shutdown Status
 */
enum {
	NVME_CSTS_SHST_NORMAL	= 0 << NVME_CSTS_SHST_SHIFT,
	NVME_CSTS_SHST_OCCUR	= 1 << NVME_CSTS_SHST_SHIFT,
	NVME_CSTS_SHST_CMPLT	= 2 << NVME_CSTS_SHST_SHIFT,
	NVME_CSTS_SHST_MASK	= 3 << NVME_CSTS_SHST_SHIFT,
};

/* NVME_REG_NSSR */

/* NVME_REG_AQA */
/* bit[11:0] Admin Submission Queue Size (in entries, zero based) */
#define NVME_AQA_FOR_ASQS(aqa)		(((aqa) & 0xfff) << 0)
#define NVME_AQA_TO_ASQS(aqa)		(((aqa) >> 0) & 0xfff)
#define NVME_AQA_ASQS_MASK		NVME_AQA_FOR_ASQS(0xfff)
/* bit[27:16] Admin Completion Queue Size (in entries, zero based) */
#define NVME_AQA_FOR_ACQS(aqa)		(((aqa) & 0xfff) << 16)
#define NVME_AQA_TO_ACQS(aqa)		(((aqa) >> 16) & 0xfff)
#define NVME_AQA_ACQS_MASK		NVME_AQA_FOR_ACQS(0xfff)

/* NVME_REG_ASQ */
/* NVME_REG_ACQ */

/* NVME_REG_CMBLOC */
#define NVME_CMBLOC_BIR(cmbloc)		((cmbloc) & 0x7)
#define NVME_CMBLOC_CQMMS		BIT(3)
#define NVME_CMBLOC_CQPDS		BIT(4)
#define NVME_CMBLOC_CDPMLS		BIT(5)
#define NVME_CMBLOC_CDPCILS		BIT(6)
#define NVME_CMBLOC_CDMMMS		BIT(7)
#define NVME_CMBLOC_CQDA		BIT(8)
#define NVME_CMBLOC_OFST(cmbloc)	(((cmbloc) >> 12) & 0xfffff)

/* NVME_REG_CMBSZ */
#define NVME_CMBSZ_SQS			BIT(0)
#define NVME_CMBSZ_CQS			BIT(1)
#define NVME_CMBSZ_LISTS		BIT(2)
#define NVME_CMBSZ_RDS			BIT(3)
#define NVME_CMBSZ_WDS			BIT(4)
#define NVME_CMBSZ_SZU(cmbsz)		(((cmbsz) >> 8) & 0xf)
#define NVME_CMBSZ_SZ(cmbsz)		(((cmbsz) >> 12) & 0xfffff)

/* NVME_REG_BPINFO */
/* NVME_REG_BPRSEL */
/* NVME_REG_BPMBL */

/* NVME_REG_CMBMSC */
#define NVME_CMBMSC_CRE			BIT(0)
#define NVME_CMBMSC_CMSE		BIT(1)

/* NVME_REG_PMRCAP */
#define NVME_PMRCAP_RDS			BIT(3)
#define NVME_PMRCAP_WDS			BIT(4)
#define NVME_PMRCAP_BIR(cap)		(((cap) >> 5) & 0x7)
#define NVME_PMRCAP_PMRTU(cap)		(((cap) >> 8) & 0x3)
#define NVME_PMRCAP_PMRWBM(cap)		(((cap) >> 10) & 0xf)
#define NVME_PMRCAP_PMRTO(cap)		(((cap) >> 16) & 0xff)
#define NVME_PMRCAP_CMSS		BIT(24)

/* NVME_REG_PMRCTL */
#define NVME_PMRCTL_EN			BIT(0)

/* NVME_REG_PMRSTS */
#define NVME_PMRSTS_ERR(sts)		((sts) & 0xff)
#define NVME_PMRSTS_NRDY		BIT(8)
#define NVME_PMRSTS_HSTS(sts)		(((sts) >> 9) & 0x7)
/* bit[12] Controller Base Address Invalid */
#define NVME_PMRSTS_CBAI		BIT(12)

/* NVME_REG_PMREBS */
/* NVME_REG_PMRSWTP */

/* NVME_REG_PMRMSCL */
#define NVME_PMRMSCL_CMSE		BIT(1)
#define NVME_PMRMSCL_RSVD		0xffd

#endif /* !_UAPI_NVME_PROPERTY_H_ */
