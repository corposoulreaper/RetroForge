cmake_minimum_required(VERSION 3.16)
project(RetroForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sans option, on compile en Release (beaucoup plus rapide pour un émulateur)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

add_executable(retroforge src/main.cpp)
target_compile_options(retroforge PRIVATE -Wall -Wextra)

if(TARGET SDL2::SDL2)
  target_link_libraries(retroforge PRIVATE SDL2::SDL2)
else()
  target_include_directories(retroforge PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(retroforge PRIVATE ${SDL2_LIBRARIES})
endif()