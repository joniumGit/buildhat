# Dependencies

In the git `buildhat` directory:

```
  git clone https://github.com/kmackay/micro-ecc.git
  git clone https://github.com/ogay/sha2.git
```

Build the ECC and SHA2 libraries:

```
  cd micro-ecc
  gcc -DuECC_PLATFORM=0 -c uECC.c -o uECC.native.o
  cd ../sha2
  gcc -c sha2.c -o sha2.native.o
  cd ..
```

# Creating an image the bootloader can use

Execute

```
  export PICO_SDK_PATH=/path/to/pico/sdk
  cd firmware-pico
  mkdir -p build
  cd build
  cmake ..
  make
  cd ../..
```

This will create files in the `firmware-pico/build/` directory (for example
`firmware-pico/build/main.bin`) that are suitable for uploading to the
board using the bootloader. Note that this binary is intended to run from
the RP2040's RAM, and so is not suitable for programming into its flash.

# Generating the signature

Execute

```
  cd bhbl-pico
  gcc sign.c ../micro-ecc/uECC.native.o ../sha2/sha2.native.o -o sign
  ./sign ../firmware-pico/build/main.bin
  cd ..
```

This creates a file bhbl-pico/`signature.bin`.
