========
Appendix
========

Reference
=========

Terminology
===========

.. glossary::

	MCL
		Maximum Copy Length, copy command 中所有 source range 指定的 NLB 数量之和不能超过该字段的限制。解析 Identify Namesapce Data Structure, NVM Command Set 数据后可以得知 MCL 信息。

	MSSRL
		Maximum Single Source Range Length, copy command 中每个 source range 指定的 NLB 数量不能超过该字段的限制。解析 Identify Namesapce Data Structure, NVM Command Set 数据后可以得知 MSSRL 信息。
	
	PSDT
		PRP or SGL for Data Transfer, 位于 command dw0 中。

Others
======

NVMe Command
------------

Admin Command
^^^^^^^^^^^^^

.. doxygenenum:: nvme_directive_type

Vendor Command
""""""""""""""

.. doxygenenum:: nvme_admin_vendor_opcode

.. doxygenenum:: nvme_admin_vendor_option

I/O Command
^^^^^^^^^^^

NVM Command Set
"""""""""""""""

.. doxygenenum:: nvme_copy_desc_format

Zoned Namespace Command Set
"""""""""""""""""""""""""""
