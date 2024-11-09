#!/bin/bash

rm -rf build
meson setup build --buildtype=debug -DGLM=../../glm -DGLI=../../gli