===
API
===

Overview
========

Command
-------

.. csv-table:: Command API table
	:header: "Function", "Description", "Note"
	:widths: 30, 60, 10

	"ut_submit_io_read_cmd", "提交 I/O read 命令"
	"ut_submit_io_read_cmds", "提交若干条 I/O read 命令"
	"ut_submit_io_read_cmd_random_region", "提交 I/O read 命令，随机读取 LBA 区域"
	"ut_submit_io_read_cmds_random_region", "提交若干条 I/O read 命令，随机读取 LBA 区域"
	"ut_submit_io_write_cmd", "提交 I/O write 命令"
	"ut_submit_io_write_cmds", "提交若干条 I/O write 命令"
	"ut_submit_io_write_cmd_random_data", "提交 I/O write 命令，向指定的 LBA 区域写入随机的数据内容"
	"ut_submit_io_write_cmd_random_d_r", "提交 I/O write 命令，向随机的 LBA 区域写入随机数据内容"
	"ut_submit_io_write_cmds_random_d_r", "提交若干条 I/O write 命令，向随机的 LBA 区域写入随机数据内容"
	"ut_submit_io_compare_cmd", "提交 I/O compare 命令"
	"ut_submit_io_compare_cmds", "提交若干条 I/O compare 命令"
	"ut_submit_io_compare_cmd_random_region", "提交 I/O compare 命令，随机比较 LBA 区域"
	"ut_submit_io_compare_cmds_random_region", "提交若干条 I/O compare 命令，随机比较 LBA 区域"
	"ut_submit_io_verify_cmd", "提交 I/O verify 命令"
	"ut_submit_io_verify_cmds", "提交若干条 I/O verify 命令"
	"ut_submit_io_verify_cmd_random_region", "提交 I/O verify 命令，随机指定 LBA 区域"
	"ut_submit_io_copy_cmd", "提交 I/O copy 命令"
	"ut_submit_io_copy_cmds", "提交若干条 I/O copy 命令"
	"ut_submit_io_fused_cw_cmd", "提交 I/O compare + write 的 fused 组合命令"
	"ut_submit_io_fused_cw_cmds", "提交若干条 I/O compare + write 的 fused 组合命令"
	"ut_submit_io_fused_cw_cmd_random_region", "提交若干条 I/O compare + write 的 fused 组合命令，随机指定 LBA 区域"
	"ut_submit_io_zone_append_cmd", "提交 I/O zone append 命令"
	"ut_submit_io_random_cmd", "随机提交 I/O 命令，eg：read/write/compare/verify/copy..."
	"ut_submit_io_random_cmd_random_region", "随机提交 I/O 命令，随机指定 LBA 区域"
	"ut_submit_io_random_cmds_random_region", "随机提交若干条 I/O 命令，随机指定 LBA 区域"
	"..."

.. attention::
	1. submit: 仅提交命令，不会等待取回 CQ entry.
	#. send: 发送命令后会等待取回 CQ entry，即命令做完后才返回.

Queue
-----

.. csv-table:: Queue API table
	:header: "Function", "Description", "Note"
	:widths: 30, 60, 10

	"ut_create_pair_io_queue", "创建一对 I/O SQ & CQ 队列"
	"ut_create_pair_io_queues", "创建若干对 I/O SQ & CQ 队列"
	"ut_delete_pair_io_queue", "删除一对 I/O SQ & CQ 队列"
	"ut_delete_pair_io_queues", "删除若干对 I/O SQ & CQ 队列"
	"ut_ring_sq_doorbell", "更新 SQ tail 值"
	"ut_ring_sqs_doorbell", "更新若干条 SQ 队列 tail 值"
	"ut_reap_cq_entry_check_status", "取回 CQE 并检查 completion status 的值"
	"ut_reap_cq_entry_check_status_by_id", "取回 CQE 并检查 completion status 的值"
	"ut_reap_cq_entry_check_no_check", "取回 CQE，不检查 completion status 的值"
	"ut_reap_cq_entry_check_no_check_by_id", "取回 CQE，不检查 completion status 的值"

Function
========

Command
-------

.. doxygenfunction:: ut_alloc_copy_resource
	:project: unittest

.. doxygenfunction:: ut_release_copy_resource
	:project: unittest

.. doxygenfunction:: ut_submit_io_read_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_read_cmds
	:project: unittest

.. doxygenfunction:: ut_submit_io_read_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_submit_io_read_cmds_random_region
	:project: unittest

.. doxygenfunction:: ut_submit_io_write_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_write_cmds
	:project: unittest

.. doxygenfunction:: ut_submit_io_write_cmd_random_data
	:project: unittest

.. doxygenfunction:: ut_submit_io_write_cmd_random_d_r
	:project: unittest

.. doxygenfunction:: ut_submit_io_write_cmds_random_d_r
	:project: unittest

.. doxygenfunction:: ut_submit_io_compare_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_compare_cmds
	:project: unittest

.. doxygenfunction:: ut_submit_io_compare_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_submit_io_compare_cmds_random_region
	:project: unittest

.. doxygenfunction:: ut_submit_io_verify_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_verify_cmds
	:project: unittest

.. doxygenfunction:: ut_submit_io_verify_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_submit_io_copy_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_copy_cmds
	:project: unittest

.. doxygenfunction:: ut_submit_io_fused_cw_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_fused_cw_cmds
	:project: unittest

.. doxygenfunction:: ut_submit_io_fused_cw_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_submit_io_zone_append_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_random_cmd
	:project: unittest

.. doxygenfunction:: ut_submit_io_random_cmds
	:project: unittest

.. doxygenfunction:: ut_submit_io_random_cmds_to_sqs
	:project: unittest

.. doxygenfunction:: ut_send_io_read_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_read_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_send_io_read_cmds_random_region
	:project: unittest

.. doxygenfunction:: ut_send_io_write_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_write_cmd_random_data
	:project: unittest

.. doxygenfunction:: ut_send_io_write_cmd_random_d_r
	:project: unittest

.. doxygenfunction:: ut_send_io_write_cmds_random_d_r
	:project: unittest

.. doxygenfunction:: ut_send_io_compare_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_compare_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_send_io_verify_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_verify_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_send_io_copy_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_copy_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_send_io_fused_cw_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_fused_cw_cmd_random_region
	:project: unittest

.. doxygenfunction:: ut_send_io_zone_append_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_random_cmd
	:project: unittest

.. doxygenfunction:: ut_send_io_random_cmds
	:project: unittest

.. doxygenfunction:: ut_send_io_random_cmds_to_sqs
	:project: unittest

.. doxygenfunction:: ut_modify_cmd_prp
	:project: unittest

Queue
-----

.. doxygenfunction:: ut_create_pair_io_queue
	:project: unittest

.. doxygenfunction:: ut_create_pair_io_queues
	:project: unittest

.. doxygenfunction:: ut_delete_pair_io_queue
	:project: unittest

.. doxygenfunction:: ut_delete_pair_io_queues
	:project: unittest

.. doxygenfunction:: ut_ring_sq_doorbell
	:project: unittest

.. doxygenfunction:: ut_ring_sqs_doorbell
	:project: unittest

.. doxygenfunction:: ut_reap_cq_entry_check_status
	:project: unittest

.. doxygenfunction:: ut_reap_cq_entry_check_status_by_id
	:project: unittest

.. doxygenfunction:: ut_reap_cq_entry_no_check
	:project: unittest

.. doxygenfunction:: ut_reap_cq_entry_no_check_by_id
	:project: unittest
