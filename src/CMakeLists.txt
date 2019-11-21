
set(tank_SOURCES
    connection.cpp
    connection.hpp
    main.cpp
    protocol.cpp
    protocol.hpp
    messageschannel.cpp
    messageschannel.hpp
    requestdetails.cpp
    requestdetails.hpp
)

if (NOT DEFINED QT_VERSION_MAJOR)
    set(QT_VERSION_MAJOR 5)
endif()

add_executable(telepathy-tank ${tank_SOURCES})

if (PEDANTIC_BUILD)
    target_compile_options(telepathy-tank PRIVATE -Werror)
endif()

target_compile_features(telepathy-tank PRIVATE cxx_std_11)

find_package(Quotient REQUIRED)

target_include_directories(telepathy-tank PRIVATE
    ${TELEPATHY_QT5_INCLUDE_DIR}
)

target_link_libraries(telepathy-tank
    Qt5::Core
    Qt5::DBus
    Qt5::Network
    telepathy-qt${QT_VERSION_MAJOR}
    telepathy-qt${QT_VERSION_MAJOR}-service
    Quotient
)

configure_file(dbus-service.in org.freedesktop.Telepathy.ConnectionManager.tank.service)

install(
    TARGETS telepathy-tank
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
)

install(
    FILES tank.manager
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/telepathy/managers
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Telepathy.ConnectionManager.tank.service
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/services
)

install(
    FILES icons/telepathy-tank-32.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps
    RENAME telepathy-tank.png
)

install(
    FILES icons/telepathy-tank-48.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps
    RENAME telepathy-tank.png
)

install(
    FILES icons/telepathy-tank-64.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
    RENAME telepathy-tank.png
)
