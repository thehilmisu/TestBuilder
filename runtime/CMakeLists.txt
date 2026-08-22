# ---------------------------------------------------------------------------
# TestBuilderRuntime -- the scenario engine on its own.
#
# Drop this into another Qt project with two lines:
#
#     add_subdirectory(path/to/TestBuilder/runtime TestBuilderRuntime)
#     target_link_libraries(MyApp PRIVATE TestBuilder::Runtime)
#
# It needs Qt Core and Qt Gui only -- no Widgets, no node editor. Everything
# the editor adds (canvas, palette, docks) stays out of your binary.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET Qt6::Core AND NOT TARGET Qt5::Core)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)
endif()

add_library(TestBuilderRuntime STATIC
    scenariobackend.h
    scenariobackend.cpp
    scenariomodel.h
    scenariomodel.cpp
    scenariorunner.h
    scenariorunner.cpp
    # The block library is the vocabulary the editor and the engine share: it
    # defines the parameters a block carries and their types, which the engine
    # needs to coerce values loaded from a file. It is plain data -- no widgets.
    ../nodeeditor/blocktypes.h
    ../nodeeditor/blocktypes.cpp
)

set_target_properties(TestBuilderRuntime PROPERTIES AUTOMOC ON)

# Consumers include "runtime/..." and "nodeeditor/...", both relative to the
# repository root, so that is what goes on the include path.
target_include_directories(TestBuilderRuntime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(TestBuilderRuntime PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
)

target_compile_features(TestBuilderRuntime PUBLIC cxx_std_17)

add_library(TestBuilder::Runtime ALIAS TestBuilderRuntime)
