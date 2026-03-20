cmake_minimum_required(VERSION 3.16)
project(lgl-system-loadout VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# C++17 is used rather than C++20 (the preferred standard) because compiling
# Qt6 headers in C++20 mode activates C++20-specific Qt code paths that
# reference symbols only present in Qt 6.10+, breaking compatibility with
# the Qt6 version shipped on Fedora 43 target systems.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Treat warnings as errors on GCC and Clang (dev standard requirement).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Enable AddressSanitizer and UndefinedBehaviorSanitizer for Debug builds.
# ThreadSanitizer is mutually exclusive with ASan — use -DCMAKE_BUILD_TYPE=TSan.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "TSan")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-g -O1 -fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()
endif()

# ---------------------------------------------------------------------------
# GUI application — Qt Widgets, runs as the normal user (never root).
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

qt_add_executable(lgl-system-loadout
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/pagehelpers.h
    ${PROJECT_SOURCE_DIR}/src/mainwizard.cpp            ${PROJECT_SOURCE_DIR}/src/mainwizard.h
    ${PROJECT_SOURCE_DIR}/src/installworker.cpp         ${PROJECT_SOURCE_DIR}/src/installworker.h
    ${PROJECT_SOURCE_DIR}/src/pages/updatepage.cpp      ${PROJECT_SOURCE_DIR}/src/pages/updatepage.h
    ${PROJECT_SOURCE_DIR}/src/pages/welcomepage.cpp     ${PROJECT_SOURCE_DIR}/src/pages/welcomepage.h
    ${PROJECT_SOURCE_DIR}/src/pages/repospage.cpp       ${PROJECT_SOURCE_DIR}/src/pages/repospage.h
    ${PROJECT_SOURCE_DIR}/src/pages/systemtoolspage.cpp ${PROJECT_SOURCE_DIR}/src/pages/systemtoolspage.h
    ${PROJECT_SOURCE_DIR}/src/pages/pythonpage.cpp      ${PROJECT_SOURCE_DIR}/src/pages/pythonpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/multimediapage.cpp  ${PROJECT_SOURCE_DIR}/src/pages/multimediapage.h
    ${PROJECT_SOURCE_DIR}/src/pages/contentpage.cpp     ${PROJECT_SOURCE_DIR}/src/pages/contentpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/gpupage.cpp         ${PROJECT_SOURCE_DIR}/src/pages/gpupage.h
    ${PROJECT_SOURCE_DIR}/src/pages/gamingpage.cpp      ${PROJECT_SOURCE_DIR}/src/pages/gamingpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/virtpage.cpp        ${PROJECT_SOURCE_DIR}/src/pages/virtpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/browserspage.cpp    ${PROJECT_SOURCE_DIR}/src/pages/browserspage.h
    ${PROJECT_SOURCE_DIR}/src/pages/commspage.cpp       ${PROJECT_SOURCE_DIR}/src/pages/commspage.h
    ${PROJECT_SOURCE_DIR}/src/pages/cachyospage.cpp     ${PROJECT_SOURCE_DIR}/src/pages/cachyospage.h
    ${PROJECT_SOURCE_DIR}/src/pages/scxpage.cpp         ${PROJECT_SOURCE_DIR}/src/pages/scxpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/reviewpage.cpp      ${PROJECT_SOURCE_DIR}/src/pages/reviewpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/installpage.cpp  ${PROJECT_SOURCE_DIR}/src/pages/installpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/donepage.cpp        ${PROJECT_SOURCE_DIR}/src/pages/donepage.h
)

target_link_libraries(lgl-system-loadout PRIVATE Qt6::Core Qt6::Widgets Qt6::Concurrent Qt6::Network)

install(TARGETS lgl-system-loadout RUNTIME DESTINATION /usr/local/bin)

# ---------------------------------------------------------------------------
# Privileged helper — Qt Core only, no Widgets dependency.
# Installed to /usr/libexec/lgl-system-loadout/ (FHS location for helpers).
# Launched exclusively via pkexec — never run directly by the user.
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Network)

qt_add_executable(lgl-system-loadout-helper
    ${PROJECT_SOURCE_DIR}/src/helper/main.cpp
    ${PROJECT_SOURCE_DIR}/src/helper/helperserver.cpp   ${PROJECT_SOURCE_DIR}/src/helper/helperserver.h
    ${PROJECT_SOURCE_DIR}/src/helper/commandexecutor.cpp ${PROJECT_SOURCE_DIR}/src/helper/commandexecutor.h
)

target_link_libraries(lgl-system-loadout-helper PRIVATE Qt6::Core Qt6::Network)

install(TARGETS lgl-system-loadout-helper
        RUNTIME DESTINATION /usr/libexec/lgl-system-loadout)

# ---------------------------------------------------------------------------
# Tests — require Qt6::Test; skipped automatically if the component is absent.
# Build:  cmake .. -DCMAKE_BUILD_TYPE=Debug && make && ctest --output-on-failure
# ---------------------------------------------------------------------------
find_package(Qt6 QUIET COMPONENTS Test)
if(Qt6Test_FOUND)
    enable_testing()

    qt_add_executable(tst_installworker
        ${PROJECT_SOURCE_DIR}/tests/tst_installworker.cpp
        ${PROJECT_SOURCE_DIR}/src/installworker.cpp
        ${PROJECT_SOURCE_DIR}/src/installworker.h
    )
    target_include_directories(tst_installworker PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(tst_installworker PRIVATE Qt6::Core Qt6::Test Qt6::Network)

    add_test(NAME InstallWorker COMMAND tst_installworker)
    set_tests_properties(InstallWorker PROPERTIES TIMEOUT 60)
else()
    message(STATUS "Qt6::Test not found — test targets skipped. "
                   "Install qt6-qtbase-devel to enable.")
endif()
