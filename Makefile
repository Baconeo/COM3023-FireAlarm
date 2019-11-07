CONTIKI_PROJECT = fire_alarm
APPS += powertrace
all: $(CONTIKI_PROJECT)

#UIP_CONF_IPV6=1

CONTIKI = ../..
include $(CONTIKI)/Makefile.include
