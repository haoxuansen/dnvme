=========
Unit Test
=========

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

case_host_enable_ltr_message
""""""""""""""""""""""""""""

验证主机在使能 device LTR 后，device 是否有按配置值发送 LTR Message

.. doxygenfunction:: case_host_enable_ltr_message
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 15ms => 配置 R[Max Snoop Latency] = 0x100F, R[Max No-Snoop Latency] = 0x100F
#. 主机关闭 device L1 => 配置 R[Link Control].F[ASPM Control].B[1] = 0
#. 主机关闭 device LTR => 配置 R[Device Control 2].F[LTR Mechanism Enable] = 0
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机打开 device LTR => 配置 R[Device Control 2].F[LTR Mechanism Enable] = 1
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

case_l1tol0_ltr_message
"""""""""""""""""""""""

验证 device 从 L1 退出时是否有按配置值发送 LTR Message

.. doxygenfunction:: case_l1tol0_ltr_message
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 15ms => 配置 R[Max Snoop Latency] = 0x100F, R[Max No-Snoop Latency] = 0x100F
#. 主机打开 device L1 => 配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机打开 device link parter 的 L1 => 配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机关闭 device L1 => 配置 R[Link Control].F[ASPM Control].B[1] = 0
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

.. note:: 
	1. Host 需要支持 ASPM L1

case_l0tol1_ltr_message
"""""""""""""""""""""""

验证 device 从 L0 进入 L1 前是否有按配置值发送 LTR Message

.. doxygenfunction:: case_l0tol1_ltr_message
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 15ms => 配置 R[Max Snoop Latency] = 0x100F, R[Max No-Snoop Latency] = 0x100F
#. 主机关闭 device 的 L1CPM/L1.1/L1.2
	a. 配置 R[Link Control].F[Enable Clock Power Management] = 0 [#4]_
	#. 配置 R[L1 PM Substates Control 1].F[ASPM L1.2 Enable] = 0 [#5]_
	#. 配置 R[L1 PM Substates Control 1].F[ASPM L1.1 Enable] = 0
#. 主机关闭 device link parter 的 L1 CPM、L1.1、L1.2
#. 主机打开 device 的 L1，配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机打开 device link parter 的 L1，配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机读取 R[Device Status] 值
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

.. note:: 
	1. Host 需要支持 ASPM L1

case_l0tol1cpm_ltr_message
""""""""""""""""""""""""""

验证 device 从 L0 进入 L1CPM 前是否有按配置值发送 LTR Message

.. doxygenfunction:: case_l0tol1cpm_ltr_message
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 15ms => 配置 R[Max Snoop Latency] = 0x100F, R[Max No-Snoop Latency] = 0x100F
#. 主机关闭 device 的 L1.1、L1.2，打开 device 的 L1、L1CPM
 	a. 配置 R[L1 PM Substates Control 1].F[ASPM L1.2 Enable] = 0 [#5]_
	#. 配置 R[L1 PM Substates Control 1].F[ASPM L1.1 Enable] = 0
	#. 配置 R[Link Control].F[Enable Clock Power Management] = 1 [#4]_
	#. 配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(3)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机读取 R[Device Status] 值
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(3)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

.. note:: 
	1. Host 需要支持 ASPM L1 & L1CPM

case_l0tol11_ltr_message
""""""""""""""""""""""""

验证 device 从 L0 进入 L1.1 前是否有按配置值发送 LTR Message

.. doxygenfunction:: case_l0tol11_ltr_message
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 15ms => 配置 R[Max Snoop Latency] = 0x100F, R[Max No-Snoop Latency] = 0x100F
#. 主机关闭 device 的 L1.2，打开 device 的 L1、L1CPM、L1.1
 	a. 配置 R[L1 PM Substates Control 1].F[ASPM L1.2 Enable] = 0 [#5]_
	#. 配置 R[L1 PM Substates Control 1].F[ASPM L1.1 Enable] = 1
	#. 配置 R[Link Control].F[Enable Clock Power Management] = 1 [#4]_
	#. 配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(4)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机读取 R[Device Status] 值
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(4)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

.. note:: 
	1. Host 需要支持 ASPM L1 & L1.1

case_l0tol12_ltr_message
""""""""""""""""""""""""

验证 device 从 L0 进入 L1.2 前是否有按配置值发送 LTR Message

.. doxygenfunction:: case_l0tol12_ltr_message
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 15ms => 配置 R[Max Snoop Latency] = 0x100F, R[Max No-Snoop Latency] = 0x100F
#. 主机打开 device 的 L1、L1CPM、L1.1、L1.2
 	a. 配置 R[L1 PM Substates Control 1].F[ASPM L1.2 Enable] = 1 [#5]_
	#. 配置 R[L1 PM Substates Control 1].F[ASPM L1.1 Enable] = 1
	#. 配置 R[Link Control].F[Enable Clock Power Management] = 1 [#4]_
	#. 配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(5)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机读取 R[Device Status] 值
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(5)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

.. note:: 
	1. Host 需要支持 ASPM L1 & L1.2

case_host_enable_ltr_over_max_mode
""""""""""""""""""""""""""""""""""

主机使能 device LTR 时，若配置值大于 max LTR latency，验证 device 是否有发送 latency 为 0 的 LTR Message

.. doxygenfunction:: case_host_enable_ltr_over_max_mode
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 1ms => 配置 R[Max Snoop Latency] = 0x1001, R[Max No-Snoop Latency] = 0x1001
#. 主机关闭 device 的 L1 => 配置 R[Link Control].F[ASPM Control].B[1] = 0
#. 主机关闭 device 的 LTR => 配置 R[Device Control 2].F[LTR Mechanism Enable] = 0
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(6)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机打开 device 的 LTR => 配置 R[Device Control 2].F[LTR Mechanism Enable] = 1
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(6)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

case_l1_to_l0_ltr_over_max_mode
"""""""""""""""""""""""""""""""

若配置值大于 max LTR latency，验证 device 在退出 L1 后是否有发送 latency 为 0 的 LTR Message

.. doxygenfunction:: case_l1_to_l0_ltr_over_max_mode
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 1ms => 配置 R[Max Snoop Latency] = 0x1001, R[Max No-Snoop Latency] = 0x1001
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(7)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机读取 R[Device Status] 值
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(7)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

.. note:: 
	1. Host 需要支持 ASPM L1

case_l0_to_l1_ltr_over_max_mode
"""""""""""""""""""""""""""""""

若配置值大于 max LTR latency，验证 device 在进入 L1.2 前是否有发送 latency 为 0 的 LTR Message

.. doxygenfunction:: case_l0_to_l1_ltr_over_max_mode
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_
#. 主机设置 device Max Snoop/No-Snoop Latency 为 1ms => 配置 R[Max Snoop Latency] = 0x1001, R[Max No-Snoop Latency] = 0x1001
#. 主机打开 device 的 L1、L1CPM、L1.1、L1.2
 	a. 配置 R[L1 PM Substates Control 1].F[ASPM L1.2 Enable] = 1 [#5]_
	#. 配置 R[L1 PM Substates Control 1].F[ASPM L1.1 Enable] = 1
	#. 配置 R[Link Control].F[Enable Clock Power Management] = 1 [#4]_
	#. 配置 R[Link Control].F[ASPM Control].B[1] = 1
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(8)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机读取 R[Device Status] 值
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(8)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值：R[Max Snoop Latency]、R[Max No-Snoop Latency] [#4]_

.. note:: 
	1. Host 需要支持 ASPM L1

case_less_ltr_threshold_mode
""""""""""""""""""""""""""""

若配置值小于 LTR threshold latency，验证 device 在进入 L1.2 前是否有发送 latency 为 LTR threshold latency 的 LTR Message

.. doxygenfunction:: case_less_ltr_threshold_mode
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 以下寄存器值
 	a. R[Max Snoop Latency] [#4]_
	#. R[Max No-Snoop Latency]
	#. R[L1 PM Substates Control 1].F[LTR_L1.2_THRESHOLD_Value] [#5]_
	#. R[L1 PM Substates Control 1].F[LTR_L1.2_THRESHOLD_Scale]
#. 主机设置 device Max Snoop/No-Snoop Latency 为 15ms => 配置 R[Max Snoop Latency] = 0x100F, R[Max No-Snoop Latency] = 0x100F
#. 主机设置 device LTR_L1.2_THRESHOLD 为 3ms
	a. 配置 R[L1 PM Substates Control 1].F[LTR_L1.2_THRESHOLD_Value] = 0x3
	#. 配置 R[L1 PM Substates Control 1].F[LTR_L1.2_THRESHOLD_Scale] = 0x4
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(9)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机读取 deivce 寄存器值 => R[Device Status]
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(9)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 寄存器值
 	a. R[Max Snoop Latency] [#4]_
	#. R[Max No-Snoop Latency]
	#. R[L1 PM Substates Control 1].F[LTR_L1.2_THRESHOLD_Value] [#5]_
	#. R[L1 PM Substates Control 1].F[LTR_L1.2_THRESHOLD_Scale]

.. note:: 
	1. Host 需要支持 ASPM L1 & L1.2

case_drs_message
""""""""""""""""

验证 device 在 linkup 后是否立即发送 DRS Message。

.. doxygenfunction:: case_drs_message
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(10)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机发起 linkdown (hot reset)
	a. 配置 device link partner R[Bridge Control].F[Secondary Bus Reset] = 1
	#. 等待 10ms
	#. 配置 device link partner R[Bridge Control].F[Secondary Bus Reset] = 0
#. 主机等待 100ms
#. 主机重新初始化 device
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(10)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_frs_message
""""""""""""""""

验证 device 从 :math:`D0_{uninitiated}` 状态回到 :math:`D0_{active}` 后是否立即发送 FRS Message。

.. doxygenfunction:: case_frs_message
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(11)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机发起 FLR => 配置 device R[Device Control].F[Initiate Function Level Reset] = 1
#. 主机等待 100ms
#. 主机重新初始化 device
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_msg` 命令，设置 subcmd 为 BIT(11)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

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
#. 主机发起 hot reset，配置 device link partner 的 Secondary Bus Reset 字段的值为 1，等待 10ms 后将此字段清 0
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

case_aspm_l1sub_disable_by_fw
"""""""""""""""""""""""""""""

FW 控制是否进入 ASPM L1sub。

.. doxygenfunction:: case_aspm_l1sub_disable_by_fw
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机使能电源管理时钟，配置 device 的 Enable Clock Power Management 字段的值为 1
	- PCI Experess Capability Structure → Link Control Register bit[8]
#. 主机使能 ASPM L1.1，配置 device 的 ASPM L1.1 Enable 字段的值为 1
	- L1 PM Substates Extended Capability → L1 PM Substates Control 1 Register bit[3]
#. 主机使能 ASPM L1.2，配置 device 的 ASPM L1.2 Enable 字段的值为 1
	- L1 PM Substates Extended Capability → L1 PM Substates Control 1 Register bit[2]
#. 主机使能 ASPM L1，配置 device 的 ASPM Control 字段的值为 2
	- PCI Experess Capability Structure → Link Control Register bit[1:0]
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(0)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_data_rate_register_in_l12
""""""""""""""""""""""""""""""

检查在 L1.2 状态下可以读取的真实速率。

.. doxygenfunction:: case_data_rate_register_in_l12
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机使能 ASPM L1.2，配置 device 的 ASPM L1.2 Enable 字段的值为 1
	- L1 PM Substates Extended Capability → L1 PM Substates Control 1 Register bit[2]
#. 主机使能 ASPM L1，配置 device 的 ASPM Control 字段的值为 2
	- PCI Experess Capability Structure → Link Control Register bit[1:0]
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(1)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_hw_control_request_retry
"""""""""""""""""""""""""""""

.. doxygenfunction:: case_hw_control_request_retry
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机发起 FLR => 配置 device R[Device Control].F[Initiate Function Level Reset] = 1
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_d3_not_block_message
"""""""""""""""""""""""""

验证 device 在 D3 状态下可正常收发 Message。

.. doxygenfunction:: case_d3_not_block_message
	:project: unittest

| 操作步骤如下：

1. 主机将 device 设置成 D3 状态 => 配置 device R[Power Management Control/Status].F[PowerState] = 0x3 [#6]_
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(4)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(4)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`

case_internal_cpld_mps_check
""""""""""""""""""""""""""""

.. doxygenfunction:: case_internal_cpld_mps_check
	:project: unittest

| 操作步骤如下：

1. 主机备份 device 的 MPS 和 MRRS 字段
	- PCI Experess Capability Structure → Device Control Register
		- bit[7:5]: Max_Payload_Size, 000b - 128 bytes, 001b - 256 bytes, 010b - 512 bytes
		- bit[14:12]: Max_Read_Request_Size, 001b - 256 bytes, 010b - 512 bytes, 011b - 1024 bytes
#. 主机备份 device link partner 的 MPS 和 MRRS 字段
#. 主机将 device 的 MPS 随机配置为 128/256 bytes，MRRS 配置为 1024 bytes
#. 主机将 device link partner 的 MPS 配置为 256/512 bytes(需要大于 device MPS 配置)
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(5)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
#. 主机发送 1 条 I/O Write command，写入的 size 为 4KB
#. 主机等待 100ms
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(5)，option 为 :enumerator:`NVME_MAXIO_OPT_CHECK_RESULT`
#. 主机还原 device 的 MPS 和 MRRS 字段配置
#. 主机还原 device link partner 的 MPS 字段配置

case_bdf_check
""""""""""""""

.. doxygenfunction:: case_bdf_check
	:project: unittest

| 操作步骤如下：

1. 主机解析 device 的 BDF 信息
#. 主机发送 :enumerator:`nvme_admin_maxio_pcie_special` 命令，设置 subcmd 为 BIT(6)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`，参数如下：
	- param: bit[15:8] Bus Number, bit[7:3] Device Number, bit[2:0] Function Number 

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

case_fwdma_ut_hmb_engine_test
"""""""""""""""""""""""""""""

.. doxygenfunction:: case_fwdma_ut_hmb_engine_test
	:project: unittest

| 操作步骤如下：

1. 主机发送 Set Feature - Host Memory Buffer 命令，配置并使能 HMB
	a. 解析 controller 支持的 descriptor entry 数量上限 [#2]_ ，并取随机值配置。
	#. 解析 desc entry 指向的单个 buffer 的大小 [#3]_ 。
	#. 若所有 buffer 的总大小 ≥ 2MiB，则不作调整。否则，调整最后一个 desc entry 对应的 buffer size，使 buffer 的总大小为 2MiB。
#. 主机发送 :enumerator:`nvme_admin_maxio_fwdma_fwdma` 命令，设置 subcmd 为 BIT(7)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
	a. cdw11 值为 1
	#. param 值随机，各字段信息如下表所示
	
	.. csv-table:: Param Format
		:header: "Bit(s)", "Name", "Description"

		"0", "Encrption", "0: disable; 1: enable"
		"1", "Verify", "0: disable; 1: enable"
		"31:3", "Reserved"

#. 主机继续发送若干条 :enumerator:`nvme_admin_maxio_fwdma_fwdma` 命令，设置 subcmd 为 BIT(7)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`，参数如 :ref:`label_case_fwdma_ut_hmb_engine_test` 中所示
#. 主机发送 Set Feature - Host Memory Buffer 命令，disable HMB

.. _label_case_fwdma_ut_hmb_engine_test:

.. csv-table:: Vendor Command List
	:header: "cdw11", "param", "cdw12", "note"

	"2"
	"3", "[1, 64]"
	"4"
	"5", "[1, 64]", "uint32_t 随机数", "Search single list mode"
	"6", "[0, 1024]", "", "Search single normal mode"
	"7", "", "", "Delete TLAA mode"
	"8", "", "", "Delete TTLAA mode"
	"9", "", "", "Reset mode"

.. important::
	1. cdw11: Vendor command sequence number，即第几条 vendor command

.. note:: 
	1. [n, m]: 表示取 n 到 m 之间的随机（整）数，包含 n 和 m。

case_fwdma_ut_fwdma_mix_case_check
""""""""""""""""""""""""""""""""""

.. doxygenfunction:: case_fwdma_ut_fwdma_mix_case_check
	:project: unittest

| 操作步骤如下：

1. 主机发送 :enumerator:`nvme_admin_maxio_fwdma_fwdma` 命令，设置 subcmd 为 BIT(8)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
	a. cdw11 值为 1
	#. param 值随机，各字段信息如下表所示

	.. csv-table:: Param Format
		:header: "Bit(s)", "Name", "Description", "Note"

		"0", "Method", "0: AES; 1: SM4", "选择 SM4 时，Key 的长度必须为 128bits"
		"1", "Key Length", "0: 128bits; 1: 256bits"
		"3:2", "Mode", "0: ECB; 1: XTS; 2: CBC; 3: CFB128", "选择 XTS 时，数据长度至少为 512Byte"
		"4", "Verify", "0: disable; 1: enable"
		"5", "Key Type", "0: ATA; 1: Range 0"
		"31:6", "Reserved"

2. 主机继续发送若干条 :enumerator:`nvme_admin_maxio_fwdma_fwdma` 命令，设置 subcmd 为 BIT(8)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`，参数如 :ref:`label_case_fwdma_ut_fwdma_mix_case_check` 中所示

.. _label_case_fwdma_ut_fwdma_mix_case_check:

.. csv-table:: Vendor Command List
	:header: "cdw11", "param", "cdw6", "cdw7", "cdw13", "cdw14", "cdw15", "note"

	"2", "", "", "", "SLBAL", "SLBAH", "data length"
	"3", "", "PRP1", "PRP2", "SLBAL", "SLBAH", "data length" 
	"4", "0 或 1", "PRP1", "PRP2", "SLBAL", "SLBAH", "data length", "param=0 或 1 时，需要将 PRP buffer 所有 bit 清 0 或置 1"
	"5", "", "PRP1", "PRP2", "SLBAL", "SLBAH"
	"6", "", "PRP1", "PRP2", "SLBAL", "SLBAH"

.. important::
	1. cdw11: Vendor command sequence number，即第几条 vendor command
	#. cdw13: SLBA bit[31:0]
	#. cdw14: SLBA bit[63:32]
	#. cdw15: data length(unit: Byte)，必须按 16Byte 对齐，其最大值应小于等于 32KiB

.. note:: 
	1. 第 1 次会随机生成符合要求的 data length，后面的 command 均使用相同的 data length;
	#. 所有 command 中使用的 PRP1 和 PRP2 为主机同一个 buffer, 固定长度为 32KiB

case_dpu_mix
""""""""""""

.. doxygenfunction:: case_dpu_mix
	:project: unittest

| 操作步骤如下：

1. 重复执行以下操作 n 次
	1. 主机解析 device 支持的 namespace 并随机选取任意个 namespace 进行测试
	#. 主机发送 :enumerator:`nvme_admin_maxio_fwdma_dpu` 命令，设置 subcmd 为 BIT(2)，option 为 :enumerator:`NVME_MAXIO_OPT_SET_PARAM`
		a. 初始化配置参数如表 :ref:`label_case_dpu_mix_1` 中所示
		b. cmd param 字段记录配置参数的长度，单位：Byte
	#. 主机创建已选取 namespace 若干倍的 I/O queue (eg: 选取 4 个 namespace，创建 4/8/12... 个 I/O queue)，每个 namespace 关联相同数量的 I/O queue；
	#. 主机遍历选取的 namespace 及其关联的 I/O queue，每条 I/O queue 随机提交 512 条命令（每提交一条后就敲 doorbell 处理）：
		a. Write + Read 命令：写入数据随机，读写区域相同，读写结束后主机对比读写的数据内容是否完全一致; 随机添加 FUA Flag
		#. Copy 命令：先 write 随机数，再 copy，最后 read 取回后比较数据内容
		#. Fused 命令(Compare + Write)
		#. Compare 命令
	#. 主机遍历选取的 namespace 及其关联的 I/O queue，每条 I/O queue 随机提交 512 条命令（全部提交后再敲 doorbell 处理）：
		a. Write Command: 随机添加 FUA Flag
		#. Read Command
		#. Copy Command
		#. Fused Command: Compare + Write
		#. Compare Command

.. note:: 
	1. device 当前支持的 namespace 最大数量为 16，支持的 I/O queue 最大数量为 64
	2. 若指定提交的 command 数量大于 SQ entry 数量，则以 SQ entry 数量为准

.. _label_case_dpu_mix_1:

.. table:: DPU MIX Configuration Data Structure
	:align: center

	+---------+-------------------------------+-------------------------------+-------------------------------+-------------------------------+
	| Offset  |             Byte3             |             Byte2             |             Byte1             |             Byte0             |
	|  (DW)   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	|         | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	+=========+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+
	| 0       | Reserved                                                                                                                      |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+
	| 1       | Reserved                                                                                                                      |
	+---------+---------------------------------------------------------------------------------------------------------------------------+---+
	| 2       | Reserved                                                                                                                  |MC |
	+---------+---------------------------------------------------------------------------------------------------------------------------+---+
	| 3       | Namespace Configuration Entry 0                                                                                               |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+
	| 4       | Namespace Configuration Entry 1                                                                                               |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+
	| ...     | ...                                                                                                                           |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+
	| 66      | Namespace Configuration Entry 63                                                                                              |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+
	| 67      | SQM Configuration                                                                                                             |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+
	| 131:68  | HWDMA Configuration for WDMA                                                                                                  |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+
	| 195:132 | HWDMA Configuration for RDMA                                                                                                  |
	+---------+-------------------------------------------------------------------------------------------------------------------------------+

.. _label_case_dpu_mix_2:

.. csv-table:: DPU MIX Configuration Data Structure Field Details
	:header: "Field", "Description"
	:widths: 30, 70

	"MC", "Multi cmd, 0b: Disable, 1b: Enable"
	"Namespace Configuration Entry", "仅 namespace enable 时有效，参见 :ref:`label_case_dpu_mix_3`"
	"SQM Configuration", "参见 :ref:`label_case_dpu_mix_5`"
	"HWDMA Configuration for WDMA", "参见 :ref:`label_case_dpu_mix_6`"
	"HWDMA Configuration for RDMA", "参见 :ref:`label_case_dpu_mix_7`"

.. _label_case_dpu_mix_3:

.. table:: Namespace Configuration Entry

	+--------+-------------------------------+-------------------------------+-------------------------------+-------------------------------+
	| Offset |             Byte3             |             Byte2             |             Byte1             |             Byte0             |
	|        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	|        | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	+========+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+===+
	| DW0    |En | Reserved  | Key Comb      |OPL|DPU| Key Fmt   | Key Type  | NSID                                                          |
	+--------+---+-----------+---------------+---+---+-----------+-----------+---------------------------------------------------------------+

.. _label_case_dpu_mix_4:

.. csv-table:: Namespace Configuration Entry Field Details
	:header: "Bit", "Field", "Description"

	"15:0", "NSID", "Namespace Identifier"
	"18:16", "Key Type", "1: Select ATA Key, 2: Select Opal Key"
	"21:19", "Key Fmt", "Key Entry Format, 1: 256bits, 2: 512bits"
	"22", "DPU", "0b: Disable MPI, 1b: Enable MPI"
	"23", "OPL", "Opal, 0b: Disable, 1b: Enable"
	"27:24", "Key Comb", "若 Key Entry Format 为 256bits，则 1: AES/ECB/128, 2: AES/XTS/128, 3: SM4/ECB/128, 4: SM4/XTS/128"
	"", "", "若 Key Entry Format 为 512bits，则 1: AES/ECB/128, 2: AES/ECB/256, 3: AES/XTS/256, 4: SM4/ECB/128"
	"30:28", "Reserved"
	"31", "En", "Namespace Enable, 0: Disable, 1: Enable"

.. attention:: 
	1. 所有 namespace 配置的 Key Fmt 值必须相同，暂不支持独立配置

.. _label_case_dpu_mix_5:

.. csv-table:: SQM Configuration
	:header: "Bit", "Description"

	"0", "Write cmd 是否提前回 CQ, 0: Disable, 1: Enable"
	"1", "Write cmd PCQ 是否打开，0: Disable, 1: Enable"
	"2", "Fused cmd 支持是否打开，0: Disable, 1: Enable"
	"3", "Atomic/Consist 功能是否打开，0: Disable, 1: Enable"
	"4", "FUA PCQ 是否打开，0: Disable, 1: Enable"

.. _label_case_dpu_mix_6:

.. csv-table:: HWDMA Configuration for WDMA
	:header: "DW", "Bit", "Field", "Description"

	"0", "3:0 [#9]_", "multi cmd switch condition", "0: just cut FTL Link for multi CMD"
	"", "", "", "1: cut FTL Link List for multi CMD when there is also no new CMD after current FTL"
	"", "", "", "2: cut FTL Link List for multi CMD only when current FTL is the last FTL of a CMD"
	"0", "5:4", "FTL Interface Enable", "0: 打开 intfc0, 1: 打开 intfc1，2: 打开 intfc0 和 intfc1"
	"0", "7:6", "Special Mode", "Enable Multi CMD 时选项如下： 0 - default mode, 1 - LAA bit mode"
	"", "", "", "Disable Multi CMD 时选项如下：0 - default mode"
	"0", "15:8", "Reserved"
	"0", "31:16", "cmd pop burst cnt", "随机值 [0, 10]，预留 [0, 511]"
	"1", "3:0", "intfc0 laa_mask [#7]_ ", "随机值 [0, 2]"
	"1", "7:4", "Reserved"
	"1", "15:8", "intfc0 laa_shift [#7]_ ", "随机值 [0, 3]，预留 [0, 127]"
	"1", "31:16", "Reserved"
	"2", "3:0", "intfc1 laa_mask [#8]_ ", "随机值 [0, 2]"
	"2", "7:4", "Reserved"
	"2", "15:8", "intfc1 laa_shift [#8]_ ", "随机值 [0, 3]，预留 [0, 127]"
	"2", "31:16", "Reserved"
	"3", "31:0", "Reserved"
	"19:4", "", "16 DWs 随机值"
	"20", "11:0", "ftl_wreq_max_pending_au_num", "随机值 [64, 128]" 
	"20", "21:12", "ftl_wreq_max_size", "随机值 [1, 64]"
	"20", "24:22", "ftl_wreq_ff_depth", "随机值 [0, 7]"
	"20", "25", "ftl_wreq_fuse_cmd_cut_en", "支持 Fused CMD 时选项如下：0 - Disable, 1 - Enable"
	"", "", "", "不支持 Fused CMD 时选项如下：0 - Disable"
	"20", "31:26", "Reserved"
	"21 [#9]_ ", "0", "Reserved"
	"21", "10:1", "ftl_wreq_max_cmd", "随机值 [1, @ftl_wreq_max_size]"
	"21", "18:11", "ftl_wreq_cmd_limit", "随机值 [0, 5]"
	"21", "28:19", "ftl_wreq_min_size", "随机值 [1, @ftl_wreq_max_size]"
	"21", "31:29", "ftl_wreq_ff_thr", "随机值 [0, 7]"
	"63:22", "", "Reserved"

.. _label_case_dpu_mix_7:

.. csv-table:: HWDMA Configuration for RDMA
	:header: "DW", "Bit", "Field", "Description"

	"0", "3:0", "Reserved"
	"0", "5:4", "FTL Interface Enable", "0: 打开 intfc0, 1: 打开 intfc1，2: 打开 intfc0 和 intfc1"
	"0", "7:6", "Special Mode", "Enable Multi CMD 时选项如下： 0 - default mode, 1 - LAA bit mode"
	"", "", "", "Disable Multi CMD 时选项如下：0 - default mode"
	"0", "15:8", "Reserved"
	"0", "31:16", "cmd pop burst cnt", "随机值 [0, 10]，预留 [0, 511]"
	"1", "3:0", "intfc0 laa_mask [#7]_ ", "随机值 [0, 2]"
	"1", "7:4", "Reserved"
	"1", "15:8", "intfc0 laa_shift [#7]_ ", "随机值 [0, 3]，预留 [0, 127]"
	"1", "31:16", "Reserved"
	"2", "3:0", "intfc1 laa_mask [#8]_ ", "随机值 [0, 2]"
	"2", "7:4", "Reserved"
	"2", "15:8", "intfc1 laa_shift [#8]_ ", "随机值 [0, 3]，预留 [0, 127]"
	"2", "31:16", "Reserved"
	"3", "31:0", "Reserved"
	"19:4", "", "16 DWs 随机值"
	"20", "11:0", "ftl_rreq_max_pending_au_num", "随机值 [@ftl_rreq_max_size, 256]"
	"20", "21:12", "ftl_rreq_max_size", "随机值 [1, 256]"
	"20", "24:22", "ftl_rreq_ff_depth", "随机值 [0, 7]"
	"20", "31:25", "Reserved"
	"21 [#9]_ ", "0", "Reserved"
	"21", "10:1", "ftl_rreq_max_cmd", "随机值 [1, @ftl_rreq_max_size]"
	"21", "18:11", "ftl_rreq_cmd_limit", "随机值 [0, 5]"
	"21", "28:19", "ftl_rreq_min_size", "随机值 [1, @ftl_rreq_max_size]"
	"21", "31:29", "ftl_rreq_ff_thr", "随机值 [0, 7]"
	"63:22", "", "Reserved"




.. [#1] 由于 copy 命令需要为 desc 分配额外的资源，它在使用结束后需要手动释放资源。故目前不支持一次性提交多个随机的 copy 命令。
.. [#2] 通过解析 Identify Controller Data Strucutre 中 HMMAXD 字段可以得知 controller 支持的 descriptor entry 数量上限。若 controller 未设置数量上限，则自定义设置为 8。
.. [#3] 通过解析 Idenfity Controller Data Structure 中 HMMINDS 字段可以得知 descriptor entry 至少应指向多大的 buffer 空间。若 controller 未设置下限，则自定义设置为 PAGE_SIZE(CC.MPS)。
.. [#4] Latency Tolerance Reporting (LTR) Extended Capability
.. [#5] L1 PM Substates Extended Capability
.. [#6] Power Management Capability
.. [#7] 当 FTL Interface Enable 打开 intfc0 且 Special Mode 选择 LAA bit mode 时需要配置此字段。
.. [#8] 当 FTL Interface Enable 打开 intfc1 且 Special Mode 选择 LAA bit mode 时需要配置此字段。
.. [#9] 仅 Enable Multi CMD 时该字段有效，其它情况下 Reserved

