cmake_minimum_required(VERSION 3.24)
project(ought_to_cue_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Path to the DeckLink SDK headers (the folder holding DeckLinkAPI.h and DeckLinkAPIDispatch.cpp).
# Empty: build without the DeckLink sink (CI, machines without the SDK). The SDK is never committed.
set(OTC_DECKLINK_SDK "" CACHE PATH "DeckLink SDK include folder")

set(OTC_WARNINGS -Wall -Wextra -Werror)

add_library(otc_core
  src/core/video_mode.cpp
  src/core/motion.cpp
  src/core/strip.cpp
  src/core/pgm.cpp
  src/core/compositor.cpp
  src/core/scroll_source.cpp
)
target_include_directories(otc_core PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(otc_core PUBLIC Threads::Threads)
target_compile_options(otc_core PRIVATE ${OTC_WARNINGS})

enable_testing()
file(GLOB OTC_TESTS CONFIGURE_DEPENDS tests/test_*.cpp)
foreach(test_src ${OTC_TESTS})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE otc_core)
  target_include_directories(${test_name} PRIVATE tests)
  target_compile_options(${test_name} PRIVATE ${OTC_WARNINGS})
  add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endforeach()
