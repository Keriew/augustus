cmake_minimum_required(VERSION 3.16)
project(xml_parser_cleanup_tests C)

get_filename_component(AUGUSTUS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(XML_PARSER_SOURCE "${AUGUSTUS_ROOT}/src/core/xml_parser.c" CACHE FILEPATH
    "Parser source to test, optionally a baseline copy")

add_executable(xml_parser_cleanup_tests cleanup_test.c "${AUGUSTUS_ROOT}/ext/sxml/sxml.c")
target_include_directories(xml_parser_cleanup_tests PRIVATE "${AUGUSTUS_ROOT}/src" "${AUGUSTUS_ROOT}/ext")
target_compile_definitions(xml_parser_cleanup_tests PRIVATE XML_PARSER_SOURCE="${XML_PARSER_SOURCE}")
set_target_properties(xml_parser_cleanup_tests PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_definitions(xml_parser_cleanup_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(xml_parser_cleanup_tests PRIVATE /W3)
else()
    target_compile_options(xml_parser_cleanup_tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME xml_parser_cleanup COMMAND xml_parser_cleanup_tests)
