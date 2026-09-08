# Planetary builds its small C dependencies from pinned source snapshots so
# desktop builds do not depend on package-manager layout or prebuilt binaries.

include(CheckTypeSize)
include(GenerateExportHeader)
include(TestBigEndian)

set(CMAKE_EXTRA_INCLUDE_FILES "")
check_type_size("unsigned __int128" PLANETARY_MMDB_UINT128)
check_type_size("unsigned int __attribute__((mode(TI)))"
                PLANETARY_MMDB_UINT128_USING_MODE)
if(HAVE_PLANETARY_MMDB_UINT128)
    set(MMDB_UINT128_USING_MODE 0)
    set(MMDB_UINT128_IS_BYTE_ARRAY 0)
elseif(HAVE_PLANETARY_MMDB_UINT128_USING_MODE)
    set(MMDB_UINT128_USING_MODE 1)
    set(MMDB_UINT128_IS_BYTE_ARRAY 0)
else()
    set(MMDB_UINT128_USING_MODE 0)
    set(MMDB_UINT128_IS_BYTE_ARRAY 1)
endif()

test_big_endian(PLANETARY_MMDB_BIG_ENDIAN)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libmaxminddb/include/maxminddb_config.h.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libmaxminddb/maxminddb_config.h"
)

add_library(planetary_maxminddb STATIC
    libmaxminddb/src/maxminddb.c
    libmaxminddb/src/data-pool.c
)
add_library(Planetary::maxminddb ALIAS planetary_maxminddb)
set_target_properties(planetary_maxminddb PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE YES
)
target_compile_definitions(planetary_maxminddb PRIVATE
    PACKAGE_VERSION="1.13.3"
)
if(NOT PLANETARY_MMDB_BIG_ENDIAN)
    target_compile_definitions(planetary_maxminddb PRIVATE MMDB_LITTLE_ENDIAN=1)
endif()
if(MSVC)
    target_compile_definitions(planetary_maxminddb PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(WIN32)
    target_link_libraries(planetary_maxminddb PUBLIC ws2_32)
endif()
target_include_directories(planetary_maxminddb SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/libmaxminddb/include"
    "${CMAKE_CURRENT_BINARY_DIR}/libmaxminddb"
)

add_library(planetary_miniz STATIC
    miniz/miniz.c
    miniz/miniz_zip.c
    miniz/miniz_tinfl.c
    miniz/miniz_tdef.c
)
add_library(Planetary::miniz ALIAS planetary_miniz)
set_target_properties(planetary_miniz PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    C_STANDARD 90
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE YES
)
generate_export_header(planetary_miniz
    BASE_NAME miniz
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h"
)
target_compile_definitions(planetary_miniz
    PUBLIC MINIZ_STATIC_DEFINE
    PRIVATE $<$<C_COMPILER_ID:GNU>:_GNU_SOURCE>
)
target_include_directories(planetary_miniz SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
