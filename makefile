# ----------------------------
# Makefile Options
# ----------------------------

NAME = CEPAD
DESCRIPTION = "USB key sender"
COMPRESSED = NO

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
