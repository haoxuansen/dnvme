

.. toctree::
	:caption: Contents
	:hidden:
	:maxdepth: 2

	quick_start
	configuration
	example

========
Overview
========

| NVMe Tool 支持用户直接操作 NVMe 设备，如：Create/Delete Queue、Send Admin/IO/Vendor Command……并提供了一系列 unit test 测试用例，用于覆盖 NVMe Spec 中涉及的各项功能 Feature。NVMe Tool 目前或计划覆盖的内容如下：

1. NVM Express Base Specification
#. NVM Express NVM Command Set Specification
#. NVM Express Zoned Namesapce Command Set Specification

Architecture
============

| NVMe Tool 的架构层次如下图所示，大致可以分为三层：用户空间、内核空间及硬件。

+ 用户空间：主要包括应用程序和 lib 库两部分。为了简化应用程序，lib 库封装了常用的 API 接口。
+ 内核空间：主要会涉及 NVMe 驱动和 PCIe 驱动
	- NVMe驱动：Linux 原生的驱动较复杂、且开放给用户内容较少，实际开发使用起来不太方便。因此会使用自定义的驱动模块对 Linux 原生驱动模块进行替换。
	- PCIe驱动：直接使用 Linux 原生的驱动即可。
+ 硬件：控制对象主要是基于 PCIe 传输的 NVMe 设备

.. figure:: ./image/arch.png
	:align: center
	:width: 75%

	NVMe Tool Architecture

Reference
=========

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

