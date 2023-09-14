# Makefile for Sphinx documentation
RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = rst
BUILDDIR      = $(RELEASE_DOC_DIR)

build:
ifneq ($(DIR_EXIST),yes)
	$(Q)mkdir -p $(RELEASE_DOC_DIR)
endif
	$(Q)$(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)

clean:
	$(Q)rm -rf $(BUILDDIR)/*

distclean:
	$(Q)rm -rf $(BUILDDIR)

.PHONY: build clean distclean
