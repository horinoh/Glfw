project('Glfw', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

# get_option() で取得する値は、setup の引数として指定する
GLFW = get_option('GLFW')
GLM = get_option('GLM')
VK = '..' / '..' / 'VK' / 'VK'

# OS によるライブラリ名の違い
LIBGLFW = 'glfw3.so'
LIBGLFW_PATH = GLFW
# libVK.a or VK.lib
LIBVK = 'libVK'
if host_machine.system() == 'windows'
        LIBGLFW = 'glfw3dll'
        LIBGLFW_PATH = GLFW / 'lib-vc2022'
        LIBVK = 'VK'
endif

# 追加のインクルード
Inc = [ 
        include_directories(GLFW / 'include'),
        include_directories(GLM),
        include_directories(VK),
]

# 追加のライブラリ
Compiler = meson.get_compiler('cpp')
Lib = [
        dependency('vulkan'),
        Compiler.find_library(LIBGLFW, dirs : LIBGLFW_PATH),
        Compiler.find_library(LIBVK, dirs : meson.current_source_dir() / VK / 'build'),
]

executable('Glfw',
           'Main.cpp',
           install : true,
           include_directories : Inc,
           dependencies : Lib)

# GLSL コンパイル
foreach i : [ 'Glfw.vert', 'Glfw.frag' ]
        run_command('glslangValidator', '-V', i, '-o', 'build' / i + '.spv', '--target-env', 'vulkan1.3', '-g', '-Od')
endforeach
