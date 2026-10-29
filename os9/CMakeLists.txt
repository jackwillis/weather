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
  ${ROOT}/src/platform/os9/cache_os9.cpp
  ${ROOT}/src/app/preferences.cpp
  ${ROOT}/src/core/fetch.cpp
  ${ROOT}/src/platform/os9/config_os9.cpp
  ${ROOT}/src/tls/tls.cpp
  ${ROOT}/src/platform/os9/net_os9.cpp
  ${ROOT}/src/platform/os9/seed_store_os9.cpp
  ${ROOT}/src/platform/os9/ot_glue.cc
  ${ROOT}/src/platform/os9/log_os9.cpp
  ${ROOT}/src/platform/posix/nocxx.cpp)
# Every window target needs place-search and core-event support, including diagnostics.
set(WEATHER_WINDOW_COMMON ${WEATHER_COMMON} ${ROOT}/os9/window.r
  ${ROOT}/src/ui/os9/system_events.cpp ${ROOT}/src/ui/os9/place_search.cpp)
add_application(Weather ${WEATHER_WINDOW_COMMON} ${ROOT}/src/ui/os9/window.cpp
  ${ROOT}/src/ui/os9/location.cpp
  ${ROOT}/src/platform/os9/settings_os9.cpp ${ROOT}/src/ui/os9/main.cpp)
add_application(WeatherConsole CONSOLE ${WEATHER_COMMON} ${ROOT}/os9/weather.r ${ROOT}/src/platform/os9/console_main.cpp
  ${ROOT}/src/platform/os9/settings_os9.cpp)
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
  add_application(EntropyProbe
    ${ROOT}/test/os9/entropy_probe.cc ${ROOT}/test/os9/entropy_probe.r
    ${ROOT}/src/platform/posix/nocxx.cpp)
  target_include_directories(EntropyProbe PRIVATE ${ROOT}/src)
  target_compile_options(EntropyProbe PRIVATE -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti)

  add_application(CacheProbe CONSOLE
    ${ROOT}/src/platform/os9/cache_os9.cpp
    ${ROOT}/spike/os9/cache_probe.cc
    ${ROOT}/src/platform/posix/nocxx.cpp)
  target_include_directories(CacheProbe PRIVATE ${ROOT}/src)
  target_compile_options(CacheProbe PRIVATE -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
  add_application(PreferencesProbe CONSOLE
    ${ROOT}/spike/os9/preferences_probe.cc
    ${ROOT}/src/platform/os9/settings_os9.cpp
    ${ROOT}/src/platform/posix/nocxx.cpp)
  target_include_directories(PreferencesProbe PRIVATE ${ROOT}/src)
  target_compile_options(PreferencesProbe PRIVATE -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
  add_application(WeatherPrefsUITest CONSOLE ${WEATHER_WINDOW_COMMON}
    ${ROOT}/src/ui/os9/window.cpp ${ROOT}/src/ui/os9/location.cpp
    ${ROOT}/src/platform/os9/settings_os9.cpp ${ROOT}/spike/os9/preferences_ui_probe.cc)
  target_include_directories(WeatherPrefsUITest PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
  target_compile_definitions(WeatherPrefsUITest PRIVATE WEATHER_VERSION="${WEATHER_VERSION_STR}")
  target_compile_options(WeatherPrefsUITest PRIVATE -O2 -Wall -Wextra -fno-exceptions -fno-rtti)
  target_link_libraries(WeatherPrefsUITest bearssl OpenTransportLib OpenTptInternetLib)
  foreach(FIRST_TARGET WeatherFirstApplyTest WeatherFirstCancelTest)
    add_application(${FIRST_TARGET} CONSOLE ${WEATHER_WINDOW_COMMON}
      ${ROOT}/src/ui/os9/window.cpp ${ROOT}/src/ui/os9/location.cpp
      ${ROOT}/src/platform/os9/settings_os9.cpp ${ROOT}/spike/os9/first_location_probe.cc)
    target_include_directories(${FIRST_TARGET} PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
    target_compile_definitions(${FIRST_TARGET} PRIVATE WEATHER_VERSION="${WEATHER_VERSION_STR}")
    target_compile_options(${FIRST_TARGET} PRIVATE -O2 -Wall -Wextra -fno-exceptions -fno-rtti)
    target_link_libraries(${FIRST_TARGET} bearssl OpenTransportLib OpenTptInternetLib)
  endforeach()
  target_compile_definitions(WeatherFirstCancelTest PRIVATE WEATHER_FIRST_CANCEL=1)
  add_application(WeatherUITest CONSOLE ${WEATHER_WINDOW_COMMON}
    ${ROOT}/src/ui/os9/window.cpp
  ${ROOT}/src/ui/os9/location.cpp
  ${ROOT}/src/platform/os9/settings_os9.cpp ${ROOT}/spike/os9/window_probe.cc)
  target_include_directories(WeatherUITest PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
  target_compile_definitions(WeatherUITest PRIVATE WEATHER_VERSION="${WEATHER_VERSION_STR}")
  target_compile_options(WeatherUITest PRIVATE -O2 -Wall -Wextra -fno-exceptions -fno-rtti)
  target_link_libraries(WeatherUITest bearssl OpenTransportLib OpenTptInternetLib)
  add_custom_command(TARGET WeatherUITest POST_BUILD
    COMMAND sh ${ROOT}/test/os9-event-layout.sh ${CMAKE_NM} $<TARGET_FILE:WeatherUITest>
    VERBATIM)
  add_application(WeatherPlaceSearchTest CONSOLE ${WEATHER_WINDOW_COMMON}
    ${ROOT}/src/ui/os9/window.cpp ${ROOT}/src/ui/os9/location.cpp
    ${ROOT}/src/platform/os9/settings_os9.cpp ${ROOT}/spike/os9/place_search_probe.cc)
  target_include_directories(WeatherPlaceSearchTest PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
  target_compile_definitions(WeatherPlaceSearchTest PRIVATE WEATHER_VERSION="${WEATHER_VERSION_STR}")
  target_compile_options(WeatherPlaceSearchTest PRIVATE -O2 -Wall -Wextra -fno-exceptions -fno-rtti)
  target_link_libraries(WeatherPlaceSearchTest bearssl OpenTransportLib OpenTptInternetLib)
  add_application(WeatherCacheUITest CONSOLE ${WEATHER_WINDOW_COMMON}
    ${ROOT}/src/ui/os9/window.cpp ${ROOT}/src/ui/os9/location.cpp
    ${ROOT}/src/platform/os9/settings_os9.cpp ${ROOT}/spike/os9/window_probe.cc)
  target_include_directories(WeatherCacheUITest PRIVATE ${ROOT}/src ${ROOT}/third_party/bearssl/inc)
  target_compile_definitions(WeatherCacheUITest PRIVATE WEATHER_CACHE_UI_TEST=1 WEATHER_VERSION="${WEATHER_VERSION_STR}")
  target_compile_options(WeatherCacheUITest PRIVATE -O2 -Wall -Wextra -fno-exceptions -fno-rtti)
  target_link_libraries(WeatherCacheUITest bearssl OpenTransportLib OpenTptInternetLib)
  add_application(PortableProbe CONSOLE
    ${ROOT}/spike/os9/portable_probe.cc
    ${ROOT}/src/platform/posix/nocxx.cpp)
  target_include_directories(PortableProbe PRIVATE ${ROOT}/src)
  target_compile_options(PortableProbe PRIVATE -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti)
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
