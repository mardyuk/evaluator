project(
    'nogo',
    'cpp',
    version         : '0.1.0',
    default_options : [
        'cpp_std=c++17',
        'warning_level=2',
        'buildtype=debugoptimized',
    ],
)

sources = files(
    'src/lexer.cpp',
    'src/main.cpp',
)

executable(
    'nogo',
    sources,
    include_directories : include_directories('src'),
    install             : true,
)
