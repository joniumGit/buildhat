all:
	mkdir -p firmware-pico/build
	cd micro-ecc && gcc -DuECC_PLATFORM=0 -c uECC.c -o uECC.native.o
	cd sha2 && gcc -c sha2.c -o sha2.native.o
	cd firmware-pico/build && cmake .. && make -j4
	cd bhbl-pico && gcc sign.c ../micro-ecc/uECC.native.o ../sha2/sha2.native.o -o sign
	cd bhbl-pico && ./sign ../firmware-pico/build/main.bin
