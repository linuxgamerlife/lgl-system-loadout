cmake_minimum_required(VERSION 3.20)
project(lgl-system-loadout VERSION 3.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

# build/<app-name>, regardless of where a target is declared.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
                        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic> $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif()

option(LOADOUT_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(LOADOUT_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.20)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.6)
pkg_check_modules(GLIBMM REQUIRED IMPORTED_TARGET glibmm-2.68)
pkg_check_modules(JSON_GLIB REQUIRED IMPORTED_TARGET json-glib-1.0)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

# ---------------------------------------------------------------------------
# Resources
# ---------------------------------------------------------------------------
set(RESOURCE_XML ${PROJECT_SOURCE_DIR}/data/loadout.gresource.xml)
set(RESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/loadout-resources.c)
file(GLOB RESOURCE_ICON_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/data/icons/*.png
    ${PROJECT_SOURCE_DIR}/data/icons/*.svg)
set(RESOURCE_DEPS
    ${PROJECT_SOURCE_DIR}/data/style.css
    ${PROJECT_SOURCE_DIR}/HELP.md
    ${RESOURCE_ICON_FILES}
    ${PROJECT_SOURCE_DIR}/packaging/icons/256x256/lgl-system-loadout.png)
add_custom_command(OUTPUT ${RESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target=${RESOURCE_C}
            --sourcedir=${PROJECT_SOURCE_DIR}/data --sourcedir=${PROJECT_SOURCE_DIR} ${RESOURCE_XML}
    DEPENDS ${RESOURCE_XML} ${RESOURCE_DEPS} VERBATIM)
set_source_files_properties(${RESOURCE_C} PROPERTIES COMPILE_OPTIONS "-w")

# ---------------------------------------------------------------------------
# Protocol: shared by the app and the helper; no GLib dependency beyond
# JSON-GLib, so the helper stays free of GTK and glibmm.
# ---------------------------------------------------------------------------
add_library(loadout_protocol STATIC
    src/core/policy.cpp    src/core/policy.h
    src/core/protocol.cpp  src/core/protocol.h)
target_include_directories(loadout_protocol PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(loadout_protocol PUBLIC PkgConfig::JSON_GLIB)

# ---------------------------------------------------------------------------
# Core: no widget headers; linked by the application and by the tests.
# ---------------------------------------------------------------------------
add_library(loadout_core STATIC
    src/core/catalog.cpp   src/core/catalog.h
    src/core/plan.cpp      src/core/plan.h
    src/core/process.cpp   src/core/process.h
    src/core/selection.cpp src/core/selection.h
    src/core/settings.cpp  src/core/settings.h
    src/core/system.cpp    src/core/system.h
    src/core/theme.cpp     src/core/theme.h)
target_link_libraries(loadout_core PUBLIC loadout_protocol PkgConfig::GLIBMM Threads::Threads)

# ---------------------------------------------------------------------------
# Privileged helper: launched only through pkexec.
# ---------------------------------------------------------------------------
add_executable(lgl-system-loadout-helper
    src/helper/main.cpp
    src/helper/server.cpp   src/helper/server.h
    src/helper/executor.cpp src/helper/executor.h)
target_link_libraries(lgl-system-loadout-helper PRIVATE loadout_protocol)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(lgl-system-loadout
    ${RESOURCE_C}
    src/main.cpp
    src/ui/themewatcher.cpp src/ui/themewatcher.h
    src/ui/helpwindow.cpp   src/ui/helpwindow.h
    src/ui/itemcard.cpp      src/ui/itemcard.h
    src/ui/cardgrid.cpp      src/ui/cardgrid.h
    src/ui/checker.cpp      src/ui/checker.h
    src/ui/helpersession.cpp src/ui/helpersession.h
    src/ui/page.cpp          src/ui/page.h
    src/ui/pages/welcomepage.cpp src/ui/pages/welcomepage.h
    src/ui/pages/choicepage.cpp  src/ui/pages/choicepage.h
    src/ui/pages/selectpage.cpp  src/ui/pages/selectpage.h
    src/ui/pages/gpupage.cpp     src/ui/pages/gpupage.h
    src/ui/pages/updatepage.cpp  src/ui/pages/updatepage.h
    src/ui/pages/reviewpage.cpp  src/ui/pages/reviewpage.h
    src/ui/pages/installpage.cpp src/ui/pages/installpage.h
    src/ui/pages/donepage.cpp    src/ui/pages/donepage.h
    src/ui/window.cpp       src/ui/window.h)
target_compile_definitions(lgl-system-loadout PRIVATE
    LOADOUT_VERSION="${PROJECT_VERSION}"
    LOADOUT_HELPER_PATH="${CMAKE_INSTALL_FULL_LIBEXECDIR}/lgl-system-loadout/lgl-system-loadout-helper")
target_link_libraries(lgl-system-loadout PRIVATE
    loadout_core PkgConfig::GTKMM PkgConfig::ADWAITA Threads::Threads)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS lgl-system-loadout RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS lgl-system-loadout-helper
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/lgl-system-loadout)
install(FILES packaging/com.linuxgamerlife.lgl-system-loadout.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/com.linuxgamerlife.lgl-system-loadout.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES packaging/lgl-system-loadout.policy
        DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions
        RENAME com.linuxgamerlife.lgl-system-loadout.run-helper.policy)
foreach(size 48 64 128 256)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/icons/${size}x${size}/lgl-system-loadout.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps)
endforeach()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    foreach(module catalog plan policy protocol selection settings theme)
        add_executable(${module}_test tests/${module}_test.cpp)
        target_link_libraries(${module}_test PRIVATE loadout_core)
        add_test(NAME ${module}_test COMMAND ${module}_test)
        set_tests_properties(${module}_test PROPERTIES SKIP_RETURN_CODE 77)
    endforeach()
endif()
