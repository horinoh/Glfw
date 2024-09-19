project('Glfw', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

# get_option() で取得する値は、setup の引数として指定する
GLM = get_option('GLM')
GLI = get_option('GLI')
VK = '..' / '..' / 'VK' / 'VK'

# OS を調べる
IsWin = false
if host_machine.system() == 'windows'
   IsWin = true
endif

# 追加のインクルード
Inc = [
        include_directories(GLM),
        include_directories(GLI),
        include_directories(VK),
]
if IsWin
        GLFW = get_option('GLFW')
        Inc += include_directories(GLFW / 'include')
endif

# 追加のライブラリ
Compiler = meson.get_compiler('cpp')
Lib = [
        dependency('vulkan'),
        #Compiler.find_library(LIBVK, dirs : meson.current_source_dir() / VK / 'build'),
]
if IsWin
        Lib += Compiler.find_library('glfw3dll', dirs : GLFW / 'lib-vc2022')
        Lib += Compiler.find_library('VK', dirs : meson.current_source_dir() / VK / 'build')
else
        Lib += dependency('glfw3')
        Lib += Compiler.find_library('VK', dirs : meson.current_source_dir() / VK / 'build')
endif

executable('Glfw',
           'Main.cpp',
           install : true,
           include_directories : Inc,
           dependencies : Lib)

# GLSL コンパイル
foreach i : [ 'Glfw.vert', 'Glfw.frag' ]
        run_command('glslangValidator', '-V', i, '-o', 'build' / i + '.spv', '--target-env', 'vulkan1.3', '-g', '-Od', check: true)
endforeach
