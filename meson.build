project(
    'math_expr',
    'cpp',
    version         : '1.0.0',
    default_options : [
        'cpp_std=c++17',
        'warning_level=3',
        'buildtype=debugoptimized',
    ],
)

sources = files(
    'src/lexer.cpp',
    'src/parser.cpp',
    'src/evaluator.cpp',
    'src/main.cpp',
)

executable(
    'math_expr',
    sources,
    include_directories : include_directories('src'),
    install             : true,
)
