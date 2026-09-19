cmake_minimum_required(VERSION 3.16)
project(custom_messages_localization_tests C)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(custom_messages_localization_tests
    message_localization_test.c
    ${ROOT}/src/scenario/custom_messages_localization.c
    ${ROOT}/src/game/campaign/localization_file.c
    ${ROOT}/src/game/campaign/file.c
    ${ROOT}/src/core/xml_parser.c
    ${ROOT}/src/core/string.c
    ${ROOT}/src/core/encoding.c
    ${ROOT}/src/core/encoding_japanese.c
    ${ROOT}/src/core/encoding_korean.c
    ${ROOT}/src/core/encoding_simp_chinese.c
    ${ROOT}/src/core/encoding_trad_chinese.c
    ${ROOT}/ext/sxml/sxml.c
    ${ROOT}/ext/zip/zip.c)
target_include_directories(custom_messages_localization_tests PRIVATE ${ROOT}/src ${ROOT}/ext)
if(MSVC)
    target_compile_options(custom_messages_localization_tests PRIVATE /W3 /utf-8)
    target_compile_definitions(custom_messages_localization_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
enable_testing()
add_test(NAME custom_messages_localization_folder_and_zip COMMAND custom_messages_localization_tests)
