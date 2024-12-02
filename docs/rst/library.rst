=======
Library
=======

.. toctree::
	:caption: Contents
	:hidden:
	:maxdepth: 3

	lib/base
	lib/crc
	lib/nvme


Standard Library
================

| 部分常用的标准库函数如下表所示。

.. csv-table:: Standard API table
	:header: "Function", "Description", "Note"
	:widths: 30, 50, 20

	"malloc", "申请一块内存空间，不会对其初始化", "若需要初始化的话，建议使用 zalloc"
	"calloc", "申请若干块大小相同的内存空间，并将其初始化为 0"
	"posix_memalign", "申请一块内存空间，其地址会按指定大小对齐"
	"sleep", "休眠若干秒"
	"udelay", "延迟若干微秒", "若需要使用毫秒级别的话，可以使用 msleep"

.. note::
	1. 函数原型及详细信息可查询 man 手册。

