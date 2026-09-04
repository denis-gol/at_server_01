cmake_minimum_required(VERSION 3.31)
project(at_server_01)

set(CMAKE_CXX_STANDARD 14)

add_executable(at_server_01
        main.cpp
        DeviceTerminalGuard.cpp
        BootstrapApp.h
        BootstrapApp.cpp
        PtyProcessor.h
        PtyProcessor.cpp
)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")

install(TARGETS at_server_01 RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_INSTALL_PREFIX}/assets/" DESTINATION bin)



