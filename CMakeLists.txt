cmake_minimum_required(VERSION 3.16)
project(lgl-system-loadout VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

qt_add_executable(lgl-system-loadout
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/pagehelpers.h
    ${PROJECT_SOURCE_DIR}/src/mainwizard.cpp            ${PROJECT_SOURCE_DIR}/src/mainwizard.h
    ${PROJECT_SOURCE_DIR}/src/installworker.cpp         ${PROJECT_SOURCE_DIR}/src/installworker.h
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
    ${PROJECT_SOURCE_DIR}/src/pages/themingpage.cpp     ${PROJECT_SOURCE_DIR}/src/pages/themingpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/cachyospage.cpp     ${PROJECT_SOURCE_DIR}/src/pages/cachyospage.h
    ${PROJECT_SOURCE_DIR}/src/pages/reviewpage.cpp      ${PROJECT_SOURCE_DIR}/src/pages/reviewpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/installpage.cpp     ${PROJECT_SOURCE_DIR}/src/pages/installpage.h
    ${PROJECT_SOURCE_DIR}/src/pages/donepage.cpp        ${PROJECT_SOURCE_DIR}/src/pages/donepage.h
)

target_link_libraries(lgl-system-loadout PRIVATE Qt6::Core Qt6::Widgets Qt6::Concurrent)

install(TARGETS lgl-system-loadout RUNTIME DESTINATION /usr/local/bin)
