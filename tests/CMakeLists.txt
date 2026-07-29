find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Core Network)

function(add_planetary_test target)
    add_executable(${target}
        ${ARGN}
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_link_libraries(${target} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
    )

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_planetary_test(test_torrent
    test_torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
)

add_planetary_test(test_torrentmodel
    test_torrentmodel.cpp
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/torrentmodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentmodel.h
    ${CMAKE_SOURCE_DIR}/appicons.cpp
    ${CMAKE_SOURCE_DIR}/appicons.h
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
)

add_planetary_test(test_torrentsortproxymodel
    test_torrentsortproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/torrentmodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentmodel.h
    ${CMAKE_SOURCE_DIR}/appicons.cpp
    ${CMAKE_SOURCE_DIR}/appicons.h
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
    ${CMAKE_SOURCE_DIR}/torrentsortproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentsortproxymodel.h
)

add_planetary_test(test_sessionsettingsdialog
    test_sessionsettingsdialog.cpp
    ${CMAKE_SOURCE_DIR}/sessionsettingsdialog.cpp
    ${CMAKE_SOURCE_DIR}/sessionsettingsdialog.h
    ${CMAKE_SOURCE_DIR}/sessionsettingsdialog.ui
)

add_planetary_test(test_bencodeparser
    test_bencodeparser.cpp
    ${CMAKE_SOURCE_DIR}/bencodevalue.cpp
    ${CMAKE_SOURCE_DIR}/bencodevalue.h
    ${CMAKE_SOURCE_DIR}/bencodeparser.cpp
    ${CMAKE_SOURCE_DIR}/bencodeparser.h
    ${CMAKE_SOURCE_DIR}/torrentmetadataparser.cpp
    ${CMAKE_SOURCE_DIR}/torrentmetadataparser.h
)

add_planetary_test(test_updatechecker
    test_updatechecker.cpp
    ${CMAKE_SOURCE_DIR}/updatechecker.cpp
    ${CMAKE_SOURCE_DIR}/updatechecker.h
)

add_planetary_test(test_watchfoldermanager
    test_watchfoldermanager.cpp
    ${CMAKE_SOURCE_DIR}/watchfoldermanager.cpp
    ${CMAKE_SOURCE_DIR}/watchfoldermanager.h
)

add_planetary_test(test_torrentfiltercontroller
    test_torrentfiltercontroller.cpp
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/torrentmodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentmodel.h
    ${CMAKE_SOURCE_DIR}/appicons.cpp
    ${CMAKE_SOURCE_DIR}/appicons.h
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
    ${CMAKE_SOURCE_DIR}/torrentsortproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentsortproxymodel.h
    ${CMAKE_SOURCE_DIR}/torrentfiltercontroller.cpp
    ${CMAKE_SOURCE_DIR}/torrentfiltercontroller.h
)

add_planetary_test(test_notificationcontroller
    test_notificationcontroller.cpp
    ${CMAKE_SOURCE_DIR}/notificationcontroller.cpp
    ${CMAKE_SOURCE_DIR}/notificationcontroller.h
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
)

add_planetary_test(test_pieceprogressbarwidget
    test_pieceprogressbarwidget.cpp
    ${CMAKE_SOURCE_DIR}/pieceprogressbarwidget.cpp
    ${CMAKE_SOURCE_DIR}/pieceprogressbarwidget.h
)

add_planetary_test(test_sessionoverviewwidget
    test_sessionoverviewwidget.cpp
    ${CMAKE_SOURCE_DIR}/sessionoverviewwidget.cpp
    ${CMAKE_SOURCE_DIR}/sessionoverviewwidget.h
)

add_planetary_test(test_delugebackend
    test_delugebackend.cpp
    ${CMAKE_SOURCE_DIR}/delugebackend.cpp
    ${CMAKE_SOURCE_DIR}/delugebackend.h
    ${CMAKE_SOURCE_DIR}/torrentbackend.h
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
)

if(APPLE)
    target_sources(test_notificationcontroller PRIVATE
        ${CMAKE_SOURCE_DIR}/macnotificationbackend.h
        ${CMAKE_SOURCE_DIR}/macnotificationbackend.mm
    )

    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/macnotificationbackend.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )

    target_link_libraries(test_notificationcontroller PRIVATE
        ${USER_NOTIFICATIONS_FRAMEWORK}
    )
endif()
