project('Glfw', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

# setup の引数として指定する
GLFW = get_option('GLFW')
GLM = get_option('GLM')

# 追加のインクルード
Inc = [ 
        include_directories(GLFW / 'include'),
        include_directories(GLM),
]

# 追加のライブラリ
Compiler = meson.get_compiler('cpp')
Lib = [
        dependency('vulkan'),
        Compiler.find_library('glfw3dll', dirs : GLFW / 'lib-vc2022'),
]

executable('Glfw',
           'Main.cpp',
           install : true,
           include_directories : Inc,
           dependencies : Lib)

# GLSL 
foreach i : [ 'Glfw.vert', 'Glfw.frag' ]
        run_command('glslangValidator', '-V', i, '-o', 'build' / i + '.spv', '--target-env', 'vulkan1.3', '-g', '-Od')
endforeach
