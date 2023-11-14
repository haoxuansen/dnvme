
Case
====

Vendor Case
-----------

| Vendor command 的通用格式如下表所示：

.. doxygenstruct:: nvme_maxio_common_cmd

.. note::
	1. 除特殊说明外，默认“发送命令”会包括取回 CQE，并对 Entry 内容进行检查。
	#. 默认“提交命令”只是将准备好的命令放到 SQ 队列中，不包括敲 SQ doorbell。

case_hw_io_cmd_timeout_check
""""""""""""""""""""""""""""

| 在配置 Hardware I/O command timeout 定时器后，确认该 feature 是否生效。

.. doxygenfunction:: case_hw_io_cmd_timeout_check
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_nvme_top` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 上述命令处理结束后，主机等待 1s
#. 主机随机发送 1 条 I/O Read 或 Write 命令；
#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_top` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_cq_full_threshold_limit_sq_fetch_cmd
"""""""""""""""""""""""""""""""""""""""""

| 在配置 CQ Full 阈值后，测试是否有效限制 EP 单次从 SQ 取 Command 的数量。

.. doxygenfunction:: case_cq_full_threshold_limit_sq_fetch_cmd
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_nvme_cqm` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
	a. param 表示 CQ Size，取值范围是 [1, 10]
#. 主机创建 I/O CQ 和 SQ，其中 CQ Size 必须和前面 param 值相同。
#. 主机随机提交 50 条 I/O Read 或 Write 命令。
#. 主机敲 SQ doorbell 触发 EP 处理 command，并等待接收所有的 CQE。
	a. 主机无需检查 CQ Entry 的内容。
#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_cqm` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_hw_rdma_ftl_size_limit
"""""""""""""""""""""""""""
| 限制 Hardware RDMA 中可以 pending 的 FTL 数量上限。

.. doxygenfunction:: case_hw_rdma_ftl_size_limit
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwrdma` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
	a. param 表示 FTL 可以 pending 的数量，取值范围是 [1, 10]
#. 上述命令处理结束后，主机等待 1s
#. 主机提交超过 param 值 2 倍的 I/O read command，并指定 NLB = 1
#. 主机敲 SQ doorbell 触发 EP 处理 command，并等待接收所有的 CQE。
	a. 主机无需检查 CQ Entry 的内容。
#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwrdma` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_hw_rdma_ftl_rreq_if_en
"""""""""""""""""""""""""""

.. doxygenfunction:: case_hw_rdma_ftl_rreq_if_en
	:project: unittest

| 操作步骤如下：

1. 主机创建 I/O CQ 和 SQ
#. param 选择依次打开 1,2,…6 个 FTL IF, 每次循环执行以下操作
	a. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwrdma` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
	#. 上述命令处理结束后，主机等待 1s
	#. 主机发送 12 条 I/O read command，并指定 NLB = 1
	#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwrdma` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

.. note::
	param bit[5:0] 中每个 bit 表示 FTL IF 的状态: 1 - enable, 0 - disable

case_hw_rdma_ftl_if_namespace_bind
""""""""""""""""""""""""""""""""""

.. doxygenfunction:: case_hw_rdma_ftl_if_namespace_bind
	:project: unittest

| 操作步骤如下：

1. 随机选择 1 个 namespace 和 1 个 FTL 接口
#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwrdma` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`，绑定 namespace 和 FTL 接口
#. 主机发送 12 条 I/O read command，并指定 NLB = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwrdma` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_hw_wdma_ftl_size_limit
"""""""""""""""""""""""""""

.. doxygenfunction:: case_hw_wdma_ftl_size_limit
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwwdma` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
	a. param 表示 FTL 可以 pending 的数量，取值范围是 [1, 0x10]
#. 上述命令处理结束后，主机等待 1s
#. 主机发送超过 param 值两倍的 I/O write command，并指定 NLB = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwwdma` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_hw_wdma_ftl_wreq_if_en
"""""""""""""""""""""""""""

.. doxygenfunction:: case_hw_wdma_ftl_wreq_if_en
	:project: unittest

| 操作步骤如下：

1. 主机创建 I/O CQ 和 SQ
#. param 选择依次打开 1,2,…6 个 FTL IF, 每次循环执行以下操作
	a. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwwdma` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
	#. 上述命令处理结束后，主机等待 1s
	#. 主机发送 12 条 I/O write command，并指定 NLB = 1
	#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_hwwdma` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

.. note::
	param bit[5:0] 中每个 bit 表示 FTL IF 的状态: 1 - enable, 0 - disable

case_wrr_with_urgent_priority_class_arbitration
"""""""""""""""""""""""""""""""""""""""""""""""

.. doxygenfunction:: case_wrr_with_urgent_priority_class_arbitration
	:project: unittest

| 操作步骤如下：

- 循环执行以下步骤 10 次
	1. 主机发送 :enumerator:`nvme_admin_maxio_nvme_case` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`, 参数如下：
		a. param 为 I/O Queue 对的数量，取值范围是 [1, 64]
		#. cdw11 选择“仲裁行为分析子程序”，取值 0 或 1
	#. 主机发送 Set Feature - Arbitration 命令，参数如下：
		a. HPW 取值范围是 [0, 100]
		#. MPW 取值范围是 [0, 100]
		#. LPW 取值范围是 [0, 100]
		#. Burst 取值范围是 2 ^ [0, 6] 对应 1 ~ 64
	#. 主机创建 param 指定数量的 I/O queue，随机指定 SQ 的优先级
	#. 主机向所有 I/O SQ 中随机提交 [1, 100] 条命令，命令类型随机
		a. 当前支持 read/write/compare 命令
	#. 主机敲所有 SQ 的 doorbell，并等待接收所有的 CQE。
		a. 主机无需检查 CQ Entry 的内容。
	#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_case` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`，接收 16KB 数据
		a. param 为接收的数据长度
	#. 主机将 16KB 数据以二进制的方式保存到以“case 名称 + 序号”方式命名的文件中

case_cmd_sanity_check_according_by_protocol
"""""""""""""""""""""""""""""""""""""""""""

.. doxygenfunction:: case_cmd_sanity_check_according_by_protocol
	:project: unittest

| 操作步骤如下：

- 循环执行以下步骤 10 次
	1. 主机发送 :enumerator:`nvme_admin_maxio_nvme_case` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`, 参数如下：
		a. param 表示 subcase 的序号，当前固定值为 1，后续可能会进行扩展。
		#. cdw11 表示 I/O command 的序号
		#. cdw12 由选中的 I/O command 序号决定其含义
	#. 主机发送 param 对应的 I/O command
	#. 重复步骤 1~2，直到发送完所有的 :ref:`label_case_cmd_sanity_check_according_by_protocol`
	#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_case` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`，接收 16KB 数据
		a. param 为接收的数据长度
	#. 主机将 16KB 数据以二进制的方式保存到以“case 名称 + 序号”方式命名的文件中

.. _label_case_cmd_sanity_check_according_by_protocol:

.. csv-table:: I/O command
	:header: "SeqNum", "Command", "Requirement"
	:widths: 10, 45, 45

	"1", "Fused<1st:Compare, 2nd:Write>"
	"2", "Fused<1st:Write, 2nd:Compare>"
	"3", "Fused<1st:Compare, Read>"
	"4", "Fused<Read, 2nd:Write>"
	"5", "Fused<1st:Compare, 2nd:Write>", "Compare 和 Write 需指定不同的 LBA Range"
	"6", "Fused<1st:Compare, 2nd:Write>", "NLB>10"
	"7", "Fused<1st:Compare, 2nd:Write>", "SLBA=0, NLB=10"
	"8", "Fused<2nd:Write, 1st:Compare>"
	"9", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` "
	"10", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_40B` "
	"11", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` , Number of Ranges>10 "
	"12", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_40B` "
	"13", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` "
	"14", "Copy", "dw12 PRINFOR.PRACT=0, PRINFOW.PRACT=1"
	"15", "Copy", "dw12 PRINFOR.PRACT=1, PRINFOW.PRACT=0"
	"16", "Copy", "dw12 PRINFOR.PRACT=0, PRINFOW.PRACT=0"
	"17", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` "
	"18", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` ，Source Range 指定的 NLB> :term:`MSSRL` (50)"
	"19", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` ，所有Source Range 指定的 NLB 总和> :term:`MCL` (50)"
	"20", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` "
	"21", "Copy", "desc format= :enumerator:`NVME_COPY_DESC_FMT_32B` , desc 的 LBA Range 超过 NSZE"
	"22", "Verify"
	"23", "Verify", "NLB>16"
	"24", "Verify", "dw12 PRINFO.PRACT=1"
	"25", "Compare"
	"26", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", ":term:`PSDT` =1"
	"27", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "PSDT=2"
	"28", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>, Verify]"
	"29", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>, Verify]"
	"30", "[Write, Copy, ZNS Append]"
	"31", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>, Verify]", "SLBA+NLB>NSZE"
	"32", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>, Verify]", "NSID>16"
	"33", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", ":term:`PSDT` ≠0"
	"34", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "PRP 非 dword 对齐!"
	"35", "ZNS Append"
	"36", "ZNS Append", "遍历以下场景：1) PI Type1, dw12 PIREMAP=0; 2) PI Type3, dw12 PIREMAP=1 :strong:`注意：` 配置 vendor cmd dw12 来选择当前生效的场景，dw12=1 对应场景1，dw12=2 对应场景2"
	"37", "[Write, Copy]", "DTYPE= :enumerator:`NVME_DIR_STREAMS` "
	"38", "[Write, Copy]", "DTYPE= :enumerator:`NVME_DIR_STREAMS` , DSPEC>200"
	"39", "[Write, Copy]", "DTYPE= :enumerator:`NVME_DIR_STREAMS` , DSPEC=100"
	"40", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]"
	"41", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]"
	"42", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]"

.. note::
	1. Fused<1st:Compare, 2nd:Write> 表示 Fused 由 Compare 和 Write 两条命令组成。
		a. 1st 表示此命令配置为 Fused operation, first command
		#. 2nd 表示此命令配置为 Fused operation, second command
		#. 无标注的话，此命令配置为 Normal operation
	#. [Read, Write] 表示从 Read 和 Write 中随机选择 1 条命令发送
	#. 上述 I/O command 默认不检查 CQ Entry 中的内容。

case_ftl_interface_selectable_by_multi_mode
"""""""""""""""""""""""""""""""""""""""""""

.. doxygenfunction:: case_ftl_interface_selectable_by_multi_mode
	:project: unittest

| 操作步骤如下：

- 循环执行以下步骤 50 次
	1. 主机发送 :enumerator:`nvme_admin_maxio_nvme_case` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`, 参数如下：
		a. param 表示 subcase 的序号
	#. 主机发送 param 对应的 I/O command
	#. 重复步骤 1~2，直到发送完所有的 :ref:`label_case_ftl_interface_selectable_by_multi_mode`
	#. 主机发送 :enumerator:`nvme_admin_maxio_nvme_case` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`，接收 16KB 数据
		a. param 为接收的数据长度
	#. 主机将 16KB 数据以二进制的方式保存到以“case 名称 + 序号”方式命名的文件中

.. _label_case_ftl_interface_selectable_by_multi_mode:

.. csv-table:: I/O command
	:header: "Subcase", "Command", "Requirement", "Note"
	:widths: 5, 40, 40, 15

	"1", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]"
	"2", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "发送 16 条命令，依次配置 NSID=1,2,...16"
	"3", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "提交 11 条命令后一起敲 doorbell", "不支持 copy 命令 [#1]_ "
	"4", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "SLBA[0, 50], 确保 SLBA + NLB ≤ NSZE"
	"5", "[Write, Copy]", "dtype= :enumerator:`NVME_DIR_STREAMS` , dspec=100"
	"6", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "提交 101 条命令后一起敲 doorbell", "不支持 copy 命令 [#1]_ "
	"7", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "提交 11 条相同的命令后一起敲 doorbell"
	"8", "[Read, Write, Compare, Copy, Fused<1st:Compare, 2nd:Write>]", "SLBA[0, 50], NLB[1, 50], 确保 SLBA + NLB ≤ NSZE"

.. note::
	1. Fused<1st:Compare, 2nd:Write> 表示 Fused 由 Compare 和 Write 两条命令组成。
		a. 1st 表示此命令配置为 Fused operation, first command
		#. 2nd 表示此命令配置为 Fused operation, second command
		#. 无标注的话，此命令配置为 Normal operation
	#. [Read, Write] 表示从 Read 和 Write 中随机选择 1 条命令发送
	#. 上述 I/O command 默认不检查 CQ Entry 中的内容。

case_fwdma_buf2buf_test
"""""""""""""""""""""""

.. doxygenfunction:: case_fwdma_buf2buf_test
	:project: unittest

| 操作步骤如下：

- 循环执行以下步骤 1000 次
	1. 主机发送 :enumerator:`nvme_admin_maxio_fwdma_fwdma` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`，command 其它字段要求如下：
		a. param: 随机选择 opcode
			- bit[0]: 0 表示 opcode 2, 1 表示 opcode 10
		#. cdw13: SLBA bits[31:00], :math:`SLBA + data\_len < NSZE`
		#. cdw14: SLBA bits[63:32]
		#. cdw15: data_len(unit: Byte), 要求按 4B 对齐，且小于 128KB

.. note::

	主机不需要准备 Host Buffer.


case_fwdma_buf2buf_bufpoint
"""""""""""""""""""""""""""

.. doxygenfunction:: case_fwdma_buf2buf_bufpoint
	:project: unittest

| 操作步骤如下：

- 循环执行以下步骤 1000 次
	1. 主机发送 :enumerator:`nvme_admin_maxio_fwdma_fwdma` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_GET_PARAM`
	#. 主机解析前一条 vendor command 对应的 CQ entry 数据
		a. dw0: bit0=0 表示 buf_size 为 4KB，bit0=1 表示 buf_size 为 8 KB
	#. 主机发送 :enumerator:`nvme_admin_maxio_fwdma_fwdma` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`，command 其它字段要求如下：
		a. :ref:`param <label_case_fwdma_buf2buf_bufpoint>`: bit[0], bit[1], bit[2] 随机选择，bit[3] 由前面解析到的 buf_size 决定，若前面为 4KB, 则实际配置 8KB，反之亦然 
		#. cdw13: SLBA bits[31:00], :math:`SLBA + buf\_size < NSZE`
		#. cdw14: SLBA bits[63:32]
		#. cdw15: cdw15: data_len(unit: Byte), 要求按 4B 对齐, :math:`buf\_oft + data\_len < buf\_size`

.. _label_case_fwdma_buf2buf_bufpoint:

.. csv-table:: Fields for Parameter
	:header: "Field", "Name", "Description"
	:widths: 10, 30, 60

	"bit[0]", "Opcode", "0 表示 opcode2，1 表示 opcode10"
	"bit[1]", "Target Address Type", "0 表示 bufpoint 模式，1 表示物理地址模式"
	"bit[2]", "Source Buffer Release Mode", "0 表示 FW 释放 source buffer，1 表示  :abbr:`DPU (Data Path Unit)` 释放 source buffer"
	"bit[3]", "buf_size: Buffer Size", "0 表示 4KB，1 表示 8KB"
	"bit[15:4]", "Reserved"
	"bit[31:16]", "buf_oft: Buffer Offset", "偏移地址，单位：Byte"

.. note::

	主机不需要准备 Host Buffer.


case_cfgwr_interrupt
""""""""""""""""""""

验证主机配置 device core 寄存器时会上报中断给 FW。

.. doxygenfunction:: case_cfgwr_interrupt
	:project: unittest

| 操作步骤如下：

1. 主机解析 device 支持的 PCI&PCIe Capability 的偏移地址。按 dword 访问的方式遍历 Configuration Space Header 和 Capability 区域，循环执行步骤
	a. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`, 参数如下：
		- param: 寄存器地址（相对于配置空间起始地址的偏移量）
	#. 主机等待 100ms
	#. 主机读 param 指定的寄存器，接着将读取到的值写回寄存器
	#. 主机等待 100ms
	#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_ltssm_state_change_interrupt
"""""""""""""""""""""""""""""""""

验证 device LTSSM 状态变化时会上报中断给 FW。

.. doxygenfunction:: case_ltssm_state_change_interrupt
	:project: unittest

| 操作步骤如下：

- 循环执行以下步骤，遍历所有 param
	1. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`, 参数如下：
		- param: 0x1, 0x102, 0x204, 0x407, 0x708, 0x809, 0x90a, 0xa0b, 0xb0c, 0xc11, 0x110d, 0xd0e, 0xe0d, 0xd0f, 0xf10, 0xf07, 0x1011, 0x1113, 0x1314, 0x140d
	#. 主机等待 100ms
	#. 主机发起降速，配置 device 的 Target Link Speed 字段的值为 1
		- PCI Experess Capability Structure → EP Link Control 2 Register bit[3:0]
	#. 主机重新link，配置 device link partner 的 Retrain Link 字段的值为 1
		- PCI Experess Capability Structure → Link Control Register bit[5]
	#. 主机等待 10ms
	#. 主机发起降 lane，配置 device 寄存器 0x8c0 的值为 1
		- 0x8c0 是相对于配置空间起始地址的偏移量，该寄存器为 device 私有，PCIe spec 中无此寄存器，按 dword 方式访问该寄存器。
	#. 主机重新link，配置 device link partner 的 Retrain Link 字段的值为 1
	#. 主机等待 10ms
	#. 主机发起 hot reset，配置 device link partner 的 Secondary Bus Reset 字段的值为 1，等待 100ms 后将此字段清 0
		- PCI Configuration Space Header → Bridge Control Register bit[6]
	#. 主机等待 100ms
	#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

.. note::

	若 device 直接与 RC 连接，其 link partner 是 RC；若 device 直接与 switch 连接，其 link partner 是 switch。

case_pcie_rdlh_interrupt
""""""""""""""""""""""""

验证 PCIe link 到最高速率 L0 时会上报中断给 FW。

.. doxygenfunction:: case_pcie_rdlh_interrupt
	:project: unittest

| 操作步骤如下：

1. 主机解析 device link partner 支持的最高速率
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`, 参数如下：
	- param: host 支持的最高速率，Gen1 对应的值为 1，Gen2 对应的值为 2，以此类推
#. 主机等待 100ms
#. 主机发起 hot reset，配置 Secondary Bus Reset 字段的值为 1，等待 10ms 后将此字段清 0
	- PCI Configuration Space Header → Bridge Control Register bit[6]
#. 主机等待 100ms 后重新初始化 device
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_pcie_speed_down_interrupt
""""""""""""""""""""""""""""""

验证 PCIe 降速时会上报中断给 FW。

.. doxygenfunction:: case_pcie_speed_down_interrupt
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(3)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机发起降速，配置 device 的 Target Link Speed 字段的值为 1
	- PCI Experess Capability Structure → EP Link Control 2 Register bit[3:0]
#. 主机重新link，配置 device link partner 的 Retrain Link 字段的值为 1
	- PCI Experess Capability Structure → Link Control Register bit[5]
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_interrupt` 命令，设置 subcmd 为 BIT(3)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机设置链路速率为 Gen5，配置 Target Link Speed 字段的值为 5
#. 主机重新link，配置 device link partner 的 Retrain Link 字段的值为 1


.. [#1] 由于 copy 命令需要为 desc 分配额外的资源，它在使用结束后需要手动释放资源。故目前不支持一次性提交多个随机的 copy 命令。
