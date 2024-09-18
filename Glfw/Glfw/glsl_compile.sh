#!/bin/bash

for i in *.vert *.frag *.tese *.tesc *.geom; do
    test -e $i && glslangValidator -V $i -o $i.spv --target-env vulkan1.3 -g -Od
done

