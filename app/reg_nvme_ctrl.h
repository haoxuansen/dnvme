#ifndef __REG_NVME_CTRL_H__
#define __REG_NVME_CTRL_H__

#include <stdint.h>

typedef union _reg_nvme_cap0 {
  struct
  {
    uint32_t cap_mqes : 16;
    uint32_t cap_cqr : 1;
    uint32_t cap_ams : 2;
    uint32_t cap_ams_rsvd : 5;
    uint32_t cap_to : 8;
  } bits;
  uint32_t config;
} reg_nvme_cap0;

typedef union _reg_nvme_cap1 {
  struct
  {
    uint32_t cap_dstrd : 4;
    uint32_t cap_nssrs : 1;
    uint32_t cap_css : 8;
    uint32_t cap_css_rsvd : 3;
    uint32_t cap_mpsmin : 4;
    uint32_t cap_mpsmax : 4;
    uint32_t cap_mpsmax_rsvd : 8;
  } bits;
  uint32_t config;
} reg_nvme_cap1;

typedef union _reg_nvme_vs {
  struct
  {
    uint32_t blank : 8;
    uint32_t vs_mnr : 8;
    uint32_t vs_mjr : 16;
  } bits;
  uint32_t config;
} reg_nvme_vs;

typedef union _reg_nvme_intm_set {
  struct
  {
    uint32_t intm_set : 32;
  } bits;
  uint32_t config;
} reg_nvme_intm_set;

typedef union _reg_nvme_intm_clr {
  struct
  {
    uint32_t intm_clr : 32;
  } bits;
  uint32_t config;
} reg_nvme_intm_clr;

typedef union _reg_nvme_cc {
  struct
  {
    uint32_t cc_en : 1;
    uint32_t cc_en_rsvd : 3;
    uint32_t cc_css : 3;
    uint32_t cc_mps : 4;
    uint32_t cc_ams : 3;
    uint32_t cc_shn : 2;
    uint32_t cc_iosqes : 4;
    uint32_t cc_iocqes : 4;
    uint32_t cc_iocqes_rsvd : 8;
  } bits;
  uint32_t config;
} reg_nvme_cc;

typedef union _reg_nvme_csts {
  struct
  {
    uint32_t csts_rdy : 1;
    uint32_t csts_cfs : 1;
    uint32_t csts_shst : 2;
    uint32_t csts_nssro : 1;
    uint32_t csts_pp : 1;
    uint32_t csts_pp_rsvd : 26;
  } bits;
  uint32_t config;
} reg_nvme_csts;

typedef union _reg_nvme_subsystem_reset {
  struct
  {
    uint32_t subs_rst : 32;
  } bits;
  uint32_t config;
} reg_nvme_subsystem_reset;

typedef union _reg_nvme_aqa {
  struct
  {
    uint32_t asqs : 12;
    uint32_t asqs_rsvd : 4;
    uint32_t acqs : 12;
    uint32_t acqs_rsvd : 4;
  } bits;
  uint32_t config;
} reg_nvme_aqa;

typedef union _reg_nvme_asq0 {
  struct
  {
    uint32_t blank : 12;
    uint32_t asqbl : 20;
  } bits;
  uint32_t config;
} reg_nvme_asq0;

typedef union _reg_nvme_asq1 {
  struct
  {
    uint32_t asqbh : 32;
  } bits;
  uint32_t config;
} reg_nvme_asq1;

typedef union _reg_nvme_acq0 {
  struct
  {
    uint32_t blank : 12;
    uint32_t acqbl : 20;
  } bits;
  uint32_t config;
} reg_nvme_acq0;

typedef union _reg_nvme_acq1 {
  struct
  {
    uint32_t acqbh : 32;
  } bits;
  uint32_t config;
} reg_nvme_acq1;

typedef union _reg_nvme_adm_tdbl {
  struct
  {
    uint32_t asqtb : 16;
    uint32_t asqtb_rsvd : 16;
  } bits;
  uint32_t config;
} reg_nvme_adm_tdbl;

typedef union _reg_nvme_adm_hdbl {
  struct
  {
    uint32_t acqhb : 16;
    uint32_t acqhb_rsvd : 16;
  } bits;
  uint32_t config;
} reg_nvme_adm_hdbl;

typedef union _reg_nvme_ioq1_tdbl {
  struct
  {
    uint32_t iosq1tb : 16;
    uint32_t iosq1tb_rsvd : 16;
  } bits;
  uint32_t config;
} reg_nvme_ioq1_tdbl;

typedef union _reg_nvme_ioq1_hdbl {
  struct
  {
    uint32_t iocq1hb : 16;
    uint32_t iocq1hb_rsvd : 16;
  } bits;
  uint32_t config;
} reg_nvme_ioq1_hdbl;

struct reg_nvme_ctrl
{
	reg_nvme_cap0 nvme_cap0;         // offset = 0x0
	reg_nvme_cap1 nvme_cap1;         // offset = 0x4
	reg_nvme_vs nvme_vs;             // offset = 0x8
	reg_nvme_intm_set nvme_intm_set; // offset = 0xc
	reg_nvme_intm_clr nvme_intm_clr; // offset = 0x10
	reg_nvme_cc nvme_cc;             // offset = 0x14
	uint32_t uint32_gap_0;
	reg_nvme_csts nvme_csts;                       // offset = 0x1c
	reg_nvme_subsystem_reset nvme_subsystem_reset; // offset = 0x20
	reg_nvme_aqa nvme_aqa;                         // offset = 0x24
	reg_nvme_asq0 nvme_asq0;                       // offset = 0x28
	reg_nvme_asq1 nvme_asq1;                       // offset = 0x2c
	reg_nvme_acq0 nvme_acq0;                       // offset = 0x30
	reg_nvme_acq1 nvme_acq1;                       // offset = 0x34
};

#endif
