@for %%i in (*.vert, *.frag, *.tese, *.tesc, *.geom) do glslangValidator -V %%i -o %%i.spv --target-env vulkan1.3 -g -Od
