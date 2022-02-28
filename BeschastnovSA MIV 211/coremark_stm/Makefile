# name of your application
APPLICATION = coremark_stm

# If no BOARD is found in the environment, use this default:
BOARD ?= nucleo-f401re

USEMODULE += xtimer

CFLAGS += -DITERATIONS=10000
LINKFLAGS += -u _printf_float
WERROR = 0

CFLAGS_OPT = -O3


# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include