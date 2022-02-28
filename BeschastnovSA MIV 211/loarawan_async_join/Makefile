# name of your application
APPLICATION = lorawan

# If no BOARD is found in the environment, use this default:
BOARD ?= unwd-range-l1-r3

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

USEMODULE += lptimer
USEMODULE += bme280
USEPKG += cayenne-lpp

# LoRa definitions
USEMODULE += sx127x
USEPKG += semtech-loramac
USEMODULE += crypto

# We need CRYPTO_AES for loramac, as it uses Riot's crypto
CFLAGS += -DCRYPTO_AES

LORA_REGION = RU864
CFLAGS += -DLORA_REGION=\"$(LORA_REGION)\"
# Disable LoRaWAN duty cycle restrictions
CFLAGS += -DDISABLE_LORAMAC_DUTYCYCLE
CFLAGS += -DLORAWAN_DONT_USE_FPORT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
