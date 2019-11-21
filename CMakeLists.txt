project(telepathy-tank LANGUAGES CXX)

cmake_minimum_required(VERSION 3.2)

# Add an option for dev build
option(PEDANTIC_BUILD "Enable all kind of compiler checks" FALSE)

find_package(TelepathyQt5 0.9.6 REQUIRED)
find_package(TelepathyQt5Service 0.9.6 REQUIRED)

# Use the follow line to specify the path to Quotient
# set(Quotient_DIR "/usr/local/lib/cmake/Quotient/")
find_package(Quotient 0.6 REQUIRED)

find_package(Qt5 REQUIRED COMPONENTS Core Gui DBus Xml Network Multimedia)

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 14)

if (PEDANTIC_BUILD)
    foreach (FLAG all "" pedantic extra error=return-type no-unused-parameter no-gnu-zero-variadic-macro-arguments)
        CHECK_CXX_COMPILER_FLAG("-W${FLAG}" WARN_${FLAG}_SUPPORTED)
        if (WARN_${FLAG}_SUPPORTED AND NOT CMAKE_CXX_FLAGS MATCHES "(^| )-W?${FLAG}($| )")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W${FLAG}")
        endif ()
    endforeach ()
endif()

message(STATUS "telepathy-tank configuration:")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Qt: ${Qt5_VERSION} at ${_qt5Core_install_prefix}")
message(STATUS "  Quotient: ${Quotient_VERSION} at ${Quotient_DIR}")

add_subdirectory(src)
