#!/usr/bin/env python
#-*- coding: utf-8 -*-

import os, sys

cmd = 'pip' # default

def parse_param():
	global cmd

	if len(sys.argv) > 2:
		print("Invalid argument list: %s" %sys.argv)
		quit()
	elif len(sys.argv) == 2:
		cmds = ['pip', 'conda']
		if sys.argv[1] in cmds:
			cmd = sys.argv[1]
		else:
			print("Invalid argument: %s" %sys.argv[1])
			quit()

def check_module():
	global cmd

	modules = {
		'colorlog': 'colorlog',
		# 'PySide6': 'pyside6',
		'PyQt5': 'pyqt5',
		'qtawesome': 'qtawesome',
		'tqdm': 'tqdm',
		# 'typing': 'typing',
	}

	for mod in modules.keys():
		try:
			import mod
		except ModuleNotFoundError:
			direct = cmd + ' install ' + modules[mod]
			print(direct)
			os.system(direct)

def main():
	parse_param()
	check_module()

if __name__ == '__main__':
	main()
