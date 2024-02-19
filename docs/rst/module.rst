======
Module
======

dnvme
=====

Debug
-----

| 模块中使用 Trace Event 机制来跟踪和记录 Host 和 NVMe 设备之间交互的数据内容及相关操作。用户通过访问 debugfs 下相应的文件就可以控制需要记录的信息。

| 操作步骤如下：

1. 加载 dnvme.ko 模块后，在 `/sys/kernel/debug/tracing/events` 路径下就可以看到生成的 `nvme` 目录；
#. 配置 Event，模块支持的 Event 选项如 :ref:`Events List Table <label_dnvme_debug_trace_event>` 中所示
	a. 打开 Event，eg：打开 dnvme_submit_64_cmd，执行 :command:`echo 1 > /sys/kernel/debug/tracing/events/nvme/dnvme_submit_64b_cmd/enable` 或 :command:`echo nvme:dnvme_submit_64b_cmd > /sys/kernel/debug/tracing/set_event` 命令
	#. 关闭 Event，eg：关闭 dnvme_submit_64_cmd，执行 :command:`echo 0 > /sys/kernel/debug/tracing/events/nvme/dnvme_submit_64b_cmd/enable` 命令
#. 运行用户程序（在此期间，会自动记录 enable 的 Event 信息）
#. 查看记录的 Event 信息，执行 :command:`cat /sys/kernel/debug/tracing/trace` 命令

.. _label_dnvme_debug_trace_event:

.. csv-table:: Events List Table
	:header: "File", "Description"

	"dnvme_interrupt", "记录触发的中断类型、中断号以及时间戳（以调度中断处理函数的时间为准）"
	"dnvme_proc_write", "记录向 proc 节点写入的数据内容"
	"dnvme_setup_prps", "记录 Command 关联的 PRP List"
	"dnvme_sgl_set_bit_bucket", "记录 Command 关联的 Bit Bucket descriptor"
	"dnvme_sgl_set_data", "记录 Command 关联的 Data Block descriptor"
	"dnvme_sgl_set_seg", "记录 Command 关联的 SGL Segment descriptor 或 Last SGL Segment descriptor"
	"dnvme_submit_64b_cmd", "记录用户提交到 SQ 中的 Command"
	"handle_cmd_completion", "记录从 CQ 中取出的 CQ Entry"

.. note:: 
	1. 加载模块后，所有 event 默认处于 disable 状态；
	2. 若需要使能所有的 event，可以执行以下命令： :command:`echo 1 > /sys/kernel/debug/tracing/events/nvme/enable`

