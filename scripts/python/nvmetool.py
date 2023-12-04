#!/usr/bin/env python
#-*- coding: utf-8 -*-

import copy, json, os, sys
import qtawesome as qta
from PyQt5 import QtCore, QtGui, QtWidgets
from tqdm import tqdm
from typing import List, Dict, Tuple, Set, Union, Any

sys.path.append('./packages')
import log
from wrap import check

_AlignTop = QtCore.Qt.AlignTop
_AlignVCenter = QtCore.Qt.AlignVCenter
_AlignBottom = QtCore.Qt.AlignBottom
_AlignLeft = QtCore.Qt.AlignLeft
_AlignCenter = QtCore.Qt.AlignCenter
_AlignRight = QtCore.Qt.AlignRight

_Interactive = QtWidgets.QHeaderView.Interactive
_Stretch = QtWidgets.QHeaderView.Stretch
_Fixed = QtWidgets.QHeaderView.Fixed
_ResizeToContents = QtWidgets.QHeaderView.ResizeToContents

_ERR_OPNOTSUPP = -95

logger = log.UserLogger(os.getlogin())

class PushButton(object):
	def __init__(self, icon: QtGui.QIcon, name: str):
		self._widget = QtWidgets.QPushButton(icon, name)

	def setClickedHandler(self, handler):
		self._widget.clicked.connect(lambda: handler(self._widget.objectName()))

	def widget(self) -> QtWidgets.QPushButton:
		return self._widget

class TableSize(object):
	@check
	def __init__(self, row: int = 0, column: int = 0):
		self._row = row
		self._column = column

	@check
	def row(self) -> int:
		return self._row

	@check
	def column(self) -> int:
		return self._column

	@check
	def setRow(self, row: int) -> None:
		self._row = row

	@check
	def setColumn(self, column: int) -> None:
		self._column = column

class Table(object):
	NoEditTriggers = QtWidgets.QAbstractItemView.NoEditTriggers
	DoubleClicked = QtWidgets.QAbstractItemView.DoubleClicked

	@check
	def __init__(self, size: TableSize, hlabel: List[str] = list(), 
		resize: List[int] = list()):
		if not self.__checkSize(size):
			raise ValueError('row: %d, column: %d' %(size.row(), size.column()))

		self._size = copy.deepcopy(size)
		self._widget = QtWidgets.QTableWidget(size.row(), size.column())
		self._widget.setEditTriggers(self.NoEditTriggers)
		self.__createCellItem(size.row(), size.column())
		if len(hlabel):
			self.setHorizontalHeaderLabels(hlabel)
		if len(resize):
			self.setHorizontalResizeModes(resize)

	def clearContents(self) -> None:
		self._widget.clearContents()
		self._item.clear()

	@check
	def size(self) -> TableSize:
		return self._size

	@check
	def setSize(self, size: TableSize) -> None:
		if not self.__checkSize(size):
			raise ValueError('row: %d, column: %d' %(size.row(), size.column()))

		if self._size.row() == size.row() and \
			self._size.column() == size.column():
			return None

		self.clearContents()
		self._widget.setRowCount(size.row())
		self._widget.setColumnCount(size.column())
		self._size.setRow(size.row())
		self._size.setColumn(size.column())
		self.__createCellItem(size.row(), size.column())

	@check
	def horizontalHeaderLabels(self) -> List[str]:
		return list(self._horizontal_header_labels)

	@check
	def setHorizontalHeaderLabels(self, label: List[str]) -> bool:
		if len(label) > self._size.column():
			return False
		self._widget.setHorizontalHeaderLabels(label)
		self._horizontal_header_labels = list(label)
		return True

	@check
	def horizontalResizeModes(self) -> List[str]:
		return list(self._horizontal_resize_modes)

	@check
	def setHorizontalResizeModes(self, mode: List[int]) -> bool:
		if len(mode) > self._size.column():
			return False
		for _idx in range(0, len(mode)):
			self._widget.horizontalHeader().setSectionResizeMode(_idx, mode[_idx])
		self._horizontal_resize_modes = list(mode)
		return True

	@check
	def setCellItemText(self, row: int, column: int, text: str, 
			align: int = _AlignCenter) -> None:
		self._item[row][column].setText(text)
		self._item[row][column].setTextAlignment(align)
	
	@check
	def setCellItemBackground(self, row: int, column: int, color: str) -> None:
		self._item[row][column].setBackground(QtGui.QColor(color))

	@check
	def setEditTriggers(self, readonly: bool) -> None:
		if readonly:
			self._widget.setEditTriggers(self.NoEditTriggers)
		else:
			self._widget.setEditTriggers(self.DoubleClicked)

	def setItemClickedHandler(self, handler) -> None:
		self._widget.itemClicked.connect(lambda: handler(
			self._widget.currentRow(), self._widget.currentColumn()))

	def setItemDoubleClickedHandler(self, handler) -> None:
		self._widget.itemDoubleClicked.connect(lambda: handler(
			self._widget.currentRow(), self._widget.currentColumn()))

	def widget(self) -> QtWidgets.QTableWidget:
		return self._widget

	def __checkSize(self, size: TableSize) -> bool:
		if size.row() <= 0 or size.column() <= 0:
			return False
		return True

	def __createCellItem(self, row: int, column: int) -> None:
		if '_item' not in vars(self).keys():
			self._item = list()
		else:
			if len(self._item) > 0:
				raise Exception('item already exist!')

		for _row in range(0, row):
			_line = list()
			for _column in range(0, column):
				_item = QtWidgets.QTableWidgetItem()
				self._widget.setItem(_row, _column, _item)
				_line.append(_item)
			
			self._item.append(_line)


class JsonParser(QtCore.QObject):
	_PASS = chr(0x2713)
	_FAIL = chr(0x2717)
	_SKIP = chr(0x2500)

	parsingFinished = QtCore.pyqtSignal(object)

	@check
	def __init__(self, file: str = str()):
		super().__init__()
		self._key_top = {
			'case': self.__parseTopCase,
			'date': self.__parseTopDate,
			'version': self.__parseTopVersion,
		}
		self._key_case = {
			'result': self.__parseCommonResult,
			'speed': self.__parseCaseSpeed,
			'step': self.__parseCommonStep,
			'subcase': self.__parseCaseSubcase,
			'time': self.__parseCaseTime,
			'width': self.__parseCaseWidth,
		}
		self._key_subcase = {
			'result': self.__parseCommonResult,
			'step': self.__parseCommonStep,
		}

		if len(file) and self.__checkFile(file):
			self.__parseFile()

	@check
	def parse(self, file: str) -> None:
		if self.__checkFile(file) and self.__parseFile():
			self.parsingFinished.emit(self)

	@check
	def topDate(self) -> [str, None]:
		if 'date' in self._data_parsed.keys():
			return self._data_parsed['date']

	@check
	def topVersion(self) -> [str, None]:
		if 'version' in self._data_parsed.keys():
			return self._data_parsed['version']

	@check
	def caseCount(self) -> int:
		return self._data_parsed['case']['num']
	
	@check
	def caseData(self, idx: int) -> dict:
		return self._data_parsed['case']['data'][idx]

	@check
	def caseName(self, idx: int) -> [str, None]:
		if 'name' in self.caseData(idx).keys():
			return self.caseData(idx)['name']

	@check
	def caseResult(self, idx: int) -> [str, None]:
		if 'result' in self.caseData(idx).keys():
			return self.caseData(idx)['result']
	
	@check
	def isCaseResultPass(self, idx: int) -> bool:
		return self.caseData(idx)['result'] == self._PASS
	
	@check
	def isCaseResultFail(self, idx: int) -> bool:
		return self.caseData(idx)['result'] == self._FAIL
	
	@check
	def isCaseResultSkip(self, idx: int) -> bool:
		return self.caseData(idx)['result'] == self._SKIP

	@check
	def caseScore(self, idx: int) -> [str, None]:
		if 'score' in self.caseData(idx).keys():
			_score = self.caseData(idx)['score']
			return 'Pass:' + str(_score['pass']) + ', Fail:' + \
				str(_score['fail']) + ', Skip:' + str(_score['skip'])

	@check
	def caseSpeed(self, idx: int) -> [int, float, None]:
		if 'speed' in self.caseData(idx).keys():
			return self.caseData(idx)['speed']

	@check
	def caseStep(self, idx: int) -> [List[str], None]:
		if 'step' in self.caseData(idx).keys():
			return self.caseData(idx)['step']

	@check
	def caseTime(self, idx: int) -> [int, None]:
		if 'time' in self.caseData(idx).keys():
			return self.caseData(idx)['time']

	@check
	def caseWidth(self, idx: int) -> [int, None]:
		if 'width' in self.caseData(idx).keys():
			return self.caseData(idx)['width']

	@check
	def subcaseCount(self, idx: int) -> int:
		return self.caseData(idx)['subcase']['num']

	@check
	def subcaseData(self, idx: int, sub_idx: int) -> dict:
		return self.caseData(idx)['subcase']['data'][sub_idx]

	@check
	def subcaseName(self, idx: int, sub_idx: int) -> [str, None]:
		if 'name' in self.subcaseData(idx, sub_idx).keys():
			return self.subcaseData(idx, sub_idx)['name']

	@check
	def subcaseResult(self, idx: int, sub_idx: int) -> [str, None]:
		if 'result' in self.subcaseData(idx, sub_idx).keys():
			return self.subcaseData(idx, sub_idx)['result']

	@check
	def isSubcaseResultPass(self, idx: int, sub_idx: int) -> bool:
		return self.subcaseData(idx, sub_idx)['result'] == self._PASS
	
	@check
	def isSubcaseResultFail(self, idx: int, sub_idx: int) -> bool:
		return self.subcaseData(idx, sub_idx)['result'] == self._FAIL
	
	@check
	def isSubcaseResultSkip(self, idx: int, sub_idx: int) -> bool:
		return self.subcaseData(idx, sub_idx)['result'] == self._SKIP

	@check
	def subcaseStep(self, idx: int, sub_idx: int) -> [List[str], None]:
		if 'step' in self.subcaseData(idx, sub_idx).keys():
			return self.subcaseData(idx, sub_idx)['step']

	def setParsingFinishedHandler(self, handler) -> None:
		self.parsingFinished.connect(handler)

	@check
	def __checkFile(self, file: str) -> bool:
		if not os.path.isfile(file):
			logger.err("\"%s\" isn't a file! " %file)
			return False
		if os.path.splitext(file)[-1] != '.json':
			logger.err("\"%s\" isn't a JSON file! " %file)
			return False
		if '_file_path' not in vars(self).keys():
			self.__recordFile(file)
			return True
		if self._file_path == file and \
			self._file_mtime == os.path.getmtime(file):
			return False

		self.__recordFile(file)
		return True

	@check
	def __recordFile(self, file: str) -> None:
		self._file_path = file
		self._file_ctime = os.path.getctime(file)
		self._file_mtime = os.path.getmtime(file)
	
	@check
	def __parseFile(self) -> bool:
		logger.notice('Start parse %s ...' %self._file_path)
		self.__clearData()
		self.__readFile()
		return self.__parseFirstLayer()

	@check
	def __parseFirstLayer(self) -> bool:
		if not isinstance(self._data_raw, dict):
			logger.err('Required an object at here!')
			return False

		self._data_parsed = dict()
		for _key in self._data_raw.keys():
			if _key not in self._key_top.keys():
				logger.dbg('key |%s| is not support! skip ...' %_key)
				continue

			_ret = self._key_top[_key](self._data_parsed, self._data_raw[_key])
			if _ret == False:
				return False

		return True

	@check
	def __parseTopDate(self, parsed: dict, date: str) -> None:
		parsed['date'] = date

	@check
	def __parseTopCase(self, parsed: dict, raw: dict) -> bool:
		parsed['case'] = dict()
		parsed['case']['num'] = len(raw.keys())
		parsed['case']['data'] = list()

		for _name in tqdm(raw.keys(), desc = 'Parsing Case'):
			if not isinstance(raw[_name], dict):
				logger.err('Required an object at here!')
				return False

			logger.vdbg('Parse %s' %_name)
			_case = dict()
			_case['name'] = _name

			for _key in raw[_name].keys():
				if _key not in self._key_case.keys():
					logger.dbg('key |%s| is not support! skip ...' %_key)
					continue
				_ret = self._key_case[_key](_case, raw[_name][_key])
				if _ret == False:
					return False

			parsed['case']['data'].append(_case)

		return True

	@check
	def __parseTopVersion(self, parsed: dict, version: str) -> None:
		parsed['version'] = version

	@check
	def __parseCaseSpeed(self, parsed: dict, speed: [int, float]) -> None:
		parsed['speed'] = speed

	@check
	def __parseCaseSubcase(self, parsed: dict, raw: dict) -> bool:
		if not raw:
			return True

		parsed['score'] = {'pass': 0, 'fail': 0, 'skip': 0}
		parsed['subcase'] = dict()
		parsed['subcase']['num'] = len(raw.keys())
		parsed['subcase']['data'] = list()

		for _name in raw.keys():
			if not isinstance(raw[_name], dict):
				logger.err('Required an object at here!')
				return False
			
			logger.vdbg('Parse %s' %_name)
			_subcase = dict()
			_subcase['name'] = _name

			for _key in raw[_name].keys():
				if _key not in self._key_subcase.keys():
					logger.dbg('key |%s| is not support! skip ...' %_key)
					continue
				_ret = self._key_subcase[_key](_subcase, raw[_name][_key])
				if _ret == False:
					return False
			
			if _subcase['result'] == self._PASS:
				parsed['score']['pass'] += 1
			elif _subcase['result'] == self._FAIL:
				parsed['score']['fail'] += 1
			elif _subcase['result'] == self._SKIP:
				parsed['score']['skip'] += 1

			parsed['subcase']['data'].append(_subcase)

		return True

	@check
	def __parseCaseTime(self, parsed: dict, time: int) -> None:
		parsed['time'] = time

	@check
	def __parseCaseWidth(self, parsed: dict, width: int) -> None:
		parsed['width'] = width

	@check
	def __parseCommonResult(self, parsed: dict, result: int) -> None:
		if result < 0:
			if result == _ERR_OPNOTSUPP:
				parsed['result'] = self._SKIP
			else:
				parsed['result'] = self._FAIL
		else:
			parsed['result'] = self._PASS

	@check
	def __parseCommonStep(self, parsed: dict, step: List[str]) -> None:
		parsed['step'] = step

	@check
	def __clearData(self) -> None:
		if '_data_parsed' in vars(self).keys():
			del self._data_parsed
		if '_data_raw' in vars(self).keys():
			del self._data_raw

	@check
	def __readFile(self) -> None:
		_fp = open(self._file_path, 'r')
		self._data_raw = json.load(_fp)
		_fp.close()


class AppReportDetail(object):
	@check
	def __init__(self, title: str, size: QtCore.QSize):
		self._widget = QtWidgets.QDialog()
		self._widget.setWindowTitle(title)
		self._widget.resize(size.width(), size.height())
		self._size = QtCore.QSize(size)
		self._layout = QtWidgets.QVBoxLayout()
		self._widget.setLayout(self._layout)

		self._browser = QtWidgets.QTextBrowser()
		self._layout.addWidget(self._browser)

	@check
	def recvCaseStepData(self, data: List[str]) -> None:
		_num = 0
		for _item in data:
			_num += 1
			_txt = str(_num) + '. ' + _item
			self._browser.append(_txt)

	def show(self) -> None:
		self._widget.show()
		self._widget.exec_()

	def __checkSize(self, size: QtCore.QSize) -> bool:
		return size.width() > 0 and size.height() > 0

class AppSubcaseTable(object):
	def __init__(self):
		self._title = QtWidgets.QLabel()
		self._title.setAlignment(_AlignCenter)

		self._horizontal_label = ['SubCase', 'Result']
		self._horizontal_resize = [_Stretch, _ResizeToContents]
		self._align = [_AlignVCenter, _AlignCenter]

		self._table = Table(TableSize(1, len(self._horizontal_label)),
			self._horizontal_label, self._horizontal_resize)

	def titleWidget(self) -> QtWidgets.QLabel:
		return self._title

	def tableWidget(self) -> QtWidgets.QTableWidget:
		return self._table.widget()
	
	def updateTable(self, parser, case) -> None:
		if '_case' in vars(self).keys() and self._case == case:
			return None

		_row = parser.subcaseCount(case)
		_column = len(self._horizontal_label)

		self._case = case
		self._parser = parser

		self._title.setText(parser.caseName(self._case))
		self._table.setSize(TableSize(_row, _column))

		for _i in range(0, _row):
			for _j in range(0, _column):
				if self._horizontal_label[_j] == 'SubCase' and \
						parser.subcaseName(self._case, _i) != None:
					self._table.setCellItemText(_i, _j, 
						parser.subcaseName(self._case, _i), self._align[_j])

				elif self._horizontal_label[_j] == 'Result' and \
						parser.subcaseResult(self._case, _i) != None:
					self._table.setCellItemText(_i, _j, 
						parser.subcaseResult(self._case, _i), self._align[_j])
					self.__setResultBackground(parser, _i, _j)

		self._table.setItemDoubleClickedHandler(self.__itemDoubleClicked)

	@check
	def __setResultBackground(self, parser, row: int, column: int) -> None:
		if parser.isSubcaseResultPass(self._case, row):
			self._table.setCellItemBackground(row, column, 'lightgreen')
		elif parser.isSubcaseResultFail(self._case, row):
			self._table.setCellItemBackground(row, column, 'pink')
		elif parser.isSubcaseResultSkip(self._case, row):
			self._table.setCellItemBackground(row, column, 'lightblue')

	def __itemDoubleClicked(self, row: int, column: int) -> None:
		if self._horizontal_label[column] == 'SubCase':
			self.__showSubcaseDetails(row)

	def __showSubcaseDetails(self, row: int) -> None:
		if self._parser.subcaseStep(self._case, row) == None:
			return None
		
		_name = self._parser.subcaseName(self._case, row)
		logger.dbg('Show |%s| details...' %_name)
		_detail = AppReportDetail(_name, QtCore.QSize(480, 270))
		if self._parser.subcaseStep(self._case, row) != None:
			_detail.recvCaseStepData(self._parser.subcaseStep(self._case, row))
		_detail.show()

class AppCaseTable(QtCore.QObject):
	ctrlSubcaseTable = QtCore.pyqtSignal(int)

	def __init__(self):
		super().__init__()
		self._title = QtWidgets.QLabel('NVMe Test Report')
		self._title.setAlignment(_AlignCenter)

		self._horizontal_label = ['Time', 'GT/s', 'Width', 'Case', 'Result', 'Score']
		self._horizontal_resize = [_ResizeToContents, _ResizeToContents, 
			_ResizeToContents, _Stretch, _ResizeToContents, _ResizeToContents]
		self._align = [ _AlignCenter, _AlignCenter, _AlignCenter, 
			_AlignVCenter, _AlignCenter, _AlignVCenter]

		self._table = Table(TableSize(1, len(self._horizontal_label)), 
			self._horizontal_label, self._horizontal_resize)

	def setCtrlSubcaseTableHandler(self, handler):
		self.ctrlSubcaseTable.connect(handler)

	def titleWidget(self) -> QtWidgets.QLabel:
		return self._title
	
	def tableWidget(self) -> QtWidgets.QTableWidget:
		return self._table.widget()

	def updateTable(self, parser) -> None:
		logger.notice('Start update test report...')

		_row = parser.caseCount()
		_column = len(self._horizontal_label)

		self._parser = parser
		self._title.setText(parser.topDate() + ' - NVMe Test Report')
		self._table.setSize(TableSize(_row, _column))

		for _i in range(0, _row):
			for _j in range(0, _column):
				if self._horizontal_label[_j] == 'Time' and \
						parser.caseTime(_i) != None:
					self._table.setCellItemText(_i, _j, 
						str(parser.caseTime(_i)), self._align[_j])
				elif self._horizontal_label[_j] == 'GT/s' and \
						parser.caseSpeed(_i) != None:
					self._table.setCellItemText(_i, _j, 
						str(parser.caseSpeed(_i)), self._align[_j])
				elif self._horizontal_label[_j] == 'Width' and \
						parser.caseWidth(_i) != None:
					self._table.setCellItemText(_i, _j, 
						str(parser.caseWidth(_i)), self._align[_j])
				elif self._horizontal_label[_j] == 'Case' and \
						parser.caseName(_i) != None:
					self._table.setCellItemText(_i, _j, 
						parser.caseName(_i), self._align[_j])
				elif self._horizontal_label[_j] == 'Result' and \
						parser.caseResult(_i) != None:
					self._table.setCellItemText(_i, _j, 
						parser.caseResult(_i), self._align[_j])
					self.__setResultBackground(parser, _i, _j)
				elif self._horizontal_label[_j] == 'Score' and \
						parser.caseScore(_i) != None:
					self._table.setCellItemText(_i, _j, 
						parser.caseScore(_i), self._align[_j])

		self._table.setItemClickedHandler(self.__itemClicked)
		self._table.setItemDoubleClickedHandler(self.__itemDoubleClicked)

	@check
	def __setResultBackground(self, parser, row: int, column: int) -> None:
		if parser.isCaseResultPass(row):
			self._table.setCellItemBackground(row, column, 'lightgreen')
		elif parser.isCaseResultFail(row):
			self._table.setCellItemBackground(row, column, 'pink')
		elif parser.isCaseResultSkip(row):
			self._table.setCellItemBackground(row, column, 'lightblue')

	def __itemClicked(self, row: int, column: int) -> None:
		if self._horizontal_label[column] == 'Score':
			self.ctrlSubcaseTable.emit(row)

	def __itemDoubleClicked(self, row: int, column: int) -> None:
		if self._horizontal_label[column] == 'Case':
			self.__showCaseDetails(row)

	def __showCaseDetails(self, row: int) -> None:
		if self._parser.caseStep(row) == None:
			return None

		_name = self._parser.caseName(row)
		_detail = AppReportDetail(_name, QtCore.QSize(480, 270))
		if self._parser.caseStep(row) != None:
			_detail.recvCaseStepData(self._parser.caseStep(row))
		_detail.show()

class AppInputFile(object):
	def __init__(self):
		self._frame = dict()
		self._frame['widget'] = QtWidgets.QWidget()
		self._frame['layout'] = QtWidgets.QGridLayout()
		self._frame['widget'].setLayout(self._frame['layout'])

		self._label = dict()
		self._label['widget'] = QtWidgets.QLabel(chr(0xf002) + ' ' + '文件  ')
		self._label['widget'].setFont(qta.font('fa', 16))
		self._label['widget'].setAlignment(_AlignCenter)
		self._frame['layout'].addWidget(self._label['widget'], 0, 0, 1, 1)

		self._input = dict()
		self._input['widget'] = QtWidgets.QLineEdit()
		self._input['widget'].setPlaceholderText('输入待解析的 JSON 文件路径')
		self._frame['layout'].addWidget(self._input['widget'], 0, 1, 1, 8)

		self._button = dict()
		self._button['widget'] = QtWidgets.QPushButton('打开')
		self._button['widget'].clicked.connect(self.__buttonClickedHandler)
		self._frame['layout'].addWidget(self._button['widget'], 0, 9, 1, 1)

	def widget(self) -> QtWidgets.QWidget:
		return self._frame['widget']
	
	def setRecvFileHandler(self, handler) -> None:
		self._input['widget'].editingFinished.connect(lambda: 
			handler(self._input['widget'].text()))
	
	def __buttonClickedHandler(self):
		_file = QtWidgets.QFileDialog.getOpenFileName(None, 'Open File', os.getcwd(), 
			'JavaScript Object Notation (*json)')
		self._input['widget'].setText(_file[0])
		self._input['widget'].editingFinished.emit()

class AppHomePage(object):
	def __init__(self):
		self._widget = QtWidgets.QWidget()
		self._layout = QtWidgets.QGridLayout()
		self._widget.setLayout(self._layout)

		self._image = QtWidgets.QLabel()
		self._image.setPixmap(QtGui.QPixmap(getSourcePath('./assets/universe.jpg')).scaled(768, 432))
		self._layout.addWidget(self._image, 0, 0, 1, 1, _AlignCenter)

	def hide(self) -> None:
		self._widget.hide()

	def show(self) -> None:
		self._widget.show()

	def widget(self) -> QtWidgets.QWidget:
		return self._widget

class AppReportPage(object):
	def __init__(self):
		self._parser = JsonParser()

		self._widget = QtWidgets.QWidget()
		self._layout = QtWidgets.QGridLayout()
		self._widget.setLayout(self._layout)

		self._input_file = AppInputFile()
		self._layout.addWidget(self._input_file.widget(), 0, 0, 1, 1, _AlignTop)

		self._case = AppCaseTable()
		self._layout.addWidget(self._case.titleWidget(), 1, 0, 1, 1)
		self._layout.addWidget(self._case.tableWidget(), 2, 0, 1, 1)

		self._subcase = AppSubcaseTable()
		self._layout.addWidget(self._subcase.titleWidget(), 3, 0, 1, 1)
		self._layout.addWidget(self._subcase.tableWidget(), 4, 0, 1, 1)
		self._subcase.titleWidget().hide()
		self._subcase.tableWidget().hide()
		self._show_subcase = None

		self.__initSignalHandler()

	def hide(self) -> None:
		self._widget.hide()
	
	def show(self) -> None:
		self._widget.show()

	def widget(self) -> QtWidgets.QWidget:
		return self._widget

	def __initSignalHandler(self) -> None:
		self._input_file.setRecvFileHandler(self._parser.parse)
		self._parser.setParsingFinishedHandler(self._case.updateTable)
		self._case.setCtrlSubcaseTableHandler(self.__ctrlSubcaseTable)

	def __ctrlSubcaseTable(self, row: int) -> None:
		if self._parser.caseScore(row) == None:
			return None

		if self._show_subcase != row:
			self.__showSubcaseTable(row)
		else:
			self.__hideSubcaseTable(row)

	def __showSubcaseTable(self, row: int) -> None:
		logger.dbg('Show |%s| table...' %self._parser.caseName(row))
		self._subcase.updateTable(self._parser, row)
		self._subcase.titleWidget().show()
		self._subcase.tableWidget().show()
		self._show_subcase = row
	
	def __hideSubcaseTable(self, row: int) -> None:
		logger.dbg('Hide |%s| table...' %self._parser.caseName(row))
		self._subcase.titleWidget().hide()
		self._subcase.tableWidget().hide()
		self._layout.update()
		self._show_subcase = None

class AppSidebar(object):
	@check
	def __init__(self, btn: Dict[str, list]):
		self._widget = QtWidgets.QWidget()
		self._layout = QtWidgets.QGridLayout()
		self._widget.setLayout(self._layout)

		_row = 0
		self._btn = dict()
		self._page = dict()
		for _name in btn.keys():
			self._btn[_name] = PushButton(btn[_name][1], btn[_name][0])
			self._btn[_name].widget().setObjectName(_name)
			self._btn[_name].setClickedHandler(self.__switchPage)
			self._layout.addWidget(self._btn[_name].widget(), _row, 0, 1, 1)
			_row += 1

		# default to display first page
		self._show = list(btn.keys())[0]

	@check
	def bindPageData(self, name: str, data) -> bool:
		if name not in self._btn.keys():
			logger.err('button |%s| is not exist!' %name)
			return False
		self._page[name] = data
		return True

	def widget(self) -> QtWidgets.QWidget:
		return self._widget

	def __switchPage(self, name: str) -> None:
		if name == self._show:
			return None
		if name not in self._page.keys():
			logger.warn('Page |%s| is empty! skip...' %name)
			return None
		logger.dbg('Switch page to |%s| ...' %name)
		self._page[self._show].hide()
		self._page[name].show()
		self._show = name

class AppCentralWidget():
	def __init__(self):
		self._widget = QtWidgets.QWidget()
		self._layout = QtWidgets.QGridLayout()
		self._widget.setLayout(self._layout)

		self.__init_sidebar()
		self._layout.addWidget(self._sidebar.widget(), 0, 0, 1, 2)

		self._page_home = AppHomePage()
		# self._page_home.hide()
		self._sidebar.bindPageData('home', self._page_home)
		self._layout.addWidget(self._page_home.widget(), 0, 2, 1, 10)

		self._page_report = AppReportPage()
		self._page_report.hide()
		self._sidebar.bindPageData('report', self._page_report)
		self._layout.addWidget(self._page_report.widget(), 0, 2, 1, 10)

	
	def widget(self) -> QtWidgets.QWidget:
		return self._widget

	def __init_sidebar(self):
		_btn = { 'home': ['主页', qta.icon('fa.home')],
			'config': ['配置选项', qta.icon('mdi.cog')],
			'report': ['测试报告', qta.icon('fa.wpforms')] }
		self._sidebar = AppSidebar(_btn)

class AppWindow(object):
	@check
	def __init__(self, title: str, size: QtCore.QSize):
		if not self.__checkSize(size):
			raise ValueError('width: %d, height: %d' %(size.width(), size.height()))
		self._widget = QtWidgets.QMainWindow()
		self._widget.setWindowTitle(title)
		self._widget.resize(size.width(), size.height())
		self._size = QtCore.QSize(size)

		self._central = AppCentralWidget()
		self._widget.setCentralWidget(self._central.widget())
		self._widget.show()

	def __checkSize(self, size: QtCore.QSize) -> bool:
		return size.width() > 0 and size.height() > 0

def getSourcePath(filepath):
	""" Get real source path in different situation (eg: pyinstaller)

	:param filepath: The original path of the source file
	:type filepath: str
	:return: The actual path of the source file
	:rtype: str
	"""
	if hasattr(sys, '_MEIPASS'):
		return os.path.join(sys._MEIPASS, filepath)
	else:
		return os.path.join(os.path.abspath("."), filepath)

def main():
	logger.setFileEnable(False)
	logger.setConsoleLevel(log.DEBUG)

	_app = QtWidgets.QApplication(sys.argv)
	_ui = AppWindow('NVMe Tool', QtCore.QSize(960, 540))
	sys.exit(_app.exec_())

if __name__ == '__main__':
	main()
