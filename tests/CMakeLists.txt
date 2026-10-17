find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Core Network)

set(PLANETARY_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

# With the project's legacy CMake policy scope, GENERATED is not automatically
# visible in this subdirectory during a clean configure.
set_source_files_properties(
    "${PLANETARY_GENERATED_TRANSLATIONS_CPP}"
    PROPERTIES
        GENERATED TRUE
        SKIP_AUTOGEN TRUE
)

add_library(planetary_test_theme_support OBJECT
    ${PLANETARY_THEME_SOURCES}
)
target_include_directories(planetary_test_theme_support PRIVATE
    ${PLANETARY_SOURCE_DIR}
)
target_link_libraries(planetary_test_theme_support PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Planetary::miniz
)
target_compile_definitions(planetary_test_theme_support PRIVATE
    PLANETARY_HAVE_MINIZ
)

# Compile common production sources once and share their objects between the
# independent test executables. Each executable still gets its own copy at
# link time, so tests remain isolated while avoiding repeated compilation.
function(add_planetary_test_support target)
    add_library(${target} OBJECT ${ARGN})
    target_include_directories(${target} PRIVATE ${PLANETARY_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
    )
endfunction()

add_planetary_test_support(planetary_test_resources
    ${CMAKE_SOURCE_DIR}/Resources/resources.qrc
)
add_planetary_test_support(planetary_test_torrent
    ${PLANETARY_SOURCE_DIR}/torrent.cpp
    ${PLANETARY_SOURCE_DIR}/torrent.h
)
add_planetary_test_support(planetary_test_serverprofile
    ${PLANETARY_SOURCE_DIR}/serverprofile.cpp
    ${PLANETARY_SOURCE_DIR}/serverprofile.h
)
add_planetary_test_support(planetary_test_torrentmodel
    ${PLANETARY_SOURCE_DIR}/torrentmodel.cpp
    ${PLANETARY_SOURCE_DIR}/torrentmodel.h
)
add_planetary_test_support(planetary_test_torrentsortproxymodel
    ${PLANETARY_SOURCE_DIR}/torrentsortproxymodel.cpp
    ${PLANETARY_SOURCE_DIR}/torrentsortproxymodel.h
)
add_planetary_test_support(planetary_test_bencode
    ${PLANETARY_SOURCE_DIR}/bencodevalue.cpp
    ${PLANETARY_SOURCE_DIR}/bencodevalue.h
    ${PLANETARY_SOURCE_DIR}/bencodeparser.cpp
    ${PLANETARY_SOURCE_DIR}/bencodeparser.h
    ${PLANETARY_SOURCE_DIR}/torrentmetadataparser.cpp
    ${PLANETARY_SOURCE_DIR}/torrentmetadataparser.h
)

function(add_planetary_test target)
    add_executable(${target}
        ${ARGN}
    )

    target_include_directories(${target} PRIVATE
        ${PLANETARY_SOURCE_DIR}
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

function(add_planetary_test_with_theme_support target)
    add_planetary_test(${target} ${ARGN})
    target_sources(${target} PRIVATE
        $<TARGET_OBJECTS:planetary_test_theme_support>
    )
    target_link_libraries(${target} PRIVATE Planetary::miniz)
endfunction()

function(use_planetary_test_support test_target support_target)
    target_sources(${test_target} PRIVATE $<TARGET_OBJECTS:${support_target}>)
endfunction()

function(use_planetary_test_resources test_target)
    use_planetary_test_support(${test_target} planetary_test_resources)
endfunction()

add_planetary_test(test_torrent
    test_torrent.cpp
)
use_planetary_test_support(test_torrent planetary_test_torrent)

add_planetary_test(test_activitylogmodel
    test_activitylogmodel.cpp
    ${PLANETARY_SOURCE_DIR}/activitylogmodel.cpp
    ${PLANETARY_SOURCE_DIR}/activitylogmodel.h
)

add_planetary_test_with_theme_support(test_torrentmodel
    test_torrentmodel.cpp
)
use_planetary_test_support(test_torrentmodel planetary_test_torrent)
use_planetary_test_support(test_torrentmodel planetary_test_torrentmodel)
use_planetary_test_resources(test_torrentmodel)

add_planetary_test_with_theme_support(test_torrentsortproxymodel
    test_torrentsortproxymodel.cpp
)
use_planetary_test_support(test_torrentsortproxymodel planetary_test_torrent)
use_planetary_test_support(test_torrentsortproxymodel planetary_test_torrentmodel)
use_planetary_test_support(test_torrentsortproxymodel planetary_test_torrentsortproxymodel)
use_planetary_test_resources(test_torrentsortproxymodel)

add_planetary_test(test_sessionsettingsdialog
    test_sessionsettingsdialog.cpp
    ${PLANETARY_SOURCE_DIR}/sessionsettingsdialog.cpp
    ${PLANETARY_SOURCE_DIR}/sessionsettingsdialog.h
    ${PLANETARY_SOURCE_DIR}/sessionsettingsdialog.ui
)

add_planetary_test(test_bencodeparser
    test_bencodeparser.cpp
)
use_planetary_test_support(test_bencodeparser planetary_test_bencode)

add_planetary_test(test_updatechecker
    test_updatechecker.cpp
    ${PLANETARY_SOURCE_DIR}/updatechecker.cpp
    ${PLANETARY_SOURCE_DIR}/updatechecker.h
)

add_planetary_test(test_watchfoldermanager
    test_watchfoldermanager.cpp
    ${PLANETARY_SOURCE_DIR}/watchfoldermanager.cpp
    ${PLANETARY_SOURCE_DIR}/watchfoldermanager.h
)

add_planetary_test_with_theme_support(test_statusbarcontroller
    test_statusbarcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/statusbarcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/statusbarcontroller.h
    ${PLANETARY_SOURCE_DIR}/torrentbackend.h
)
use_planetary_test_support(test_statusbarcontroller planetary_test_torrent)
use_planetary_test_resources(test_statusbarcontroller)

add_planetary_test(test_serverprofile
    test_serverprofile.cpp
    ${PLANETARY_SOURCE_DIR}/serverprofile.cpp
    ${PLANETARY_SOURCE_DIR}/serverprofile.h
)

add_planetary_test(test_serverconnectionprobe
    test_serverconnectionprobe.cpp
    ${PLANETARY_SOURCE_DIR}/serverconnectionprobe.cpp
    ${PLANETARY_SOURCE_DIR}/serverconnectionprobe.h
)

add_planetary_test(test_applicationlocale
    test_applicationlocale.cpp
    ${PLANETARY_SOURCE_DIR}/applicationlocale.cpp
    ${PLANETARY_SOURCE_DIR}/applicationlocale.h
    ${PLANETARY_GENERATED_TRANSLATIONS_CPP}
)
add_dependencies(test_applicationlocale PlanetaryTranslations)

add_planetary_test(test_windowlayoutcontroller
    test_windowlayoutcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/windowlayoutcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/windowlayoutcontroller.h
)

add_planetary_test(test_pollingcoordinator
    test_pollingcoordinator.cpp
    ${PLANETARY_SOURCE_DIR}/pollingcoordinator.cpp
    ${PLANETARY_SOURCE_DIR}/pollingcoordinator.h
)

add_planetary_test_with_theme_support(test_applicationcommandcontroller
    test_applicationcommandcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/applicationcommandcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/applicationcommandcontroller.h
)
use_planetary_test_resources(test_applicationcommandcontroller)

add_planetary_test_with_theme_support(test_iconthemeregistry
    test_iconthemeregistry.cpp
)
use_planetary_test_resources(test_iconthemeregistry)

add_planetary_test_with_theme_support(test_torrentfiltercontroller
    test_torrentfiltercontroller.cpp
    ${PLANETARY_SOURCE_DIR}/torrentfiltercontroller.cpp
    ${PLANETARY_SOURCE_DIR}/torrentfiltercontroller.h
)
use_planetary_test_support(test_torrentfiltercontroller planetary_test_torrent)
use_planetary_test_support(test_torrentfiltercontroller planetary_test_torrentmodel)
use_planetary_test_support(test_torrentfiltercontroller planetary_test_torrentsortproxymodel)
use_planetary_test_resources(test_torrentfiltercontroller)

add_planetary_test(test_notificationcontroller
    test_notificationcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/notificationcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/notificationcontroller.h
)
use_planetary_test_support(test_notificationcontroller planetary_test_torrent)

add_planetary_test_with_theme_support(test_pieceprogressbarwidget
    test_pieceprogressbarwidget.cpp
    ${PLANETARY_SOURCE_DIR}/pieceprogressbarwidget.cpp
    ${PLANETARY_SOURCE_DIR}/pieceprogressbarwidget.h
)
use_planetary_test_resources(test_pieceprogressbarwidget)

add_planetary_test(test_peerreconciler
    test_peerreconciler.cpp
    ${PLANETARY_SOURCE_DIR}/peerreconciler.cpp
    ${PLANETARY_SOURCE_DIR}/peerreconciler.h
    ${PLANETARY_SOURCE_DIR}/torrentpeerscontroller.cpp
    ${PLANETARY_SOURCE_DIR}/torrentpeerscontroller.h
    ${PLANETARY_SOURCE_DIR}/geoipservice.cpp
    ${PLANETARY_SOURCE_DIR}/geoipservice.h
    ${PLANETARY_SOURCE_DIR}/tablecolumncontroller.cpp
    ${PLANETARY_SOURCE_DIR}/tablecolumncontroller.h
    ${PLANETARY_SOURCE_DIR}/tableplaceholdercontroller.cpp
    ${PLANETARY_SOURCE_DIR}/tableplaceholdercontroller.h
)

add_planetary_test_with_theme_support(test_torrentfilemodel
    test_torrentfilemodel.cpp
    ${PLANETARY_SOURCE_DIR}/torrentfilemodel.cpp
    ${PLANETARY_SOURCE_DIR}/torrentfilemodel.h
)
use_planetary_test_resources(test_torrentfilemodel)

add_planetary_test_with_theme_support(test_sessionoverviewwidget
    test_sessionoverviewwidget.cpp
    ${PLANETARY_SOURCE_DIR}/sessionoverviewwidget.cpp
    ${PLANETARY_SOURCE_DIR}/sessionoverviewwidget.h
)
use_planetary_test_resources(test_sessionoverviewwidget)

add_planetary_test_with_theme_support(test_colorthememanager
    test_colorthememanager.cpp
)
use_planetary_test_resources(test_colorthememanager)

add_planetary_test(test_torrentaddcontroller
    test_torrentaddcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/torrentaddcontroller.cpp
    ${PLANETARY_SOURCE_DIR}/torrentaddcontroller.h
    ${PLANETARY_SOURCE_DIR}/torrentadddialog.cpp
    ${PLANETARY_SOURCE_DIR}/torrentadddialog.h
    ${PLANETARY_SOURCE_DIR}/torrentadddialog.ui
    ${PLANETARY_SOURCE_DIR}/torrentbackend.h
)
use_planetary_test_support(test_torrentaddcontroller planetary_test_bencode)
use_planetary_test_support(test_torrentaddcontroller planetary_test_torrent)

add_planetary_test(test_delugebackend
    test_delugebackend.cpp
    ${PLANETARY_SOURCE_DIR}/delugebackend.cpp
    ${PLANETARY_SOURCE_DIR}/delugebackend.h
    ${PLANETARY_SOURCE_DIR}/torrentbackend.h
)
use_planetary_test_support(test_delugebackend planetary_test_torrent)
use_planetary_test_support(test_delugebackend planetary_test_serverprofile)

add_planetary_test(test_transmissionbackend
    test_transmissionbackend.cpp
    fakehttpserver.h
    ${PLANETARY_SOURCE_DIR}/transmissionbackend.cpp
    ${PLANETARY_SOURCE_DIR}/transmissionbackend.h
    ${PLANETARY_SOURCE_DIR}/transmissionprotocol.cpp
    ${PLANETARY_SOURCE_DIR}/transmissionprotocol.h
    ${PLANETARY_SOURCE_DIR}/torrentbackend.h
)
use_planetary_test_support(test_transmissionbackend planetary_test_torrent)
use_planetary_test_support(test_transmissionbackend planetary_test_serverprofile)

add_planetary_test(test_qbittorrentbackend
    test_qbittorrentbackend.cpp
    fakehttpserver.h
    ${PLANETARY_SOURCE_DIR}/qbittorrentbackend.cpp
    ${PLANETARY_SOURCE_DIR}/qbittorrentbackend.h
    ${PLANETARY_SOURCE_DIR}/torrentbackend.h
)
use_planetary_test_support(test_qbittorrentbackend planetary_test_torrent)
use_planetary_test_support(test_qbittorrentbackend planetary_test_serverprofile)

if(APPLE)
    target_sources(test_notificationcontroller PRIVATE
        ${PLANETARY_SOURCE_DIR}/macnotificationbackend.h
        ${PLANETARY_SOURCE_DIR}/macnotificationbackend.mm
    )

    set_source_files_properties(
        ${PLANETARY_SOURCE_DIR}/macnotificationbackend.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )

    target_link_libraries(test_notificationcontroller PRIVATE
        ${USER_NOTIFICATIONS_FRAMEWORK}
    )
endif()
