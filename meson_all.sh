#!/bin/bash

pushd VK/VK
rm -rf build
meson setup build -DGLM=../../glm -DGLI=../../gli
pushd build
meson compile
popd
popd

pushd Glfw/Glfw
rm -rf build
meson setup build -DGLM=../../glm -DGLI=../../gli
pushd build
meson compile
./Glfw
popd
popd
