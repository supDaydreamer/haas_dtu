gcc lib/mongoose.c bfbr.c -o test.bfbr -Ilib -lpthread -DMG_ENABLE_MQTT_BROKER -DBFBR_MAIN_TEST

handle_sigint() {
	echo "====== bfbr test end ======"
	rm test.bfbr
}

trap handle_sigint SIGINT

echo "===== bfbr test start ====="
./test.bfbr

