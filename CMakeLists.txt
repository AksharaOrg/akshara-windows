cmake_minimum_required(VERSION 3.25)
set(AKSHARA_VERSION "0.1.0" CACHE STRING "Akshara semantic version")
project(AksharaWindows VERSION ${AKSHARA_VERSION} LANGUAGES CXX)

set(AKSHARA_VERSION_STRING "${PROJECT_VERSION}")
add_compile_definitions(
  AKSHARA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  AKSHARA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  AKSHARA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  AKSHARA_VERSION_STRING="${AKSHARA_VERSION_STRING}"
  AKSHARA_VERSION_STRING_W=L"${AKSHARA_VERSION_STRING}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

option(AKSHARA_BUILD_TESTS "Build portable core tests" ON)
option(AKSHARA_BUILD_WINDOWS_COMPONENTS "Build TSF and Help executables" ${WIN32})

add_library(akshara_core STATIC
  src/core/AksharaEngine.cpp
  src/core/MappingTrie.cpp
  src/core/SinhalaUnicode.cpp)
target_include_directories(akshara_core PUBLIC src/core)
if(MSVC)
  target_compile_options(akshara_core PRIVATE /W4 /WX /permissive- /sdl /guard:cf)
else()
  target_compile_options(akshara_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(AKSHARA_BUILD_TESTS)
  enable_testing()
  add_executable(akshara_core_tests tests/core/AksharaEngineTests.cpp)
  target_link_libraries(akshara_core_tests PRIVATE akshara_core)
  target_compile_definitions(akshara_core_tests PRIVATE AKSHARA_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  add_test(NAME akshara-core COMMAND akshara_core_tests)
endif()

if(WIN32 AND AKSHARA_BUILD_WINDOWS_COMPONENTS)
  add_library(AksharaIME SHARED
    src/tsf/dllmain.cpp src/tsf/Module.cpp src/tsf/TextService.cpp
    src/tsf/EditSession.cpp src/tsf/Registration.cpp src/tsf/AksharaIME.rc src/tsf/AksharaIME.def)
  target_link_libraries(AksharaIME PRIVATE akshara_core ole32 oleaut32 advapi32 uuid)
  target_compile_definitions(AksharaIME PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(AksharaIME PRIVATE /W4 /WX /permissive- /sdl /guard:cf)
  target_link_options(AksharaIME PRIVATE /DYNAMICBASE /NXCOMPAT /GUARD:CF)

  add_executable(AksharaHelp WIN32 src/help/main.cpp src/help/AksharaHelp.rc)
  target_compile_definitions(AksharaHelp PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(AksharaHelp PRIVATE comctl32 shell32)

  add_executable(AksharaRegister src/setup-helper/main.cpp)
  target_compile_definitions(AksharaRegister PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(AksharaRegister PRIVATE /W4 /WX /permissive- /sdl /guard:cf)
  target_link_options(AksharaRegister PRIVATE /DYNAMICBASE /NXCOMPAT /GUARD:CF)
  target_link_libraries(AksharaRegister PRIVATE ole32 msctf)
endif()
