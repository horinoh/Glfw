pushd VK\VK
meson setup build --reconfigure --buildtype=debug -DGLM=..\..\glm -DGLI=..\..\gli -DCV=%OPENCV_SDK_PATH% 
pushd build
meson compile
popd
popd

pushd Glfw\Glfw
meson setup build --reconfigure --buildtype=debug -DGLFW=%GLFW_SDK_PATH% -DGLM=..\..\glm -DGLI=..\..\gli -DCV=%OPENCV_SDK_PATH% 
pushd build
meson compile
popd
popd

pushd Glfw\LookingGlass
meson setup build --reconfigure --buildtype=debug -DGLFW=%GLFW_SDK_PATH% -DGLM=..\..\glm -DGLI=..\..\gli -DCV=%OPENCV_SDK_PATH% 
pushd build
meson compile
popd
popd
