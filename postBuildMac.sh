#!/bin/sh
rm -rf bin
mkdir bin
cp -r build/bin/* bin/
rm -rf build/bin/res/
mkdir build/bin/res
cp -r res/* build/bin/res/
