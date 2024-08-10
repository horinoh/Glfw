pushd VK\VK
meson setup build --reconfigure -DGLM=..\..\glm
pushd build
meson compile
popd
popd

pushd Glfw\Glfw
meson setup build --reconfigure -DGLFW=%GLFW_SDK_PATH% -DGLM=..\..\glm
pushd build
meson compile
Glfw.exe
popd
popd
