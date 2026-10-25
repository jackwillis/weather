# Mac OS 9 build (PowerPC, Carbon-free classic toolbox) with Retro68. Run inside the Retro68 container; see `make os9`.
#   cmake -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake <this dir>
# No Apple binary is needed: Open Transport's client glue is our own (src/platform/os9/ot_glue.cc).
cmake_minimum_required(VERSION 3.9)
project(weather_os9 C CXX)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
file(GLOB_RECURSE BEARSSL_SRC ${ROOT}/third_party/bearssl/src/*.c)
add_library(bearssl STATIC ${BEARSSL_SRC})
target_include_directories(bearssl PUBLIC ${ROOT}/third_party/bearssl/inc PRIVATE ${ROOT}/third_party/bearssl/src)
target_compile_options(bearssl PRIVATE -Os)

set(WEATHER_VERSION_STR "0.1.0")
set(WEATHER_COMMON
  ${ROOT}/src/core/fetch.cpp
  ${ROOT}/src/tls/tls.cpp
  ${ROOT}/src/platform/os9/net_os9.cpp
  ${ROOT}/src/platform/os9/seed_store_os9.cpp
  ${ROOT}/src/platform/os9/ot_glue.cc
  ${ROOT}/src/platform/os9/log_os9.cpp
  ${ROOT}/src/platform/posix/nocxx.cpp
  ${ROOT}/os9/weather.r)
add_application(Weather ${WEATHER_COMMON} ${ROOT}/src/ui/os9/main.cpp)
add_application(WeatherConsole CONSOLE ${WEATHER_COMMON} ${ROOT}/src/platform/os9/console_main.cpp)
target_include_directories(WeatherConsole PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
target_compile_definitions(WeatherConsole PRIVATE WEATHER_VERSION="${WEATHER_VERSION_STR}")
target_compile_options(WeatherConsole PRIVATE -O2 -fno-exceptions -fno-rtti)
target_link_libraries(WeatherConsole bearssl OpenTransportLib OpenTptInternetLib)
target_include_directories(Weather PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
target_compile_definitions(Weather PRIVATE WEATHER_VERSION="${WEATHER_VERSION_STR}")
target_compile_options(Weather PRIVATE -O2 -fno-exceptions -fno-rtti)
target_link_libraries(Weather bearssl OpenTransportLib OpenTptInternetLib)

add_application(SeedInstall CONSOLE
  ${ROOT}/src/platform/os9/seed_install.cpp
  ${ROOT}/src/platform/os9/seed_store_os9.cpp
  ${ROOT}/src/platform/posix/nocxx.cpp)
target_include_directories(SeedInstall PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
target_compile_options(SeedInstall PRIVATE -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
target_link_libraries(SeedInstall bearssl)

option(WEATHER_BUILD_DIAGNOSTICS "Build self-running OS 9 network checks" OFF)
if(WEATHER_BUILD_DIAGNOSTICS)
  add_application(TlsSeedProbe CONSOLE
    ${ROOT}/spike/os9/tls_seed_probe.cc
    ${ROOT}/src/core/fetch.cpp
    ${ROOT}/src/tls/tls.cpp
    ${ROOT}/src/platform/os9/net_os9.cpp
    ${ROOT}/src/platform/os9/seed_store_os9.cpp
    ${ROOT}/src/platform/os9/ot_glue.cc
    ${ROOT}/src/platform/os9/log_os9.cpp
    ${ROOT}/src/platform/posix/nocxx.cpp
    ${ROOT}/os9/weather.r)
  target_include_directories(TlsSeedProbe PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
  target_compile_definitions(TlsSeedProbe PRIVATE WEATHER_VERSION="${WEATHER_VERSION_STR}")
  target_compile_options(TlsSeedProbe PRIVATE -O2 -fno-exceptions -fno-rtti)
  target_link_libraries(TlsSeedProbe bearssl OpenTransportLib OpenTptInternetLib)
  add_application(SeedStoreProbe CONSOLE
    ${ROOT}/spike/os9/seed_store_probe.cc
    ${ROOT}/src/platform/os9/seed_store_os9.cpp
    ${ROOT}/src/platform/posix/nocxx.cpp
    ${ROOT}/os9/weather.r)
  target_include_directories(SeedStoreProbe PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
  target_compile_definitions(SeedStoreProbe PRIVATE WEATHER_SEED_DIAGNOSTICS=1)
  target_compile_options(SeedStoreProbe PRIVATE -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
  target_link_libraries(SeedStoreProbe bearssl)
  add_application(DnsNetProbe CONSOLE
    ${ROOT}/spike/os9/dns_net_probe.cc
    ${ROOT}/src/platform/os9/net_os9.cpp
    ${ROOT}/src/platform/os9/seed_store_os9.cpp
    ${ROOT}/src/platform/os9/ot_glue.cc
    ${ROOT}/src/platform/os9/log_os9.cpp
    ${ROOT}/src/platform/posix/nocxx.cpp
    ${ROOT}/os9/weather.r)
  target_include_directories(DnsNetProbe PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
  target_compile_options(DnsNetProbe PRIVATE -O2 -Wall -Wextra -fno-exceptions -fno-rtti)
  target_link_libraries(DnsNetProbe bearssl OpenTransportLib OpenTptInternetLib)
endif()
