rm test_main
mipsel-openwrt-linux-gcc main.c \
	-I../include \
	-L../lib -lpaho-mqtt3a \
	-lpthread -ldl \
	-W -Wall \
	-Os \
	-o test_main
