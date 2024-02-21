/**
 * @file trace.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM nvme

#if !defined(_TRACE_NVME_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVME_H

#include <linux/tracepoint.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <asm/io.h>
#include "nvme.h"
#include "core.h"

DECLARE_EVENT_CLASS(nvme_log_proc,
	TP_PROTO(struct device *dev, const char *buf),
	TP_ARGS(dev, buf),
	TP_STRUCT__entry(
		__string(name, dev_name(dev))
		__string(cmd, buf)
	),
	TP_fast_assign(
		__assign_str(name, dev_name(dev));
		__assign_str(cmd, buf);
	),
	TP_printk("%s: %s", __get_str(name), __get_str(cmd))
);

DEFINE_EVENT(nvme_log_proc, dnvme_proc_write, 
	TP_PROTO(struct device *dev, const char *buf),
	TP_ARGS(dev, buf)
);

DECLARE_EVENT_CLASS(nvme_log_cmd,
	TP_PROTO(struct device *dev, struct nvme_common_command *ccmd, u16 sqid),
	TP_ARGS(dev, ccmd, sqid),
	TP_STRUCT__entry(
		__string(name, dev_name(dev))
		__field(u16, sqid)
		__field(u8, opcode)
		__field(u8, flags)
		__field(u16, cid)
		__field(u32, nsid)
		__field(u32, cdw2)
		__field(u32, cdw3)
		__field(u64, mptr)
		__field(u64, prp1)
		__field(u64, prp2)
		__field(u32, cdw10)
		__field(u32, cdw11)
		__field(u32, cdw12)
		__field(u32, cdw13)
		__field(u32, cdw14)
		__field(u32, cdw15)
	),
	TP_fast_assign(
		__assign_str(name, dev_name(dev));
		__entry->sqid = sqid;
		__entry->opcode = ccmd->opcode;
		__entry->flags = ccmd->flags;
		__entry->cid = ccmd->command_id;
		__entry->nsid = le32_to_cpu(ccmd->nsid);
		__entry->cdw2 = le32_to_cpu(ccmd->cdw2[0]);
		__entry->cdw3 = le32_to_cpu(ccmd->cdw2[1]);
		__entry->mptr = le64_to_cpu(ccmd->metadata);
		__entry->prp1 = le64_to_cpu(ccmd->dptr.prp1);
		__entry->prp2 = le64_to_cpu(ccmd->dptr.prp2);
		__entry->cdw10 = le32_to_cpu(ccmd->cdw10);
		__entry->cdw11 = le32_to_cpu(ccmd->cdw11);
		__entry->cdw12 = le32_to_cpu(ccmd->cdw12);
		__entry->cdw13 = le32_to_cpu(ccmd->cdw13);
		__entry->cdw14 = le32_to_cpu(ccmd->cdw14);
		__entry->cdw15 = le32_to_cpu(ccmd->cdw15);
	),
	TP_printk("%s->%u: opcode:%02x flags:%02x cid:%04x nsid:%08x "
		"dw2-3:%08x %08x mptr:%016llx prp1:%016llx prp2:%016llx "
		"dw10-15:%08x %08x %08x %08x %08x %08x", 
		__get_str(name), __entry->sqid, 
		__entry->opcode, __entry->flags, __entry->cid, __entry->nsid,
		__entry->cdw2, __entry->cdw3, __entry->mptr, __entry->prp1,
		__entry->prp2, __entry->cdw10, __entry->cdw11, __entry->cdw12,
		__entry->cdw13, __entry->cdw14, __entry->cdw15)
);

DEFINE_EVENT(nvme_log_cmd, dnvme_submit_64b_cmd, 
	TP_PROTO(struct device *dev, struct nvme_common_command *ccmd, u16 sqid),
	TP_ARGS(dev, ccmd, sqid)
);

DECLARE_EVENT_CLASS(nvme_log_sgl,
	TP_PROTO(struct nvme_sgl_desc *sge),
	TP_ARGS(sge),
	TP_STRUCT__entry(
		__field(u64, addr)
		__field(u32, length)
		__array(u8, rsvd, 3)
		__field(u8, type)
	),
	TP_fast_assign(
		__entry->addr = le64_to_cpu(sge->addr);
		__entry->length = le32_to_cpu(sge->length);
		memcpy(__entry->rsvd, sge->rsvd, 3);
		__entry->type = sge->type;
	),
	TP_printk("addr:%016llx length:%08x rsvd:%02x %02x %02x type:%02x",
		__entry->addr, __entry->length, __entry->rsvd[0], 
		__entry->rsvd[1], __entry->rsvd[2], __entry->type)
);

DEFINE_EVENT(nvme_log_sgl, dnvme_sgl_set_data, 
	TP_PROTO(struct nvme_sgl_desc *sge),
	TP_ARGS(sge)
);

DEFINE_EVENT(nvme_log_sgl, dnvme_sgl_set_bit_bucket, 
	TP_PROTO(struct nvme_sgl_desc *sge),
	TP_ARGS(sge)
);

DEFINE_EVENT(nvme_log_sgl, dnvme_sgl_set_seg, 
	TP_PROTO(struct nvme_sgl_desc *sge),
	TP_ARGS(sge)
);

DECLARE_EVENT_CLASS(nvme_log_prp,
	TP_PROTO(int idx, dma_addr_t addr, int len),
	TP_ARGS(idx, addr, len),
	TP_STRUCT__entry(
		__field(int, idx)
		__field(u64, addr)
		__field(int, len)
	),
	TP_fast_assign(
		__entry->idx = idx;
		__entry->addr = addr;
		__entry->len = len;
	),
	TP_printk("[%02x] %016llx + %x", 
		__entry->idx, __entry->addr, __entry->len)
);

DEFINE_EVENT(nvme_log_prp, dnvme_setup_prps, 
	TP_PROTO(int idx, dma_addr_t addr, int len),
	TP_ARGS(idx, addr, len)
);

DECLARE_EVENT_CLASS(nvme_log_cqe,
	TP_PROTO(struct nvme_completion *cqe),
	TP_ARGS(cqe),
	TP_STRUCT__entry(
		__field(u64, addr_phys)
		__field(u64, result)
		__field(u16, sq_head)
		__field(u16, sq_id)
		__field(u16, cid)
		__field(u16, status)
	),
	TP_fast_assign(
		__entry->addr_phys = virt_to_phys(cqe);
		__entry->result = le64_to_cpu(cqe->result.u64);
		__entry->sq_head = le16_to_cpu(cqe->sq_head);
		__entry->sq_id = le16_to_cpu(cqe->sq_id);
		__entry->cid = cqe->command_id;
		__entry->status = le16_to_cpu(cqe->status);
	),
	TP_printk("|%016llx| result:%016llx sq_head:%04x sq_id:%04x cid:%04x status:%04x",
		__entry->addr_phys, __entry->result, __entry->sq_head, __entry->sq_id, 
		__entry->cid, __entry->status)
);

DEFINE_EVENT(nvme_log_cqe, handle_cmd_completion, 
	TP_PROTO(struct nvme_completion *cqe),
	TP_ARGS(cqe)
);

/* Timestamp is automatically recorded, so there is no need to repeatedly record */
DECLARE_EVENT_CLASS(nvme_log_irq,
	TP_PROTO(struct nvme_irq_set *irq_set, int vec),
	TP_ARGS(irq_set, vec),
	TP_STRUCT__entry(
		__string(name, irq_set->irq_name)
		__field(int, vec)
	),
	TP_fast_assign(
		__assign_str(name, irq_set->irq_name);
		__entry->vec = vec;
	),
	TP_printk("%8s %d", __get_str(name), __entry->vec)
);

DEFINE_EVENT(nvme_log_irq, dnvme_interrupt, 
	TP_PROTO(struct nvme_irq_set *irq_set, int vec),
	TP_ARGS(irq_set, vec)
);

#endif /* _TRACE_NVME_H */

/* this part must be outside header guard */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
