#!/bin/bash
set -e
rm -rf release
mkdir -p release release/buildhat/ release/buildhat/docs/ release/buildhat/bhbl-pico release/buildhat/firmware-pico
for i in `cat releasefilelist` ; do cp $i release/buildhat/$i ; done
