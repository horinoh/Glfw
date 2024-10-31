meson setup build --reconfigure -DGLFW=%GLFW_SDK_PATH% -DGLM=..\..\glm -DGLI=..\..\gli -DCV=%OPENCV_SDK_PATH%
meson configure build
