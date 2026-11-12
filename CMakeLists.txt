cmake_minimum_required(VERSION 3.16)
set(ENV{LIB}
    "$ENV{LIB};C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.22000.0\\um\\x64"
)
message(STATUS "LIB ENV = $ENV{LIB}")
project(DiamondDetectorSIgnalGenerator VERSION 0.1 LANGUAGES CXX)

option(BUILD_CORE_TESTS_ONLY "Build model regression tests without Qt" OFF)
include(CTest)
if(BUILD_TESTING)
    add_executable(pulse_model_tests tests/pulse_model_tests.cpp
        src/core/pulse_model.cpp src/core/signal_synthesis_service.cpp src/core/signal_diagnostics.cpp)
    target_include_directories(pulse_model_tests PRIVATE src/core)
    target_compile_features(pulse_model_tests PRIVATE cxx_std_17)
    add_test(NAME pulse_model_tests COMMAND pulse_model_tests)
    add_executable(fpga_processing_tests tests/fpga_processing_tests.cpp src/core/fpga_processing.cpp)
    target_include_directories(fpga_processing_tests PRIVATE src/core)
    target_compile_features(fpga_processing_tests PRIVATE cxx_std_17)
    add_test(NAME fpga_processing_tests COMMAND fpga_processing_tests)
endif()
if(BUILD_CORE_TESTS_ONLY)
    return()
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Charts Concurrent PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Charts Concurrent PrintSupport)

set(PROJECT_SOURCES
    src/app/main.cpp
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/mainwindow.ui

    src/core/Distribution.cpp
    src/core/Distribution.h
    src/core/event_time_sampler.cpp
    src/core/event_time_sampler.h
    src/core/amplitude_spectrum.cpp
    src/core/amplitude_spectrum.h
    src/core/pulse_model.cpp
    src/core/pulse_model.h
    src/core/signal_diagnostics.cpp
    src/core/signal_diagnostics.h
    src/core/signal_synthesis_service.cpp
    src/core/signal_synthesis_service.h
    src/core/amplifier_model.cpp
    src/core/amplifier_model.h
    src/core/signal_csv_loader.cpp
    src/core/signal_csv_loader.h
    src/core/amplitude_spectrum_analyzer.cpp
    src/core/amplitude_spectrum_analyzer.h
    src/core/values_provider.h
    src/core/fpga_processing.cpp
    src/core/fpga_processing.h
    src/ui/fpga_processing_dialog.cpp
    src/ui/fpga_processing_dialog.h

    src/ui/signal_plot_widget.cpp
    src/ui/signal_plot_widget.h
    src/ui/spectrumanalysiswidget.cpp
    src/ui/spectrumanalysiswidget.h
    src/ui/amplifiermodelwidget.cpp
    src/ui/amplifiermodelwidget.h
    src/ui/amplifiermodelwidget.ui

    third_party/qcustomplot.cpp
    third_party/qcustomplot.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DiamondDetectorSIgnalGenerator
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(DiamondDetectorSIgnalGenerator SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(DiamondDetectorSIgnalGenerator
            ${PROJECT_SOURCES}
        )
    endif()
endif()

if(WIN32)
    target_sources(DiamondDetectorSIgnalGenerator PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/app.rc"
    )
endif()

target_include_directories(DiamondDetectorSIgnalGenerator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(DiamondDetectorSIgnalGenerator PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::PrintSupport
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.DiamondDetectorSIgnalGenerator)
endif()
set_target_properties(DiamondDetectorSIgnalGenerator PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS DiamondDetectorSIgnalGenerator
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DiamondDetectorSIgnalGenerator)
endif()
