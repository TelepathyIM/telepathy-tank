
set(tank_SOURCES
    connection.cpp
    connection.hpp
    main.cpp
    protocol.cpp
    protocol.hpp
    messageschannel.cpp
    messageschannel.hpp
)

if (NOT DEFINED QT_VERSION_MAJOR)
    set(QT_VERSION_MAJOR 5)
endif()

add_executable(telepathy-tank ${tank_SOURCES})

target_compile_options(telepathy-tank PRIVATE -Werror)
target_compile_features(telepathy-tank PRIVATE cxx_std_11)

find_package(QMatrixClient REQUIRED)

target_include_directories(telepathy-tank PRIVATE
    ${TELEPATHY_QT5_INCLUDE_DIR}
)

target_link_libraries(telepathy-tank
    Qt5::Core
    Qt5::DBus
    Qt5::Network
    telepathy-qt${QT_VERSION_MAJOR}
    telepathy-qt${QT_VERSION_MAJOR}-service
    QMatrixClient
    -lcrypto
    -lz
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

#install(
#    FILES telepathy-tank.png
#    DESTINATION share/icons/hicolor/48x48/apps
#)
