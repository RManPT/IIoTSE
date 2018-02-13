# Includes the project-conf configuration file
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = iotnode
#CONTIKI_TARGET_SOURCEFILES += button-sensor.c 
CONTIKI_TARGET_SOURCEFILES += adc-sensors.c 
#CONTIKI_TARGET_SOURCEFILES += zoul-sensors.c 
CONTIKI_TARGET_SOURCEFILES += dht22.c 
#CONTIKI_TARGET_SOURCEFILES += uip-udp-packet.c 
#CONTIKI_TARGET_SOURCEFILES += simple-udp.c 
#CONTIKI_TARGET_SOURCEFILES += packetbuf.c 
#CONTIKI_TARGET_SOURCEFILES += uip-ds6.c 
CONTIKI_TARGET_SOURCEFILES += uip-debug.c

all: $(CONTIKI_PROJECT)

CONTIKI = ../../../..

# Linker optimizations
SMALL = 1
CONTIKI_WITH_IPV6 = 1

include $(CONTIKI)/Makefile.include




