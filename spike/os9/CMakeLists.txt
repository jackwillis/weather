cmake_minimum_required(VERSION 3.9)
project(os9probe)
add_application(Probe1 CONSOLE probe1.cc)
add_application(Probe2 CONSOLE probe2.cc)
target_link_libraries(Probe2 OpenTransportLib OpenTptInternetLib)

# Probe3 needs Apple's Open Transport static glue (not redistributable, never committed):
#   cmake ... -DOT_SDK=/path/to/dir containing OpenTransportAppPPC.o and OpenTptInetPPC.o (XCOFF, MacBinary stripped)
if(OT_SDK)
  add_application(Probe3 CONSOLE probe3.cc)
  target_link_libraries(Probe3 ${OT_SDK}/OpenTransportAppPPC.o ${OT_SDK}/OpenTptInetPPC.o OpenTransportLib OpenTptInternetLib)
endif()
