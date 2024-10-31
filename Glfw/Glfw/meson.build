project('Glfw', 'cpp', version : '0.1', default_options : ['warning_level=3', 'cpp_std=c++20'])

VK = '..' / '..' / 'VK' / 'VK'

# OS を調べる
IsWin = host_machine.system() == 'windows'

# 追加のインクルード
Inc = [
        include_directories(get_option('GLM')),
        include_directories(get_option('GLI')),
        include_directories(VK),
]
if IsWin
        Inc += include_directories(get_option('GLFW') / 'include')
        Inc += include_directories(get_option('CV') / 'include')
endif

# 追加のライブラリ
Compiler = meson.get_compiler('cpp')
Lib = [
        dependency('vulkan'),
        Compiler.find_library('VK', dirs : meson.current_source_dir() / VK / 'build')
]
if IsWin
        Lib += Compiler.find_library('glfw3dll', dirs : get_option('GLFW') / 'lib-vc2022')
        if get_option('buildtype').startswith('debug')
                Lib += Compiler.find_library('opencv_world4100d', dirs : get_option('CV') / 'x64' / 'vc17' / 'lib')
        else
                Lib += Compiler.find_library('opencv_world4100', dirs : get_option('CV') / 'x64' / 'vc17' / 'lib')
        endif
else
        Lib += dependency('glfw3')
        Lib += dependency('opencv4')
endif

executable('Glfw', 'Main.cpp', install : true, include_directories : Inc, dependencies : Lib)

# GLSL コンパイル
foreach i : [ 'Glfw.vert', 'Glfw.frag' ]
        run_command('glslangValidator', '-V', i, '-o', 'build' / i + '.spv', '--target-env', 'vulkan1.3', '-g', '-Od', check: true)
endforeach
