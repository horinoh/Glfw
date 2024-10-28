pushd VK\VK
meson setup build --reconfigure -DGLM=..\..\glm -DGLI=..\..\gli
pushd build
meson compile
popd
popd

pushd Glfw\Glfw
meson setup build --reconfigure -DGLFW=%GLFW_SDK_PATH% -DGLM=..\..\glm -DGLI=..\..\gli
pushd build
meson compile
rem Glfw.exe
popd
popd

pushd Glfw\LookingGlass
meson setup build --reconfigure -DGLFW=%GLFW_SDK_PATH% -DGLM=..\..\glm -DGLI=..\..\gli
pushd build
meson compile
rem LookingGlass.exe
popd
popd
