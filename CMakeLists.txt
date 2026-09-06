cmake_minimum_required(VERSION 3.20)
project(pcalc_clang_constexpr LANGUAGES C CXX)

include(GNUInstallDirs)

option(PCALC_CONSTEXPR_BUILD_SHARED "Build the C ABI as a shared library" ON)
option(PCALC_CONSTEXPR_ENABLE_IPO "Enable interprocedural optimization" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang QUIET CONFIG)

if(PCALC_CONSTEXPR_BUILD_SHARED)
  set(PCALC_CONSTEXPR_LIBRARY_TYPE SHARED)
else()
  set(PCALC_CONSTEXPR_LIBRARY_TYPE STATIC)
endif()

add_library(pcalc_clang_constexpr ${PCALC_CONSTEXPR_LIBRARY_TYPE}
  src/evaluator.cpp
)
set_target_properties(pcalc_clang_constexpr PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)
if(PCALC_CONSTEXPR_ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT PCALC_CONSTEXPR_IPO_SUPPORTED OUTPUT PCALC_CONSTEXPR_IPO_ERROR)
  if(NOT PCALC_CONSTEXPR_IPO_SUPPORTED)
    message(FATAL_ERROR "IPO is unavailable: ${PCALC_CONSTEXPR_IPO_ERROR}")
  endif()
  set_property(TARGET pcalc_clang_constexpr PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_include_directories(pcalc_clang_constexpr
  PUBLIC include
  PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS}
)

target_compile_definitions(pcalc_clang_constexpr PRIVATE ${LLVM_DEFINITIONS})
target_compile_definitions(pcalc_clang_constexpr PRIVATE PCALC_CONSTEXPR_BUILDING_LIBRARY)
if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(pcalc_clang_constexpr PRIVATE -fno-rtti)
endif()
if(NOT LLVM_ENABLE_EH)
  target_compile_options(pcalc_clang_constexpr PRIVATE -fno-exceptions)
endif()
if(TARGET clangFrontend)
  target_link_libraries(pcalc_clang_constexpr PUBLIC
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

install(TARGETS pcalc_clang_constexpr
  EXPORT pcalcClangConstexprTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES include/pcalc_constexpr/evaluator.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pcalc_constexpr
)

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
