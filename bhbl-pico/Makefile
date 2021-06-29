all: upload sign

sign: sign.c
	gcc sign.c ../micro-ecc/uECC.native.o ../sha2/sha2.native.o -o sign

upload: upload.c
	gcc upload.c -o upload
