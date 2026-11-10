cmake_minimum_required(VERSION 3.16)
project(RetroForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sans option, on compile en Release (beaucoup plus rapide pour un émulateur)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(retroforge src/main.cpp)
target_compile_options(retroforge PRIVATE -Wall -Wextra)

if(EMSCRIPTEN)
  target_compile_options(retroforge PRIVATE "-sUSE_SDL=2")
  target_link_options(retroforge PRIVATE
    "-sUSE_SDL=2"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFORCE_FILESYSTEM=1"
    "-sNO_EXIT_RUNTIME=1"
    "-sEXPORTED_RUNTIME_METHODS=FS,callMain,cwrap"
    "-lidbfs.js"
    "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
  )
  set_target_properties(retroforge PROPERTIES SUFFIX ".html")
else()
  find_package(SDL2 REQUIRED)
  if(TARGET SDL2::SDL2)
    target_link_libraries(retroforge PRIVATE SDL2::SDL2)
  else()
    target_include_directories(retroforge PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(retroforge PRIVATE ${SDL2_LIBRARIES})
  endif()
endif()

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
  add_executable(retroforge_core_tests tests/core_tests.cpp)
  target_include_directories(retroforge_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_options(retroforge_core_tests PRIVATE -Wall -Wextra)
  add_test(NAME retroforge_core_tests COMMAND retroforge_core_tests)
endif()