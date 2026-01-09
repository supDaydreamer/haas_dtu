VERSION_CODE	:= 0.9.6
CROSS			:= mipsel-openwrt-linux-
BUILD_PATH		:= build
VERSION_FILE	:= version
APP_NAME		:= haas_dtu
IS_DEBUG		:= 1

NEW_PAHO_DIR := src/lib/new_paho
NEW_PAHO_INCLUDE := -I $(NEW_PAHO_DIR)/include
NEW_PAHO_LIB := -L $(NEW_PAHO_DIR)/lib -lpaho-mqtt3a

CFLAGS_BASE := -std=gnu99
CFLAGS_DEBUG := -D DEBUG -g -O0
CFLAGS_NO_DEBUG := -g0 -Os
ifeq ($(IS_DEBUG), 1)
	CFLAGS_BASE += $(CFLAGS_DEBUG)
else
	CFLAGS_BASE += $(CFLAGS_NO_DEBUG)
endif
MODULE_CFLAGS := \
	-w \
	-D MQTTCLIENT_PLATFORM_HEADER=MQTTLinux.h \
	-D MG_ENABLE_MQTT_BROKER
APP_CFLAGS := \
	-W -Wall \
	-Wno-unknown-pragmas \
	-Wno-unused-parameter \
	-Wno-unused-variable \
	-Wno-unused-function \
	-Wno-pointer-sign \
	-Wno-sign-compare \
	-Wno-format \
	-Wno-unused-but-set-variable \
	-D MQTTCLIENT_PLATFORM_HEADER=MQTTLinux.h \
	-D BF_VERSION=\"v$(VERSION_CODE)\"
LDFLAGS := -lz -lcurl -ldl -lpthread -llfds711 \
	$(NEW_PAHO_LIB) \
	-L ../liblfds/liblfds7.1.1/liblfds711/bin

CC := $(CROSS)gcc
LD := $(CROSS)gcc
STRIP := $(CROSS)strip

APP_SOURCES := \
	src/bf_uart.c \
	src/bf_cmd.c \
	src/haas_mqtt.c \
	src/mqtt.c \
	src/uart.c \
	src/main.c \
	src/json.c \
	src/data.c \
	src/ini.c \
	src/udp.c \
	src/bfmsg.c \
	src/modbus.c \
  src/modbus-tcp.c \
	src/modbus-data.c \
  src/curl.c \
	src/mbtcp_server.c

CJSON_SOURCES := src/lib/cJSON.c

PAHO_DIR := src/lib/paho.mqtt.embedded-c
PAHO_INCLUDE := \
	-I $(PAHO_DIR)/MQTTClient-C/src \
	-I $(PAHO_DIR)/MQTTClient-C/src/linux \
	-I $(PAHO_DIR)/MQTTPacket/src
PAHO_SOURCES := \
	$(PAHO_DIR)/MQTTClient-C/src/MQTTClient.c \
	$(PAHO_DIR)/MQTTClient-C/src/linux/MQTTLinux.c \
	$(PAHO_DIR)/MQTTPacket/src/MQTTFormat.c \
	$(PAHO_DIR)/MQTTPacket/src/MQTTPacket.c \
	$(PAHO_DIR)/MQTTPacket/src/MQTTDeserializePublish.c \
	$(PAHO_DIR)/MQTTPacket/src/MQTTConnectClient.c \
	$(PAHO_DIR)/MQTTPacket/src/MQTTSerializePublish.c \
	$(PAHO_DIR)/MQTTPacket/src/MQTTSubscribeClient.c \
	$(PAHO_DIR)/MQTTPacket/src/MQTTUnsubscribeClient.c

HAAS_SOURCES := \
	src/mcu_sdk/mcu_api.c \
	src/mcu_sdk/protocol.c \
	src/mcu_sdk/system.c

MONGOOSE_SOURCES := src/lib/mongoose.c

OBJ_DIR := mk.obj
CJSON := $(OBJ_DIR)/cjson
PAHO := $(OBJ_DIR)/paho
HAAS := $(OBJ_DIR)/haas
MONGOOSE := $(OBJ_DIR)/mongoose
APP	:= $(OBJ_DIR)/app

CJSON_OBJ := $(patsubst %.c, $(CJSON)/%.o, $(CJSON_SOURCES))
PAHO_OBJ := $(patsubst %.c, $(PAHO)/%.o, $(PAHO_SOURCES))
HAAS_OBJ := $(patsubst %.c, $(HAAS)/%.o, $(HAAS_SOURCES))
MONGOOSE_OBJ := $(patsubst %.c, $(MONGOOSE)/%.o, $(MONGOOSE_SOURCES))
APP_OBJ	:= $(patsubst %.c, $(APP)/%.o, $(APP_SOURCES))

INCLUDE_BASE := $(PAHO_INCLUDE) \
	$(NEW_PAHO_INCLUDE) \
	-I src/lib \
	-I src/mcu_sdk \
	-I ../liblfds/liblfds7.1.1/liblfds711/inc

.PHONY: all clean_obj clean

all: build_start $(APP_NAME)
	@echo -ne "v$(VERSION_CODE)\t" > $(BUILD_PATH)/$(VERSION_FILE)
	@date +"[%F %T]" >> $(BUILD_PATH)/$(VERSION_FILE)
	@rm -rf mk.obj/
	@echo
	@echo "========== Build FINISHED ! =========="
	@echo

build_start:
	@make clean
	@echo
	@echo "---------- Build START ... ----------"
	@echo

$(APP_NAME): $(APP_OBJ) $(CJSON_OBJ) $(PAHO_OBJ) $(HAAS_OBJ) $(MONGOOSE_OBJ)
	@if [ ! -d $(BUILD_PATH) ]; then mkdir $(BUILD_PATH); fi
	$(LD) $^ -o $(BUILD_PATH)/$@ $(LDFLAGS)
	$(STRIP) $(BUILD_PATH)/$@
	make clean_obj

$(APP)/%.o:
	@mkdir -p $(shell dirname $@)
	@$(CC) $(patsubst $(APP)/%.o, %.c, $@) $(INCLUDE_BASE) -c -o $@ $(CFLAGS_BASE) $(APP_CFLAGS)

$(CJSON)/%.o:
	@mkdir -p $(shell dirname $@)
	@$(CC) $(patsubst $(CJSON)/%.o, %.c, $@) $(INCLUDE_BASE) -c -o $@ $(CFLAGS_BASE) $(MODULE_CFLAGS)

$(PAHO)/%.o:
	@mkdir -p $(shell dirname $@)
	@$(CC) $(patsubst $(PAHO)/%.o, %.c, $@) $(INCLUDE_BASE) -c -o $@ $(CFLAGS_BASE) $(MODULE_CFLAGS)

$(HAAS)/%.o:
	@mkdir -p $(shell dirname $@)
	@$(CC) $(patsubst $(HAAS)/%.o, %.c, $@) $(INCLUDE_BASE) -c -o $@ $(CFLAGS_BASE) $(MODULE_CFLAGS)

$(MONGOOSE)/%.o:
	@mkdir -p $(shell dirname $@)
	@$(CC) $(patsubst $(MONGOOSE)/%.o, %.c, $@) $(INCLUDE_BASE) -c -o $@ $(CFLAGS_BASE) $(MODULE_CFLAGS)

clean_obj:
	@rm -rf $(APP_NAME) $(APP) $(HAAS)

clean:
	@rm -rf *.gc* *.dSYM *.exe *.obj *.o a.out
	@rm -rf $(BUILD_PATH) $(OBJ_DIR)
