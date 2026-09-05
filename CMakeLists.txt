cmake_minimum_required(VERSION 3.20)
project(pcalc_clang_constexpr LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang QUIET CONFIG)

add_library(pcalc_clang_constexpr STATIC
  src/evaluator.cpp
)

target_include_directories(pcalc_clang_constexpr
  PUBLIC include
  PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS}
)

target_compile_definitions(pcalc_clang_constexpr PRIVATE ${LLVM_DEFINITIONS})
if(TARGET clangFrontend)
  target_link_libraries(pcalc_clang_constexpr PUBLIC
    clangFrontend
    clangParse
    clangSema
    clangAST
    clangLex
    clangBasic
  )
else()
  find_library(CLANG_CPP_LIBRARY
    NAMES clang-cpp libclang-cpp
    HINTS ${LLVM_LIBRARY_DIRS} /usr/lib64
    REQUIRED)
  target_link_libraries(pcalc_clang_constexpr PUBLIC ${CLANG_CPP_LIBRARY} LLVM)
endif()

add_executable(pcalc-constexpr tools/pcalc_constexpr.cpp)
target_link_libraries(pcalc-constexpr PRIVATE pcalc_clang_constexpr)

enable_testing()
add_executable(pcalc-constexpr-test tests/evaluator_test.cpp)
target_link_libraries(pcalc-constexpr-test PRIVATE pcalc_clang_constexpr)
add_test(NAME pcalc-constexpr-test COMMAND pcalc-constexpr-test)

if(EMSCRIPTEN)
  add_executable(pcalc-constexpr-wasm wasm/module.cpp)
  target_link_libraries(pcalc-constexpr-wasm PRIVATE pcalc_clang_constexpr)
  target_compile_options(pcalc-constexpr-wasm PRIVATE -Oz -flto)
  target_link_options(pcalc-constexpr-wasm PRIVATE
    -Oz
    -flto
    --no-entry
    "SHELL:-sEXPORTED_FUNCTIONS=['_pcalc_constexpr_evaluate','_pcalc_constexpr_evaluate_language','_pcalc_constexpr_result_size','_malloc','_free']"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','HEAPU8']"
    -sALLOW_MEMORY_GROWTH=1
    -sFILESYSTEM=0
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sENVIRONMENT=web,worker,node
  )
  set_target_properties(pcalc-constexpr-wasm PROPERTIES SUFFIX ".js")
endif()
