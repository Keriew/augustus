cmake_minimum_required(VERSION 3.16)
project(campaign_localization_tests C)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(campaign_localization_tests
    campaign_localization_test.c
    ${ROOT}/src/game/campaign/campaign.c
    ${ROOT}/src/game/campaign/file.c
    ${ROOT}/src/game/campaign/mission.c
    ${ROOT}/src/game/campaign/xml.c
    ${ROOT}/src/game/campaign/localization.c
    ${ROOT}/src/game/campaign/localization_file.c
    ${ROOT}/src/core/xml_parser.c
    ${ROOT}/src/core/array.c
    ${ROOT}/src/core/buffer.c
    ${ROOT}/src/core/string.c
    ${ROOT}/src/core/encoding.c
    ${ROOT}/src/core/encoding_japanese.c
    ${ROOT}/src/core/encoding_korean.c
    ${ROOT}/src/core/encoding_simp_chinese.c
    ${ROOT}/src/core/encoding_trad_chinese.c
    ${ROOT}/ext/sxml/sxml.c
    ${ROOT}/ext/zip/zip.c)
target_include_directories(campaign_localization_tests PRIVATE ${ROOT}/src ${ROOT}/ext)
if(MSVC)
    target_compile_options(campaign_localization_tests PRIVATE /W3 /utf-8)
    target_compile_definitions(campaign_localization_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
enable_testing()
add_test(NAME campaign_localization_folder_and_zip COMMAND campaign_localization_tests)
