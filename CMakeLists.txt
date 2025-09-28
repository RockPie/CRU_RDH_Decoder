cmake_minimum_required(VERSION 3.20)
project(binparse VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- 库本体（不包含 tail.cpp，避免 Windows 上的 unistd.h 问题）
add_library(binparse STATIC
  src/parser.cpp
)
target_include_directories(binparse
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(binparse PUBLIC cxx_std_23)
if(MSVC)
  target_compile_options(binparse PRIVATE /W4)
else()
  target_compile_options(binparse PRIVATE -fvisibility=hidden -Wall -Wextra -Wpedantic)
endif()
add_library(binparse::binparse ALIAS binparse)

# ---- 可选 CLI（Windows 默认关闭）
option(BUILD_TOOLS "Build CLI tools (bpx_tail)" ON)
if(WIN32)
  set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)  # Windows 先关闭，避免 unistd.h
endif()

if(BUILD_TOOLS)
  add_executable(bpx_tail
    src/main_tail.cpp
    src/tail.cpp        # 仅在 CLI 中编译
  )
  target_link_libraries(bpx_tail PRIVATE binparse)
endif()

# ---- 安装库与头文件
include(GNUInstallDirs)
install(TARGETS binparse
  EXPORT binparseTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---- 安装 CMake 包（供 find_package 使用）
install(EXPORT binparseTargets
  NAMESPACE binparse::
  FILE binparseTargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binparse
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  cmake/binparse-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/binparse-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binparse
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/binparse-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/binparse-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/binparse-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binparse
)

# ---- 可选：安装 CLI（仅当构建时）
if(BUILD_TOOLS)
  install(TARGETS bpx_tail RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# ---- 可选：构建 Python 扩展（给 scikit-build 或本地测试）
option(BUILD_PYTHON "Build pybind11 extension in-tree" OFF)
if(BUILD_PYTHON)
  add_subdirectory(python)
endif()