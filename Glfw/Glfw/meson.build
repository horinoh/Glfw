project('Glfw', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

# setup の引数として指定する
GLFW = get_option('GLFW')
VK = get_option('VK')
GLM = get_option('GLM')

# 追加のインクルード
Inc = [ 
        include_directories(GLFW / 'include'),
        include_directories(VK / 'Include'),
        include_directories(GLM),
]

# 追加のライブラリ
Compiler = meson.get_compiler('cpp')
Lib = [
        Compiler.find_library('glfw3dll', dirs : GLFW / 'lib-vc2022'),
        Compiler.find_library('vulkan-1', dirs : VK / 'Lib'),
]

executable('Glfw',
           'Main.cpp',
           install : true,
           include_directories : Inc,
           dependencies : Lib)
