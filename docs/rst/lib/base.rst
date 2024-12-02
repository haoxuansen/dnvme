=======
libbase
=======

API
===

Overview
--------

.. csv-table:: Base API table
	:header: "Function", "Description", "Note"
	:widths: 30, 60, 10

	"call_system", "功能和 system 函数相同，增加了错误检查"
	"fill_data_with_incseq", "初始化 buf，从 0 开始以递增(+1)的方式填充数据"
	"fill_data_with_decseq", "初始化 buf，从 0xff 开始以递减(-1)的方式填充数据"
	"fill_data_with_random", "初始化 buf，填充随机数"
	"dump_data_to_console", "打印 buf 中的数据内容"
	"dump_data_to_file", "将 buf 数据保存到指定的文件中"
	"dump_data_to_fmt_file", "将 buf 数据保存到指定的文件中"
	"dump_stack", "打印函数的调用关系，for debug"

Log
^^^

| 这里提供了一组用于打印不同等级日志的接口：pr_emerg/pr_alert/pr_crit/pr_err/pr_warn/pr_notice/pr_info/pr_debug...

| 另外提供了一组用于打印不同颜色日志的接口: pr_red/pr_green/pr_yellow/pr_blue/pr_cyan/pr_white...

