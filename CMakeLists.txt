cmake_minimum_required(VERSION 3.20)
project(binparse VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- 库本体
add_library(binparse STATIC
  src/parser.cpp
  src/tail.cpp
)
target_include_directories(binparse
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>  # 安装后使用
)
target_compile_features(binparse PUBLIC cxx_std_23)
# 建议默认隐藏符号，公开 API 用头文件内联或显式可见性（可后续加）
if(NOT MSVC)
  target_compile_options(binparse PRIVATE -fvisibility=hidden -Wall -Wextra -Wpedantic)
endif()
add_library(binparse::binparse ALIAS binparse)

# ---- 可选 CLI 示例（不安装也行）
add_executable(bpx_tail src/main_tail.cpp)
target_link_libraries(bpx_tail PRIVATE binparse)

# ---- 安装头文件、库与 CMake 包描述
include(GNUInstallDirs)
install(TARGETS binparse
  EXPORT binparseTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 导出 targets 文件
install(EXPORT binparseTargets
  NAMESPACE binparse::
  FILE binparseTargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binparse
)

# 生成 config 文件（让 find_package 找到）
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

# ---- 可选：安装 CLI
install(TARGETS bpx_tail RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---- 选项：构建 Python 扩展（本地开发时）
option(BUILD_PYTHON "Build pybind11 extension in-tree" OFF)
if(BUILD_PYTHON)
  add_subdirectory(python)
endif()