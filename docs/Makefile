# Makefile for Sphinx documentation
RULES_MK := $(addsuffix Rules.mk,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
-include $(RULES_MK)

.DEFAULT_GOAL := build

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = rst

doxygen:
	$(Q)mkdir -p $(RELEASE_DOC_DOXYGEN_DIR)
	$(Q)doxygen

sphinx:
	$(Q)mkdir -p $(RELEASE_DOC_SPHINX_DIR)
	$(Q)$(SPHINXBUILD) -b html $(SOURCEDIR) $(RELEASE_DOC_SPHINX_DIR)

build: doxygen sphinx

clean:
	$(Q)rm -rf $(RELEASE_DOC_SPHINX_DIR)
	$(Q)rm -rf $(RELEASE_DOC_DOXYGEN_DIR)

distclean: clean

.PHONY: doxygen sphinx build clean distclean
