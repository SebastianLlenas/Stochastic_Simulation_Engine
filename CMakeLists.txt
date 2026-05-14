cmake_minimum_required(VERSION 3.24)

project(omega9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  nanobind
  GIT_REPOSITORY https://github.com/wjakob/nanobind.git
  GIT_TAG v2.10.0
)

FetchContent_MakeAvailable(nanobind)

set(OMEGA9_SOURCES
  src/data/database.cpp
  src/engine/action.cpp
  src/engine/damage.cpp
  src/engine/mutator.cpp
  src/engine/switch.cpp
  src/engine/step.cpp
  src/engine/batch.cpp
  src/engine/encoder.cpp
  src/python/bindings.cpp
)

nanobind_add_module(omega9 ${OMEGA9_SOURCES})

target_include_directories(omega9 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (MSVC)
  target_compile_options(omega9 PRIVATE /O2 /openmp)
else()
  target_compile_options(omega9 PRIVATE -O3 -march=native -fopenmp)
  target_link_options(omega9 PRIVATE -fopenmp)
endif()
