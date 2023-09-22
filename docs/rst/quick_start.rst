===========
Quick Start
===========

Directory Tree
==============

| NVMe Tool 工程的目录结构以及重要文件（夹）的功能说明如下：

.. code-block:: text

	.
	├── .config		: 默认全局配置文件
	├── .readthedocs.yaml	: 部署 ReadtheDocs 的配置文件
	├── app
	│   ├── sample		: 单个示例文件
	│   └── unittest	: 测试用例集合
	├── docs		: 文档及其它附件
	├── include		: 头文件
	├── Kconfig		: 记录配置选项的依赖规则，用于生成全局配置文件
	├── lib			: 库文件
	│   ├── base		: libbase 库文件
	│   ├── crc		: libcrc 库文件
	│   ├── include		: 所有库开放给应用程序的头文件
	│   └── nvme		: libnvme 库文件
	├── LICENSE.txt
	├── Makefile		: 顶层的编译脚本
	├── modules		: 内核模块
	│   └─ dnvme		: NVMe 驱动模块
	├── README.md		: NVMe Tool 简介
	├── Rules.mk		: 顶层 Makefile 编译规则
	├── scripts		: 脚本文件/工具
	└── zoo			: 一键编译&运行的脚本

Configuration
=============

| 通常情况下直接使用默认的全局配置文件 (.config) 即可。若需要修改配置选项，可以执行 :command:`make menuconfig` 命令，随后在弹出的 UI 窗口界面中选择合适的配置项并保存即可。

.. figure:: ./image/cfg_UI.png
	:align: center
	:width: 75%

	Graphical configuration interface

.. tip:: 更多配置信息请参考 :doc:`../configuration`

Compile
=======

| 在顶层目录下执行 :command:`make` 命令，编译整个工程。编译生成的成果物会自动拷贝到顶层目录的 :file:`release` 文件夹中。

Running
=======

| 编译结束后，切换到 :file:`release` 目录下：

1. 运行 :command:`./insmod.sh` 脚本，将 Linux 原生 NVMe 驱动替换成自定义的 NVMe 驱动模块；
#. 执行 :command:`./nvmetool /dev/nvme0` 命令，运行 NVMe 测试程序；( :literal:`/dev/nvme0` 是 NVMe 设备节点名称，需要根据实际情况来配置 )

.. note:: 
	
	1. 上述编译&运行过程也可以通过一键完成，只需要运行 :command:`./zoo` 脚本即可；
	#. 为了避免在编译&运行过程中因权限问题而导致失败，建议先执行 :command:`su root` 切换成 root 用户。

