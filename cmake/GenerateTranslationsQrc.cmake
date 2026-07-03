if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if(NOT DEFINED RESOURCE_PREFIX)
    set(RESOURCE_PREFIX "/")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(QRC_CONTENT "<RCC>\n")
string(APPEND QRC_CONTENT "    <qresource prefix=\"${RESOURCE_PREFIX}\">\n")

foreach(QM_FILE IN LISTS QM_FILES)
    get_filename_component(QM_FILE_NAME "${QM_FILE}" NAME)

    # planetary_translations.qrc is generated into:
    #   <build>/generated/
    #
    # The .qm files are generated into:
    #   <build>/generated/translations/
    #
    # Since rcc resolves file paths relative to the .qrc file location,
    # the correct path inside the .qrc is just translations/<file>.qm.
    string(APPEND QRC_CONTENT
        "        <file alias=\"translations/${QM_FILE_NAME}\">translations/${QM_FILE_NAME}</file>\n"
    )
endforeach()

string(APPEND QRC_CONTENT "    </qresource>\n")
string(APPEND QRC_CONTENT "</RCC>\n")

file(WRITE "${OUTPUT_FILE}" "${QRC_CONTENT}")
