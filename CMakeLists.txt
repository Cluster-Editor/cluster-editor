cmake_minimum_required(VERSION 3.20)

project(collab_editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COLLAB_EDITOR_BUILD_TESTS "Build the test suite" ON)

add_library(collab_editor_warnings INTERFACE)
target_compile_options(collab_editor_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_executable(collab_server
    server/src/main.cpp
)
target_include_directories(collab_server PRIVATE server/include)
target_link_libraries(collab_server PRIVATE collab_editor_warnings)

include(GNUInstallDirs)
install(TARGETS collab_server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(COLLAB_EDITOR_BUILD_TESTS)
    enable_testing()

    add_executable(collab_smoke_test
        server/tests/smoke_test.cpp
    )
    target_include_directories(collab_smoke_test PRIVATE server/include)
    target_link_libraries(collab_smoke_test PRIVATE collab_editor_warnings)

    add_test(NAME collab_smoke_test COMMAND collab_smoke_test)
endif()
