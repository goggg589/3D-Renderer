cmake_minimum_required(VERSION 3.15)
project(ThreeDRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_QT   "Build Qt viewer"   ON)
option(ENABLE_SFML "Build SFML apps"   ON)

# ---------------- core ----------------
add_library(core
        src/core/Math.h
        src/core/Mesh.h
        src/core/Geometry.h
        src/core/ObjLoader.h  src/core/ObjLoader.cpp
        src/core/Camera.h
        src/core/Renderer.h   src/core/Renderer.cpp
)
target_include_directories(core PUBLIC src)

# ---------------- SFML (CLI + optional GUI) ----------------
set(SFML_FOUND FALSE)
if(ENABLE_SFML)
    # First try SFML 3 (Homebrew on macOS), then fall back to SFML 2.x (Linux)
    find_package(SFML 3 QUIET COMPONENTS Graphics Window System)
    if(SFML_FOUND)
        set(SFML_VERSION_MAJOR 3)
    else()
        find_package(SFML 2.5 QUIET COMPONENTS graphics window system)
        if(SFML_FOUND)
            set(SFML_VERSION_MAJOR 2)
        endif()
    endif()

    if(SFML_FOUND)
        # render-cli (uses SFML)
        add_executable(render-cli src/apps/render_cli.cpp)
        target_link_libraries(render-cli PRIVATE core)
        if(SFML_VERSION_MAJOR EQUAL 3)
            target_link_libraries(render-cli PRIVATE SFML::Graphics SFML::Window SFML::System)
        else()
            target_link_libraries(render-cli PRIVATE sfml-graphics sfml-window sfml-system)
        endif()

        # render-gui (optional) – only add if the source file actually exists
        if(EXISTS "${CMAKE_SOURCE_DIR}/src/apps/render_gui.cpp")
            add_executable(render-gui src/apps/render_gui.cpp)
            target_link_libraries(render-gui PRIVATE core)
            if(SFML_VERSION_MAJOR EQUAL 3)
                target_link_libraries(render-gui PRIVATE SFML::Graphics SFML::Window SFML::System)
            else()
                target_link_libraries(render-gui PRIVATE sfml-graphics sfml-window sfml-system)
            endif()
        endif()
    endif()
endif()

# ---------------- Qt (Qt5 or Qt6) ----------------
if(ENABLE_QT)
    find_package(Qt6 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        add_executable(render-qt src/apps/render_qt.cpp)
        target_include_directories(render-qt PRIVATE src)
        target_link_libraries(render-qt PRIVATE core Qt6::Widgets)
    else()
        find_package(Qt5 QUIET COMPONENTS Widgets)
        if(Qt5_FOUND)
            add_executable(render-qt src/apps/render_qt.cpp)
            target_include_directories(render-qt PRIVATE src)
            target_link_libraries(render-qt PRIVATE core Qt5::Widgets)
        endif()
    endif()
endif()
