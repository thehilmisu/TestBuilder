# ---------------------------------------------------------------------------
# A host application linking the scenario engine. The two lines that matter are
# add_subdirectory() and target_link_libraries(); everything else here is just
# the example's own build.
#
#     cmake -S examples/integration -B build-example
#     cmake --build build-example
#     ./build-example/scenariorun myscenario.tbscn
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(scenariorun LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

# 1. Pull in the engine. No Widgets, no editor.
add_subdirectory(../../runtime TestBuilderRuntime)

add_executable(scenariorun
    main.cpp
    appbackend.h
    appbackend.cpp
)

# 2. Link it.
target_link_libraries(scenariorun PRIVATE TestBuilder::Runtime)
