#!/usr/bin/env python
#-*- coding: utf-8 -*-

__author__ = 'yeqiang_xu <yeqiang_xu@maxio-tech.com>'
__version__ = '1.0.0'

import os
import sys
import datetime
import logging
import colorlog

VERBOSE_DEBUG = 5
DEBUG = logging.DEBUG
INFO = logging.INFO
NOTICE = 25
WARN = logging.WARNING
ERROR = logging.ERROR
CRITICAL = logging.CRITICAL

class UserLogger(object):
	def __init__(self, user: str):
		self.user = user
		self.date = datetime.date.today()
		self.logger = logging.getLogger(self.user)
		logging.addLevelName(NOTICE, 'NOTICE')
		logging.addLevelName(VERBOSE_DEBUG, 'VERBOSE_DEBUG')
		self.logger.setLevel(VERBOSE_DEBUG)
		self.color = {
			'VERBOSE_DEBUG': 'white',
			'DEBUG': 'bold_white',
			'INFO': 'green',
			'NOTICE': 'blue',
			'WARNING': 'yellow',
			'ERROR': 'red',
			'CRITICAL': 'purple',
		}

		# Set log file options
		self.file = {}
		self.file['name'] = './' + str(self.date) + '.log'
		self.file['formatter'] = logging.Formatter(
			'%(asctime)s %(levelname)8s %(name)8s %(process)d %(message)s')
		self.file['handler'] = logging.FileHandler(self.file['name'])
		self.file['handler'].setFormatter(self.file['formatter'])
		self.file['handler'].setLevel(DEBUG)
		self.file['enable'] = True

		# Set log console options
		self.console = {}
		self.console['formatter'] = colorlog.ColoredFormatter(
			fmt = '%(log_color)s[%(levelno)-2s] %(message)s',
			log_colors = self.color)
		self.console['handler'] = logging.StreamHandler()
		self.console['handler'].setFormatter(self.console['formatter'])
		self.console['handler'].setLevel(DEBUG)
		self.console['enable'] = True

		# Add log handler to Logger
		self.logger.addHandler(self.file['handler'])
		self.logger.addHandler(self.console['handler'])
	
	def __del__(self):
		if self.file['enable']:
			self.logger.removeHandler(self.file['handler'])
			self.file['enable'] = False
		if self.console['enable']:
			self.logger.removeHandler(self.console['handler'])
			self.console['enable'] = False

		self.file['handler'].close()
		self.console['handler'].close()

	def vdbg(self, msg):
		self.logger.log(VERBOSE_DEBUG, os.path.basename(sys._getframe(1).f_code.co_filename) + 
			',' + str(sys._getframe(1).f_lineno) + ': ' + msg)

	def dbg(self, msg):
		self.logger.log(DEBUG, os.path.basename(sys._getframe(1).f_code.co_filename) + 
			',' + str(sys._getframe(1).f_lineno) + ': ' + msg)

	def info(self, msg):
		self.logger.log(INFO, os.path.basename(sys._getframe(1).f_code.co_filename) + 
			',' + str(sys._getframe(1).f_lineno) + ': ' + msg)

	def notice(self, msg):
		self.logger.log(NOTICE, os.path.basename(sys._getframe(1).f_code.co_filename) + 
			',' + str(sys._getframe(1).f_lineno) + ': ' + msg)
	
	def warn(self, msg):
		self.logger.log(WARN, os.path.basename(sys._getframe(1).f_code.co_filename) + 
			',' + str(sys._getframe(1).f_lineno) + ': ' + msg)

	def err(self, msg):
		self.logger.log(ERROR, os.path.basename(sys._getframe(1).f_code.co_filename) + 
			',' + str(sys._getframe(1).f_lineno) + ': ' + msg)

	def crit(self, msg):
		self.logger.log(CRITICAL, os.path.basename(sys._getframe(1).f_code.co_filename) + 
			',' + str(sys._getframe(1).f_lineno) + ': ' + msg)

	def setConsoleLevel(self, level: int) -> None:
		if isinstance(level, int) == False:
			raise ValueError("input param type is %s, but required %s" 
				%(type(level), type(1)))

		self.console['handler'].setLevel(level)

	def setFileLevel(self, level: int) -> None:
		if isinstance(level, int) == False:
			raise ValueError("input param type is %s, but required %s" 
				%(type(level), type(1)))
			
		self.file['handler'].setLevel(level)

	def __setHandlerEnable(self, handler, enable: bool) -> bool:
		if enable:
			self.logger.addHandler(handler)
		else:
			self.logger.removeHandler(handler)

		return enable

	def setConsoleEnable(self, enable: bool) -> None:
		if isinstance(enable, bool) == False:
			raise ValueError("input param type is %s, but required %s" 
				%(type(enable), type(True)))
		if self.console['enable'] == enable:
			return None

		self.console['enable'] = self.__setHandlerEnable(self.console['handler'], enable)
	
	def setFileEnable(self, enable: bool) -> None:
		if isinstance(enable, bool) == False:
			raise ValueError("input param type is %s, but required %s" 
				%(type(enable), type(True)))
		if self.file['enable'] == enable:
			return None
		
		self.file['enable'] = self.__setHandlerEnable(self.file['handler'], enable)


if __name__ == '__main__':
	_errno = 10
	_logger = UserLogger(os.getlogin())
	_logger.setConsoleLevel(VERBOSE_DEBUG)
	_logger.vdbg("Bubble message!")
	_logger.dbg("Debug message!")
	_logger.info("Information message!")
	_logger.notice("Notice message!")
	_logger.warn("Warning message!")
	_logger.err("Error message!(errno:%d)" %_errno)
	_logger.crit("Critical message!")
