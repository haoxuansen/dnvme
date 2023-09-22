#!/usr/bin/python env
#-*- coding: utf-8 -*-

import sys
import os
import sphinx
import subprocess

# Get Sphinx version
major, minor, patch = sphinx.version_info[:3]

print('Sphinx version:%d.%d.%d' %(major, minor, patch))

# General information about the project.
project = 'NVMe Tool'
copyright = 'none'
author = 'yeqiang_xu'

version = '0.1'
release = '0.1'

# A list of ignored prefixes for module index sorting.
#modindex_common_prefix = []

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

# The encoding of source files.
#source_encoding = 'utf-8-sig'

# The master toctree document.
master_doc = 'index'

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
exclude_patterns = ['output']

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

primary_domain = 'c'
highlight_language = 'none'

# If true, '()' will be appended to :func: etc. cross-reference text.
#add_function_parentheses = True

# If true, the current module name will be prepended to all description
# unit titles (such as .. function::).
#add_module_names = True

# =========================================================================== #
# Options for Sphinx Extensions
# =========================================================================== #

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
	# Inline extension
	'sphinx.ext.todo',
	# Third extension
	'breathe',
	# 'exhale',
	'sphinx_design',
	# 'sphinx_tabs.tabs',
	'sphinx_togglebutton',
]

# --------------------------------------------------------------------------- #
# breathe configuration
# --------------------------------------------------------------------------- #

# Determine whether or not we are building on the Read the Docs servers.
read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'
if read_the_docs_build:
	subprocess.call('mkdir -p ../../release/docs/doxygen', shell=True)
	subprocess.call('cd ..; doxygen', shell=True)

if 'breathe' in extensions:
	breathe_projects = {
		"nvmetool": "../../release/docs/doxygen/xml"
	}

	breathe_default_project = 'nvmetool'

	breathe_domain_by_extension = {
		"c" : "c",
		"h" : "c",
	}

# --------------------------------------------------------------------------- #
# exhale configuration
# --------------------------------------------------------------------------- #
if 'exhale' in extensions:
	exhale_args = {
		"containmentFolder":		"./api",
		"rootFileName":			"library_root.rst",
		"rootFileTitle":		"Library API",
		"doxygenStripFromPath":		"..",
		# Suggested optional arguments
		"createTreeView":		True,
		"exhaleExecutesDoxygen":	False,
		"exhaleDoxygenStdin":		"INPUT = ../../lib"
	}

# --------------------------------------------------------------------------- #
# sphinx.ext.todo configuration
# --------------------------------------------------------------------------- #
if 'sphinx.ext.todo' in extensions:
	# If true, `todo` and `todoList` produce output, else they produce nothing.
	todo_include_todos = True

	# If true, 'todo' emits a warning for each TODO entries.
	todo_emit_warnings = False

# --------------------------------------------------------------------------- #
# sphinx-togglebutton configuration
# --------------------------------------------------------------------------- #
if 'sphinx_togglebutton' in extensions:
	togglebutton_hint = 'click to show'
	togglebutton_hint_hide = 'click to hide'

# --------------------------------------------------------------------------- #
# Options for HTML Output
# --------------------------------------------------------------------------- #

# The Read the Docs theme is available from
# - https://github.com/snide/sphinx_rtd_theme
# - https://pypi.python.org/pypi/sphinx_rtd_theme
# - python-sphinx-rtd-theme package (on Debian)
try:
	import sphinx_rtd_theme
	html_theme = 'sphinx_rtd_theme'
	html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]
except ImportError:
	sys.stderr.write('Warning: The Sphinx \'sphinx_rtd_theme\' HTML \
		theme was not found. Make sure you have the theme installed \
		to produce pretty HTML output. Falling back to the default \
		theme.\n')

# html_theme = 'classic'
# html_theme = 'press'
# html_theme = 'sphinx_book_theme'

# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
if html_theme == 'sphinx_rtd_theme':
	html_theme_options = {
		# 'collapse_navigation': True,
		# 'includehidden': True,
		# "logo_only": True,
		# 'navigation_depth': 4,
		# 'prev_next_buttons_location': 'bottom',
		# 'sticky_navigation': True,
		# 'style_external_links': True,
		# 'titles_only': True
	}

if html_theme == 'sphinx_book_theme':
	html_theme_options = {
		"home_page_in_toc": True,
		"show_navbar_depth": 3,
		"show_toc_level": 1,
	}

# The name for this set of Sphinx documents.  If None, it defaults to
# "<project> v<release> documentation".
html_title = project + ' v' + version

# A shorter title for the navigation bar.  Default is the same as html_title.
#html_short_title = None

# The name of an image file (relative to this directory) to place at the top
# of the sidebar.
# html_logo = "./image/aircraft.svg"

# The name of an image file (within the static path) to use as favicon of the
# docs.  This file should be a Windows icon file (.ico) being 16x16 or 32x32
# pixels large.
# html_favicon = "./image/aircraft.svg"

# This is the file name suffix for HTML files (e.g. ".xhtml").
#html_file_suffix = '.html'

# Language to be used for generating the HTML full-text search index.
# Sphinx supports the following languages:
#   'da', 'de', 'en', 'es', 'fi', 'fr', 'h', 'it', 'ja'
#   'nl', 'no', 'pt', 'ro', 'r', 'sv', 'tr'
html_search_language = 'en'

# Add any extra paths that contain custom files (such as robots.txt or
# .htaccess) here, relative to this directory. These files are copied
# directly to the root of the documentation.
#html_extra_path = []

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
#html_static_path = ['sphinx-static']

# If true, links to the reST sources are added to the pages.
#html_show_sourcelink = True
