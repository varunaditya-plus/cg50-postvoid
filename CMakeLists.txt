cmake_minimum_required(VERSION 3.15)
project(postvoid-cg50 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "$ENV{HOME}/.local/lib/cmake/fxsdk")

include(GenerateG3A)
find_package(Gint 2.9 REQUIRED)

add_executable(postvoid src/main.c src/map.c)
target_compile_options(postvoid PRIVATE -Wall -Wextra -Os)
target_link_libraries(postvoid Gint::Gint m)

generate_g3a(
  TARGET postvoid
  OUTPUT "${CMAKE_SOURCE_DIR}/PostVoid.g3a"
  NAME "Post Void"
  ICONS
    "${CMAKE_SOURCE_DIR}/assets-cg/icon-uns.png"
    "${CMAKE_SOURCE_DIR}/assets-cg/icon-sel.png"
)

