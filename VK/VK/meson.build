project('Glfw', 'cpp', version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 
        'default_library=static']
        # 'default_library=shared']
)

IsWin = host_machine.system() == 'windows'

# 追加のインクルード
Inc = [ 
        include_directories(get_option('GLM')),
        include_directories(get_option('GLI')),
]
if IsWin
        Inc += include_directories(get_option('CV') / 'include')
endif

# 追加のライブラリ
Lib = [
        dependency('vulkan'),
]
if IsWin
        Compiler = meson.get_compiler('cpp')
        if get_option('buildtype').startswith('debug')
                Lib += Compiler.find_library('opencv_world4100d', dirs : get_option('CV') / 'x64' / 'vc17' / 'lib')
        else
                Lib += Compiler.find_library('opencv_world4100', dirs : get_option('CV') / 'x64' / 'vc17' / 'lib')
        endif
else
        Lib += dependency('opencv4')
endif

# OS によるライブラリ名の違い
PRE = 'lib'
SUF = 'a' #'so'
if IsWin
       PRE = ''
       SUF = 'lib' # 'dll'
endif

library('VK', 'VK.cpp', install : true, include_directories : Inc, dependencies : Lib, name_prefix : PRE, name_suffix : SUF)