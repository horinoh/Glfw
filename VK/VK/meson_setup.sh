#!/bin/bash

rm -rf build
meson setup build -DGLM=../../glm -DGLI=../../gli
meson configure build

