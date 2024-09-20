project('Glfw', 'cpp', version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 
        'default_library=static']
        # 'default_library=shared']
)

# 追加のインクルード
Inc = [ 
        include_directories(get_option('GLM')),
        include_directories(get_option('GLI')),
]

# 追加のライブラリ
Lib = [
        dependency('vulkan'),
]

# OS によるライブラリ名の違い
PRE = 'lib'
SUF = 'a' #'so'
if host_machine.system() == 'windows'
       PRE = ''
       SUF = 'lib' # 'dll'
endif

library('VK', 'VK.cpp', install : true, include_directories : Inc, dependencies : Lib, name_prefix : PRE, name_suffix : SUF)