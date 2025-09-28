# CRU_RDH_Decoder

[![C++ CI](https://github.com/RockPie/CRU_RDH_Decoder/actions/workflows/cpp.yml/badge.svg)](https://github.com/RockPie/CRU_RDH_Decoder/actions/workflows/cpp.yml) [![License](https://img.shields.io/github/license/RockPie/CRU_RDH_Decoder.svg)](LICENSE)

---

## 🧭 Overview

**CRU_RDH_Decoder** is a high-performance binary parser and streaming framework written in modern **C++23**, designed for real-time decoding of **Front-End (FEE)** and **CRU DMA** data from detector readout systems.

It provides:

- ⚙️ A **C++ static library (`binparse`)** for low-level, zero-copy binary parsing  
- 🐍 A **Python module (`pybinparse`)** for analysis and fast prototyping  
- 🔄 Stream parsing for live or incremental data feeds  
- 🧩 Structured decoding of RDH_L0 / RDH_L1 / TRG / Data lines  
- 🧪 Full CI support across Linux, macOS, and Windows  

---

## 📁 Directory Structure

```
CRU_RDH_Decoder/
├── include/binparse/          # Public C++ headers
│   ├── bytecursor.hpp
│   ├── parser.hpp
│   └── ...
├── src/                       # Core C++ source files
│   ├── parser.cpp
│   ├── tail.cpp
│   └── main_tail.cpp
├── python/                    # Pybind11 bindings
│   └── module.cpp
├── data/                      # Example binary files (ignored in .gitignore)
├── cmake/                     # CMake package config templates
├── .github/workflows/         # GitHub Actions CI configs
├── CMakeLists.txt
├── pyproject.toml
└── README.md
```

---

## ⚙️ Build & Install (C++)

### Requirements

- CMake ≥ 3.20  
- Compiler with full **C++23** support  
  - GCC ≥ 13 / Clang ≥ 15 / MSVC ≥ 2022  
- (optional) Ninja build system for faster compilation  

### Build Example

```bash
git clone https://github.com/RockPie/CRU_RDH_Decoder.git
cd CRU_RDH_Decoder

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build --prefix /usr/local
```

Use in your own project:

```cpp
#include <binparse/parser.hpp>

int main() {
    bp::StreamParser parser;
    // ...
}
```

CMake integration:

```cmake
find_package(binparse CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE binparse::binparse)
```

---

## 🐍 Python Module: `pybinparse`

### Installation

Build from source:

```bash
pip install scikit-build-core pybind11 ninja build
python -m build
```

The generated `.whl` and `.tar.gz` will appear in the `dist/` folder.

---

### Example Usage

```python
import pybinparse as m
import mmap, os

path = "data/2025_09_25_cru_dma_log_example"
with open(path, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as mm:
    stats = m.count_types_v2(mm)
    print(stats)
```

Parsing RDH lines:

```python
line = bytes(40)
typ, info = m.parse_rdh_line(line)
print(f"Type: {typ}")
for k, v in info.items():
    print(f"  {k:<16}: {v}")
```

---

## 🧱 Developer Quick Start

### macOS

```bash
brew install cmake ninja pybind11
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Windows

```powershell
choco install cmake ninja llvm
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Linux

```bash
sudo apt install cmake ninja-build g++-13
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

---

## 🧪 Continuous Integration

GitHub Actions automatically build and test the project across:

- ✅ **Linux** (GCC 13 + Ninja)
- ✅ **macOS** (AppleClang)
- ✅ **Windows** (MSVC 2022)

Each CI run verifies:

1. Successful C++ build and installation  
2. CMake `find_package(binparse)` usability  
3. Python wheel build and import test  

See build results in the [Actions tab](https://github.com/RockPie/CRU_RDH_Decoder/actions).

---

## 🧾 License

This project is released under the [MIT License](LICENSE).

---

## 🙌 Acknowledgements

This readme file is generated using [ChatGPT](https://chat.openai.com/) based on the project structure and content.