#!/usr/bin/env python
#-*- coding: utf-8 -*-

__author__ = 'yeqiang_xu <yeqiang_xu@maxio-tech.com>'
__version__ = '1.0.0'

import os
import functools
import inspect
try:
	import typing
	from typing import List, Dict, Tuple, Set, Union, Any
except ModuleNotFoundError:
	os.system('pip install typing')
	import typing
	from typing import List, Dict, Tuple, Set, Union, Any

def check_type(arg_val, arg_type) -> bool:
	if typing.get_origin(arg_type) == None:
		if isinstance(arg_type, (tuple, list, set)):
			# 'tuple', 'list', 'set'
			return True in [check_type(arg_val, sub_type) for sub_type in arg_type]
		elif arg_type is inspect._empty:
			return True
		elif arg_type is None:
			return arg_val is None
		elif isinstance(arg_type, type):
			# 'str', 'int', 'float', 'bool'
			return isinstance(arg_val, arg_type)
		elif callable(arg_type):
			# "arg_type" may pointer to a function, eg: lambda x: x > 0
			return arg_type(arg_val)
		else:
			return False
	else:
		if isinstance(list(), typing.get_origin(arg_type)):
			if not isinstance(arg_val, list):
				return False
			_sub_type = typing.get_args(arg_type)
			for _sub_val in arg_val:
				if not check_type(_sub_val, _sub_type):
					return False
			return True
		elif isinstance(dict(), typing.get_origin(arg_type)):
			if not isinstance(arg_val, dict):
				return False
			_sub_key_type = typing.get_args(arg_type)[0]
			_sub_val_type = typing.get_args(arg_type)[1]
			for _sub_key_val in arg_val.keys():
				if not check_type(_sub_key_val, _sub_key_type):
					return False
			for _sub_val_val in arg_val.values():
				if not check_type(_sub_val_val, _sub_val_type):
					return False
			return True
		elif isinstance(tuple(), typing.get_origin(arg_type)):
			if not isinstance(arg_val, tuple):
				return False
			_sub_types = typing.get_args(arg_type)
			if _sub_types[-1] == Ellipsis:
				_sub_type = _sub_types[0]
				for _sub_val in arg_val:
					if not check_type(_sub_val, _sub_type):
						return False
			else:
				if len(_sub_types) != len(arg_val):
					return False
				for _sub_val, _sub_type in zip(arg_val, _sub_types):
					if not check_type(_sub_val, _sub_type):
						return False
			return True
		elif isinstance(set(), typing.get_origin(arg_type)):
			if not isinstance(arg_val, set):
				return False
			_sub_type = typing.get_args(arg_type)
			for _sub_val in arg_val:
				if not check_type(_sub_val, _sub_type):
					return False
			return True
		else:
			return False

def check(function):
	@functools.wraps(function)
	def wrapper(*args, **kwargs):
		# Check input arguments at first
		_params = inspect.signature(function).parameters
		# print(_params)

		_args = list(_params.keys())
		# print('input args: %s' %_args)

		_types = list()
		for _item in _args:
			_types.append(_params[_item].annotation)
		# print('input type: %s' %_types)

		_values = list()
		for _item in inspect.getfullargspec(function).args:
			_values.append(inspect.getcallargs(function, *args, **kwargs)[_item])
		# print('input vals: %s' %_values)

		_results = dict()
		for _arg, _val, _type in zip(_args, _values, _types):
			_results[_arg] = check_type(_val, _type)
		# print(_results)

		_fails = list()
		for _item in _results.keys():
			if not _results[_item]:
				_fails.append(_item)

		if len(_fails) > 0:
			raise Exception(_fails)

		# Check return arg_val
		_ret_val = function(*args, **kwargs)
		_ret_type = inspect.signature(function).return_annotation
		# print('ret type: %s' %_ret_type)

		if not check_type(_ret_val, _ret_type):
			raise Exception(['return'])
		
		return _ret_val
	
	return wrapper

@check
def foo(arg1: int, arg2, arg3: (int, float) = 0, arg4: lambda x: x > 0 = 1, arg5: None = None) -> list:
	'''
	: An example function
	:
	: param arg1: type is <class 'int'>
	: param arg2: type is <class 'inspect._empty'>
	: param arg3: type is (<class 'int'>, <class 'float'>)
	: param arg4: required a number greater than zero, type is <function <lambda> at 0x7f3070b6d820>
	: param arg5: type is None
	: return: a list
	'''
	# return (arg1, arg2, arg3, arg4, arg5)
	return [arg1, arg2, arg3, arg4, arg5]

@check
def foo2(arg1: List[int], arg2: Dict[str, int], arg3: Tuple[int, str, float], arg4: Set[int]) -> tuple:
	'''
	: An example function for expansion types
	:
	: param arg1: type is typing.List[int]
	: param arg2: type is typing.Dict[str, int]
	: param arg3: type is typing.Tuple[int, str, float]
	: param arg4: type is typing.Set[int]
	'''
	return (arg1, arg2, arg3, arg4)

if __name__ == '__main__':
	
	''' Check Invalid '''
	# foo(1.0, None, 3.14, -1, 1)
	''' Check Pass '''
	# foo(1, 'name', 3.14, 6, None)

	''' Check Invalid '''
	# _list = [ 1, '2', 3.0 ]
	# _dict = { 'Name': 'Tony', 'Age': 18 }
	# _tuple = ( '1', 2, 3.0)
	# _set = { 1, '2', 3 }
	# foo2(_list, _dict, _tuple, _set)

	''' Check Pass '''
	_list2 = [ 1, 2, 3 ]
	_dict2 = { 'Year': 2023, 'Month': 10, 'Day': 10 }
	_tuple2 = (1, '2', 3.0)
	_set2 = { 1, 2, 3 }
	foo2(_list2, _dict2, _tuple2, _set2)

