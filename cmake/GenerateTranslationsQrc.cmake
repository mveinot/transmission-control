if(NOT DEFINED OUTPUT_FILE OR OUTPUT_FILE STREQUAL "")
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(QRC_CONTENT "<RCC>\n")
string(APPEND QRC_CONTENT "    <qresource prefix=\"/\">\n")

foreach(QM_FILE IN LISTS QM_FILES)
    if(QM_FILE STREQUAL "")
        continue()
    endif()

    get_filename_component(QM_NAME "${QM_FILE}" NAME)
    string(APPEND QRC_CONTENT
        "        <file alias=\"translations/${QM_NAME}\">translations/${QM_NAME}</file>\n"
    )
endforeach()

string(APPEND QRC_CONTENT "    </qresource>\n")
string(APPEND QRC_CONTENT "</RCC>\n")

file(WRITE "${OUTPUT_FILE}" "${QRC_CONTENT}")
