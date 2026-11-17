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
  src/sinks/headless_sink.cpp
  src/sinks/output_report.cpp
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

# The scroll test program. DeckLink output only when built with the SDK.
add_executable(ought-scrolltest src/apps/scrolltest_main.cpp)
target_link_libraries(ought-scrolltest PRIVATE otc_core)
target_compile_options(ought-scrolltest PRIVATE ${OTC_WARNINGS})

# Smoke tests of the command line (exit codes: 0 ok, 2 bad usage or input).
add_test(NAME scrolltest_headless_runs
  COMMAND ought-scrolltest --sink headless --pattern bars --speed 100 --frames 20)
add_test(NAME scrolltest_rejects_unknown_mode
  COMMAND sh -c "$<TARGET_FILE:ought-scrolltest> --sink headless --mode 720p --frames 1; test $? -eq 2")
add_test(NAME scrolltest_rejects_unknown_option
  COMMAND sh -c "$<TARGET_FILE:ought-scrolltest> --sink headless --bogus; test $? -eq 2")
add_test(NAME scrolltest_rejects_narrow_strip
  COMMAND sh -c "printf 'P5\\n10 1\\n255\\n0123456789' > narrow.pgm && $<TARGET_FILE:ought-scrolltest> --sink headless --strip narrow.pgm --frames 1; test $? -eq 2"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME scrolltest_rejects_missing_strip
  COMMAND sh -c "$<TARGET_FILE:ought-scrolltest> --sink headless --strip /nonexistent.pgm --frames 1; test $? -eq 2")

# DeckLink output (only with the SDK headers). The SDK's own dispatch file is compiled without -Werror.
if(OTC_DECKLINK_SDK)
  if(NOT EXISTS "${OTC_DECKLINK_SDK}/DeckLinkAPI.h" OR NOT EXISTS "${OTC_DECKLINK_SDK}/DeckLinkAPIDispatch.cpp")
    message(FATAL_ERROR "OTC_DECKLINK_SDK=${OTC_DECKLINK_SDK} does not contain DeckLinkAPI.h and DeckLinkAPIDispatch.cpp")
  endif()
  add_library(otc_decklink_dispatch OBJECT "${OTC_DECKLINK_SDK}/DeckLinkAPIDispatch.cpp")
  target_include_directories(otc_decklink_dispatch PRIVATE "${OTC_DECKLINK_SDK}")
  target_compile_options(otc_decklink_dispatch PRIVATE -Wno-multichar -fno-rtti -w)

  add_library(otc_decklink
    src/sinks/decklink/decklink_sink.cpp
    src/apps/scrolltest_decklink.cpp
    $<TARGET_OBJECTS:otc_decklink_dispatch>)
  target_include_directories(otc_decklink PUBLIC src "${OTC_DECKLINK_SDK}")
  target_compile_options(otc_decklink PRIVATE ${OTC_WARNINGS} -Wno-multichar -fno-rtti)
  target_link_libraries(otc_decklink PUBLIC otc_core ${CMAKE_DL_LIBS} Threads::Threads)

  target_link_libraries(ought-scrolltest PRIVATE otc_decklink)
  target_compile_definitions(ought-scrolltest PRIVATE OTC_HAVE_DECKLINK)
  target_compile_options(ought-scrolltest PRIVATE -Wno-multichar)
endif()

# The text-strip tool (skipped where ImageMagick isn't installed).
add_test(NAME make_text_strip_handles_crlf
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_make_text_strip.sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_text_strip.sh)
set_tests_properties(make_text_strip_handles_crlf PROPERTIES SKIP_RETURN_CODE 77)
