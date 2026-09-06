find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Core Network)

# With the project's legacy CMake policy scope, GENERATED is not automatically
# visible in this subdirectory during a clean configure.
set_source_files_properties(
    "${PLANETARY_GENERATED_TRANSLATIONS_CPP}"
    PROPERTIES
        GENERATED TRUE
        SKIP_AUTOGEN TRUE
)

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
    ${PLANETARY_ICON_THEME_SOURCES}
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
)

add_planetary_test(test_torrentsortproxymodel
    test_torrentsortproxymodel.cpp
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/torrentmodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentmodel.h
    ${PLANETARY_ICON_THEME_SOURCES}
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

add_planetary_test(test_statusbarcontroller
    test_statusbarcontroller.cpp
    ${CMAKE_SOURCE_DIR}/statusbarcontroller.cpp
    ${CMAKE_SOURCE_DIR}/statusbarcontroller.h
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/torrentbackend.h
)

add_planetary_test(test_serverprofile
    test_serverprofile.cpp
    ${CMAKE_SOURCE_DIR}/serverprofile.cpp
    ${CMAKE_SOURCE_DIR}/serverprofile.h
)

add_planetary_test(test_serverconnectionprobe
    test_serverconnectionprobe.cpp
    ${CMAKE_SOURCE_DIR}/serverconnectionprobe.cpp
    ${CMAKE_SOURCE_DIR}/serverconnectionprobe.h
)

add_planetary_test(test_applicationlocale
    test_applicationlocale.cpp
    ${CMAKE_SOURCE_DIR}/applicationlocale.cpp
    ${CMAKE_SOURCE_DIR}/applicationlocale.h
    ${PLANETARY_GENERATED_TRANSLATIONS_CPP}
)
add_dependencies(test_applicationlocale PlanetaryTranslations)

add_planetary_test(test_windowlayoutcontroller
    test_windowlayoutcontroller.cpp
    ${CMAKE_SOURCE_DIR}/windowlayoutcontroller.cpp
    ${CMAKE_SOURCE_DIR}/windowlayoutcontroller.h
)

add_planetary_test(test_pollingcoordinator
    test_pollingcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/pollingcoordinator.cpp
    ${CMAKE_SOURCE_DIR}/pollingcoordinator.h
)

add_planetary_test(test_applicationcommandcontroller
    test_applicationcommandcontroller.cpp
    ${CMAKE_SOURCE_DIR}/applicationcommandcontroller.cpp
    ${CMAKE_SOURCE_DIR}/applicationcommandcontroller.h
    ${PLANETARY_ICON_THEME_SOURCES}
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
)

add_planetary_test(test_iconthemeregistry
    test_iconthemeregistry.cpp
    ${PLANETARY_ICON_THEME_SOURCES}
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
)

add_planetary_test(test_torrentfiltercontroller
    test_torrentfiltercontroller.cpp
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/torrentmodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentmodel.h
    ${PLANETARY_ICON_THEME_SOURCES}
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

add_planetary_test(test_peerreconciler
    test_peerreconciler.cpp
    ${CMAKE_SOURCE_DIR}/peerreconciler.cpp
    ${CMAKE_SOURCE_DIR}/peerreconciler.h
    ${CMAKE_SOURCE_DIR}/torrentpeerscontroller.cpp
    ${CMAKE_SOURCE_DIR}/torrentpeerscontroller.h
    ${CMAKE_SOURCE_DIR}/geoipservice.cpp
    ${CMAKE_SOURCE_DIR}/geoipservice.h
    ${CMAKE_SOURCE_DIR}/tablecolumncontroller.cpp
    ${CMAKE_SOURCE_DIR}/tablecolumncontroller.h
    ${CMAKE_SOURCE_DIR}/tableplaceholdercontroller.cpp
    ${CMAKE_SOURCE_DIR}/tableplaceholdercontroller.h
)

add_planetary_test(test_torrentfilemodel
    test_torrentfilemodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentfilemodel.cpp
    ${CMAKE_SOURCE_DIR}/torrentfilemodel.h
    ${PLANETARY_ICON_THEME_SOURCES}
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
)

add_planetary_test(test_sessionoverviewwidget
    test_sessionoverviewwidget.cpp
    ${CMAKE_SOURCE_DIR}/sessionoverviewwidget.cpp
    ${CMAKE_SOURCE_DIR}/sessionoverviewwidget.h
)

add_planetary_test(test_torrentaddcontroller
    test_torrentaddcontroller.cpp
    ${CMAKE_SOURCE_DIR}/torrentaddcontroller.cpp
    ${CMAKE_SOURCE_DIR}/torrentaddcontroller.h
    ${CMAKE_SOURCE_DIR}/torrentadddialog.cpp
    ${CMAKE_SOURCE_DIR}/torrentadddialog.h
    ${CMAKE_SOURCE_DIR}/torrentadddialog.ui
    ${CMAKE_SOURCE_DIR}/torrentmetadataparser.cpp
    ${CMAKE_SOURCE_DIR}/torrentmetadataparser.h
    ${CMAKE_SOURCE_DIR}/bencodeparser.cpp
    ${CMAKE_SOURCE_DIR}/bencodeparser.h
    ${CMAKE_SOURCE_DIR}/bencodevalue.cpp
    ${CMAKE_SOURCE_DIR}/bencodevalue.h
    ${CMAKE_SOURCE_DIR}/torrentbackend.h
)

add_planetary_test(test_delugebackend
    test_delugebackend.cpp
    ${CMAKE_SOURCE_DIR}/delugebackend.cpp
    ${CMAKE_SOURCE_DIR}/delugebackend.h
    ${CMAKE_SOURCE_DIR}/torrentbackend.h
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/serverprofile.cpp
    ${CMAKE_SOURCE_DIR}/serverprofile.h
)

add_planetary_test(test_transmissionbackend
    test_transmissionbackend.cpp
    fakehttpserver.h
    ${CMAKE_SOURCE_DIR}/transmissionbackend.cpp
    ${CMAKE_SOURCE_DIR}/transmissionbackend.h
    ${CMAKE_SOURCE_DIR}/transmissionprotocol.cpp
    ${CMAKE_SOURCE_DIR}/transmissionprotocol.h
    ${CMAKE_SOURCE_DIR}/torrentbackend.h
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/serverprofile.cpp
    ${CMAKE_SOURCE_DIR}/serverprofile.h
)

add_planetary_test(test_qbittorrentbackend
    test_qbittorrentbackend.cpp
    fakehttpserver.h
    ${CMAKE_SOURCE_DIR}/qbittorrentbackend.cpp
    ${CMAKE_SOURCE_DIR}/qbittorrentbackend.h
    ${CMAKE_SOURCE_DIR}/torrentbackend.h
    ${CMAKE_SOURCE_DIR}/torrent.cpp
    ${CMAKE_SOURCE_DIR}/torrent.h
    ${CMAKE_SOURCE_DIR}/serverprofile.cpp
    ${CMAKE_SOURCE_DIR}/serverprofile.h
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
