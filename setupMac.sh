#!/bin/sh
git submodule update

cd source/external/gitsubmodules
cp -R assimp/include/assimp/ ../include/assimp/
cp -R assimp/build_dll/include/assimp/config.h ../include/assimp/

cp -R Vulkan-Headers/include/vulkan/ ../include/vulkan/

cp -R  glfw/include/GLFW/ ../include/GLFW/

cp -R  stb/stb_image.h ../include/stb/

cd ../

mkdir -p dll/mac
mkdir -p lib/mac
