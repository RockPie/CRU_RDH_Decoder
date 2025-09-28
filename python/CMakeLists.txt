cmake_minimum_required(VERSION 3.20)

# 解决 CMP0148 提示：让 pybind11 使用新的 FindPython
set(PYBIND11_FINDPYTHON ON)
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

find_package(pybind11 CONFIG REQUIRED)

# 源码在“当前目录”的 module.cpp
pybind11_add_module(pybinparse MODULE
    ${CMAKE_CURRENT_SOURCE_DIR}/module.cpp
)

# 链接你的 C++ 库与头文件
target_link_libraries(pybinparse PRIVATE binparse)
target_include_directories(pybinparse PRIVATE ${CMAKE_SOURCE_DIR}/include)

# 安装到 wheel 根目录（scikit-build 会处理路径）
install(TARGETS pybinparse LIBRARY DESTINATION .)