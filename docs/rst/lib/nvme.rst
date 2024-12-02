=======
libnvme
=======

API
===

Overview
--------

Command
^^^^^^^

.. csv-table:: Command API table
	:header: "Function", "Description", "Note"
	:widths: 30, 60, 10

	"nvme_tamper_cmd", "篡改 SQ 中 cmd 自身及其关联(PRP,SGL)的数据内容，用于注错"
	"..."

Command
-------

.. doxygenfunction:: nvme_submit_64b_cmd
	:project: lib

.. doxygenfunction:: nvme_tamper_cmd
	:project: lib

.. doxygenfunction:: nvme_cmd_keep_alive
	:project: lib

.. doxygenfunction:: nvme_cmd_create_iosq
	:project: lib

.. doxygenfunction:: nvme_cmd_create_iocq
	:project: lib

.. doxygenfunction:: nvme_cmd_delete_iosq
	:project: lib

.. doxygenfunction:: nvme_cmd_delete_iocq
	:project: lib

.. doxygenfunction:: nvme_cmd_format_nvm
	:project: lib

.. doxygenfunction:: nvme_format_nvm
	:project: lib

.. doxygenfunction:: nvme_cmd_io_read
	:project: lib

.. doxygenfunction:: nvme_io_read
	:project: lib

.. doxygenfunction:: nvme_cmd_io_write
	:project: lib

.. doxygenfunction:: nvme_io_write
	:project: lib

.. doxygenfunction:: nvme_cmd_io_compare
	:project: lib

.. doxygenfunction:: nvme_io_compare
	:project: lib

.. doxygenfunction:: nvme_cmd_io_verify
	:project: lib

.. doxygenfunction:: nvme_io_verify
	:project: lib

.. doxygenfunction:: nvme_cmd_io_copy
	:project: lib

.. doxygenfunction:: nvme_io_copy
	:project: lib

Config Space Access
-------------------

.. doxygenfunction:: pci_read_config_byte
	:project: lib

.. doxygenfunction:: pci_read_config_word
	:project: lib

.. doxygenfunction:: pci_read_config_dword
	:project: lib

.. doxygenfunction:: pci_write_config_byte
	:project: lib

.. doxygenfunction:: pci_write_config_word
	:project: lib

.. doxygenfunction:: pci_write_config_dword
	:project: lib


Control Property Access
-----------------------

.. doxygenfunction:: nvme_read_ctrl_property
	:project: lib

.. doxygenfunction:: nvme_write_ctrl_property
	:project: lib




