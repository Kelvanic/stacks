cmake_minimum_required(VERSION 3.21.1)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" STACKS_DEFAULT_VERSION LIMIT_COUNT 1)
set(STACKS_VERSION "${STACKS_DEFAULT_VERSION}" CACHE STRING "Numeric package version")
set(STACKS_DISPLAY_VERSION "${STACKS_VERSION}" CACHE STRING "Application/release version, including prerelease suffix")
project(Stacks VERSION ${STACKS_VERSION} LANGUAGES CXX)
option(STACKS_BUNDLED_RESOURCES "Embed the UI, fonts, textures and artwork for relocatable packages" ON)
if(MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus)
endif()
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(CTest)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Multimedia Network Xml)
# Ubuntu 22.04's libzip config imports optional zipcmp/zipmerge tools even
# when only libzip-dev is installed. pkg-config avoids that broken config.
if(WIN32 OR APPLE)
    find_package(libzip CONFIG QUIET)
endif()
if(TARGET libzip::zip)
    set(STACKS_ZIP_TARGET libzip::zip)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)
    set(STACKS_ZIP_TARGET PkgConfig::LIBZIP)
endif()
find_package(ZLIB REQUIRED)

qt_standard_project_setup()

add_library(pdfium SHARED IMPORTED)
set(PDFIUM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "Platform-specific PDFium distribution")
find_path(PDFIUM_INCLUDE_DIR fpdfview.h PATHS "${PDFIUM_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
if(WIN32)
    find_file(PDFIUM_LIBRARY NAMES pdfium.dll PATHS "${PDFIUM_ROOT}/bin" "${PDFIUM_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
    find_library(PDFIUM_IMPLIB NAMES pdfium pdfium.dll PATHS "${PDFIUM_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
    set_target_properties(pdfium PROPERTIES IMPORTED_IMPLIB "${PDFIUM_IMPLIB}")
elseif(APPLE)
    find_file(PDFIUM_LIBRARY NAMES libpdfium.dylib PATHS "${PDFIUM_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
    # Some upstream builds use ./libpdfium.dylib as their install name. Fix a
    # build-local copy, leaving the downloaded distribution untouched.
    set(PDFIUM_RUNTIME_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/pdfium/libpdfium.dylib")
    configure_file("${PDFIUM_LIBRARY}" "${PDFIUM_RUNTIME_LIBRARY}" COPYONLY)
    find_program(INSTALL_NAME_TOOL install_name_tool REQUIRED)
    find_program(CODESIGN codesign REQUIRED)
    execute_process(COMMAND "${INSTALL_NAME_TOOL}" -id "@rpath/libpdfium.dylib" "${PDFIUM_RUNTIME_LIBRARY}"
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND "${CODESIGN}" --force --sign - "${PDFIUM_RUNTIME_LIBRARY}"
        COMMAND_ERROR_IS_FATAL ANY)
else()
    find_library(PDFIUM_LIBRARY NAMES pdfium PATHS "${PDFIUM_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
endif()
get_filename_component(PDFIUM_LIBRARY_DIR "${PDFIUM_LIBRARY}" DIRECTORY)
if(APPLE)
    set(PDFIUM_LIBRARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/pdfium")
    set(PDFIUM_IMPORTED_LIBRARY "${PDFIUM_RUNTIME_LIBRARY}")
else()
    set(PDFIUM_IMPORTED_LIBRARY "${PDFIUM_LIBRARY}")
endif()
set_target_properties(pdfium PROPERTIES
    IMPORTED_LOCATION "${PDFIUM_IMPORTED_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${PDFIUM_INCLUDE_DIR}"
)

add_library(stacks_core STATIC
    src/core/AppController.cpp
    src/core/AppController.h
    src/core/KindleConverter.cpp
    src/core/KindleConverter.h
    src/core/KindleSender.cpp
    src/core/KindleSender.h
    src/core/Book.h
    src/core/Database.cpp
    src/core/Database.h
    src/core/EpubDocument.cpp
    src/core/EpubDocument.h
    src/core/Library.cpp
    src/core/Library.h
    src/core/PdfDocument.cpp
    src/core/PdfDocument.h
    src/core/VoiceRecorder.cpp
    src/core/VoiceRecorder.h
)
target_include_directories(stacks_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(stacks_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Sql Qt6::Multimedia Qt6::Network Qt6::Xml
    pdfium ${STACKS_ZIP_TARGET} ZLIB::ZLIB
)
if(WIN32)
    target_link_libraries(stacks_core PUBLIC crypt32)
endif()
set_target_properties(stacks_core PROPERTIES
    BUILD_RPATH "${PDFIUM_LIBRARY_DIR}"
)

add_library(stacks_native STATIC
    src/native/ReaderItem.cpp
    src/native/ReaderItem.h
)
target_link_libraries(stacks_native PUBLIC stacks_core Qt6::Quick Qt6::Gui)
set_target_properties(stacks_native PROPERTIES
    BUILD_RPATH "${PDFIUM_LIBRARY_DIR}"
)

qt_add_executable(stacks src/main.cpp)
qt_add_resources(stacks "stacks_branding"
    PREFIX "/branding"
    BASE "assets/branding"
    FILES assets/branding/stacks-icon.png
)
if(APPLE)
    set(STACKS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/Stacks.icns")
    set_source_files_properties("${STACKS_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(stacks PRIVATE "${STACKS_ICON}")
    set_target_properties(stacks PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Stacks"
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.stacks.reader"
        MACOSX_BUNDLE_ICON_FILE "Stacks.icns"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    # Keep the documented CLI and e2e entry point available alongside the app.
    add_custom_command(TARGET stacks POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/stacks"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE:stacks>" "${CMAKE_CURRENT_BINARY_DIR}/stacks"
    )
endif()
if(STACKS_BUNDLED_RESOURCES)
    file(GLOB STACKS_UI_FILES CONFIGURE_DEPENDS "qml/*.qml" "qml/*.js")
    file(GLOB STACKS_FONT_FILES CONFIGURE_DEPENDS "assets/fonts/*")
    file(GLOB STACKS_TEXTURE_FILES CONFIGURE_DEPENDS "assets/textures/*")
    qt_add_resources(stacks "stacks_ui"
        PREFIX "/stacks"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES ${STACKS_UI_FILES} ${STACKS_FONT_FILES} ${STACKS_TEXTURE_FILES}
              qml/qmldir
    )
    # Qt keeps a resource alias on source files already used in another qrc.
    # Give the shared app/brand artwork its own prefix, preserving that alias.
    qt_add_resources(stacks "stacks_ui_branding"
        PREFIX "/stacks/assets/branding"
        BASE "assets/branding"
        FILES assets/branding/stacks-icon.png
    )
    target_compile_definitions(stacks PRIVATE STACKS_BUNDLED_RESOURCES)
endif()
if(WIN32)
    set_target_properties(stacks PROPERTIES WIN32_EXECUTABLE TRUE)
    configure_file(cmake/Stacks.rc.in "${CMAKE_CURRENT_BINARY_DIR}/Stacks.rc" @ONLY)
    target_sources(stacks PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Stacks.rc")
endif()
target_link_libraries(stacks PRIVATE stacks_native Qt6::Quick Qt6::QuickControls2 Qt6::Gui)
target_compile_definitions(stacks PRIVATE
    STACKS_APP_VERSION="${STACKS_DISPLAY_VERSION}"
    STACKS_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    STACKS_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    STACKS_SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/samples"
)
set_target_properties(stacks PROPERTIES
    BUILD_RPATH "${PDFIUM_LIBRARY_DIR}"
)
include(cmake/Packaging.cmake)

if(WIN32)
    add_custom_command(TARGET stacks POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:pdfium>" "$<TARGET_FILE_DIR:stacks>")
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(stacks_tests tests/tst_core.cpp)
    target_link_libraries(stacks_tests PRIVATE stacks_native Qt6::Quick Qt6::Test)
    target_compile_definitions(stacks_tests PRIVATE STACKS_SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/samples")
    add_test(NAME stacks_core COMMAND stacks_tests -o "${CMAKE_CURRENT_BINARY_DIR}/core-results.txt,txt" -o -,txt)
    set_tests_properties(stacks_core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;STACKS_TEST_MODE=1")

    qt_add_executable(stacks_regression_tests tests/tst_regressions.cpp)
    target_link_libraries(stacks_regression_tests PRIVATE stacks_native Qt6::Quick Qt6::Test)
    target_compile_definitions(stacks_regression_tests PRIVATE
        STACKS_SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/samples"
        STACKS_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    )
    add_test(NAME stacks_regressions COMMAND stacks_regression_tests -o "${CMAKE_CURRENT_BINARY_DIR}/regression-results.txt,txt" -o -,txt)
    set_tests_properties(stacks_regressions PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;STACKS_TEST_MODE=1"
    )

    qt_add_executable(stacks_smtp_tests tests/tst_smtp.cpp)
    target_link_libraries(stacks_smtp_tests PRIVATE stacks_core Qt6::Test)
    target_compile_definitions(stacks_smtp_tests PRIVATE STACKS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME stacks_smtp COMMAND stacks_smtp_tests -o "${CMAKE_CURRENT_BINARY_DIR}/smtp-results.txt,txt" -o -,txt)

    foreach(test_target stacks_tests stacks_regression_tests stacks_smtp_tests)
        set_target_properties(${test_target} PROPERTIES BUILD_RPATH "${PDFIUM_LIBRARY_DIR}")
        if(WIN32)
            set_target_properties(${test_target} PROPERTIES WIN32_EXECUTABLE FALSE)
            add_custom_command(TARGET ${test_target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:pdfium>" "$<TARGET_FILE_DIR:${test_target}>")
        endif()
    endforeach()
endif()
