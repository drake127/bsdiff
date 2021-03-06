cmake_minimum_required(VERSION 3.13)
project (
    bsdiff
    LANGUAGES C)

# Sets global options.
set(CMAKE_C_STANDARD 99)
if (MSVC)
  string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  add_compile_options("/wd4100")
  add_compile_definitions(
    "_CRT_DECLARE_NONSTDC_NAMES=0"
    "_CRT_SECURE_NO_WARNINGS"
    "_CRT_NONSTDC_NO_WARNINGS"
    "stat=_stat64"
    "fstat=_fstat64"
    "fileno=_fileno"
    "ATTR_UNUSED="
    "ATTR_NORETURN=__declspec(noreturn)")
else()
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og")
  add_compile_options("-Wall" "-Wpedantic" "-Wextra" "-Werror")
  add_compile_definitions(
    "_FILE_OFFSET_BITS=64"
    "ATTR_UNUSED=__attribute__((unused))"
    "ATTR_NORETURN=__attribute__((noreturn))")
endif()

# Includes bzip2 library.
find_package(BZip2)

# Builds bsdiff library.
add_library(static_bsdiff bsdiff.c bsdiff.h bspatch.c bspatch.h)
set_target_properties(static_bsdiff PROPERTIES OUTPUT_NAME bsdiff)

if (BZIP2_FOUND)
  # Builds bsdiff.
  add_executable(bsdiff bsdiff.c bsdiff.h bsdiff_common.h)
  target_compile_definitions(bsdiff PRIVATE "BSDIFF_EXECUTABLE")
  target_include_directories(bsdiff PRIVATE ${BZIP2_INCLUDE_DIR})
  target_link_libraries(bsdiff ${BZIP2_LIBRARIES})

  #Builds bspatch.
  add_executable(bspatch bspatch.c bspatch.h bsdiff_common.h)
  target_compile_definitions(bspatch PRIVATE "BSPATCH_EXECUTABLE")
  target_include_directories(bspatch PRIVATE ${BZIP2_INCLUDE_DIR})
  target_link_libraries(bspatch ${BZIP2_LIBRARIES})
endif()
