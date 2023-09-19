#     Priority to include "Rules.mk" of the parent direcotry, then include                                                                    
# "Rules.mk" of the current directory.
RULES_MK := $(addsuffix ../$(notdir $(RULES_MK)),$(dir $(RULES_MK)))
-include $(RULES_MK)
 
ifeq ($(RULES_LIST),)
RULES_LIST := $(MAKEFILE_LIST)
else
RULES_LIST := $(filter-out $(lastword $(RULES_LIST)),$(RULES_LIST))
endif

# --------------------------------------------------------------------------- #
# Directory
# --------------------------------------------------------------------------- #

RELEASE_DOC_SPHINX_DIR := $(RELEASE_DIR)/docs/sphinx
RELEASE_DOC_DOXYGEN_DIR := $(RELEASE_DIR)/docs/doxygen

