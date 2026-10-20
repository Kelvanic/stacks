cmake_minimum_required(VERSION 3.21)

project(Stacks VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Multimedia Network Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)
find_package(ZLIB REQUIRED)

qt_standard_project_setup()

add_library(pdfium SHARED IMPORTED)
set_target_properties(pdfium PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/libpdfium.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/include"
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
    Qt6::Core Qt6::Gui Qt6::Sql Qt6::Multimedia Qt6::Network
    pdfium PkgConfig::LIBZIP ZLIB::ZLIB
)
set_target_properties(stacks_core PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib"
)

add_library(stacks_native STATIC
    src/native/ReaderItem.cpp
    src/native/ReaderItem.h
)
target_link_libraries(stacks_native PUBLIC stacks_core Qt6::Quick Qt6::Gui)
set_target_properties(stacks_native PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib"
)

qt_add_executable(stacks src/main.cpp)
target_link_libraries(stacks PRIVATE stacks_native Qt6::Quick Qt6::QuickControls2 Qt6::Gui)
target_compile_definitions(stacks PRIVATE
    STACKS_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    STACKS_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    STACKS_SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/samples"
)
set_target_properties(stacks PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib"
)

qt_add_executable(stacks_tests tests/tst_core.cpp)
target_link_libraries(stacks_tests PRIVATE stacks_native Qt6::Quick Qt6::Test)
target_compile_definitions(stacks_tests PRIVATE
    STACKS_SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/samples"
)
set_target_properties(stacks_tests PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib"
)

enable_testing()
add_test(NAME stacks_core COMMAND stacks_tests)
set_tests_properties(stacks_core PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;STACKS_TEST_MODE=1"
)
