project(
    'nogo',
    'cpp',
    version         : '0.1.0',
    default_options : [
        'cpp_std=c++20',
        'warning_level=2',
        'buildtype=debugoptimized',
    ],
)

sources = files(
    'src/lexer.cpp',
    'src/tokenizer.cpp',
    'src/ast.cpp',
    'src/parser.cpp',
    'src/compiler.cpp',
    'src/vm.cpp',
    'src/main.cpp',
)

executable(
    'nogo',
    sources,
    include_directories : include_directories('src'),
    install             : true,
)
