project(telepathy-tank LANGUAGES CXX)

cmake_minimum_required(VERSION 3.2)

find_package(TelepathyQt5 0.9.6 REQUIRED)
find_package(TelepathyQt5Service 0.9.6 REQUIRED)

find_package(QMatrixClient REQUIRED)

find_package(Qt5 REQUIRED COMPONENTS Core Gui DBus Xml Network)

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 14)

foreach (FLAG all "" pedantic extra error=return-type no-unused-parameter no-gnu-zero-variadic-macro-arguments)
    CHECK_CXX_COMPILER_FLAG("-W${FLAG}" WARN_${FLAG}_SUPPORTED)
    if (WARN_${FLAG}_SUPPORTED AND NOT CMAKE_CXX_FLAGS MATCHES "(^| )-W?${FLAG}($| )")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W${FLAG}")
    endif ()
endforeach ()

message(STATUS "telepathy-tank configuration:")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Qt: ${Qt5_VERSION} at ${_qt5Core_install_prefix}")
message(STATUS "  QMatrixClient: ${QMatrixClient_VERSION} at ${QMatrixClient_DIR}")

add_subdirectory(src)
