project('Glfw', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 'default_library=static']
  #default_options : ['warning_level=3', 'cpp_std=c++20', 'default_library=shared']
  )

# setup の引数として指定する
GLM = get_option('GLM')
GLI = get_option('GLI')

# 追加のインクルード
Inc = [ 
        include_directories(GLM),
        include_directories(GLI),
]

# 追加のライブラリ
Compiler = meson.get_compiler('cpp')
Lib = [
        dependency('vulkan'),
]

# OS によるライブラリ名の違い
LIBPRE = 'lib'
LIBSUF = 'a'
#LIBSUF = 'so'
if host_machine.system() == 'windows'
       LIBPRE = ''
       LIBSUF = 'lib'
       #LIBSUF = 'dll'
endif

library('VK',
           'VK.cpp',
           install : true,
           include_directories : Inc,
           dependencies : Lib,
           name_prefix : LIBPRE, name_suffix : LIBSUF
           )