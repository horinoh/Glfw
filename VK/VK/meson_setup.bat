meson setup build --reconfigure -DGLM=..\..\glm -DGLI=..\..\gli -DCV=%OPENCV_SDK_PATH% -DHAILO="%HAILORT_SDK_PATH%"
meson configure build
