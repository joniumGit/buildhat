#!/bin/bash
set -e
rm -rf release
mkdir -p release release/docs/ release/bhbl-pico release/firmware-pico
for i in `cat releasefilelist` ; do cp $i release/$i ; done
