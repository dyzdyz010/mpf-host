cmake_minimum_required(VERSION 3.21)
project(mpf-host-tests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Enable testing
enable_testing()

# Find dependencies
find_package(Qt6 REQUIRED COMPONENTS Core Test)
find_package(MPF REQUIRED)

# Event Bus Service sources (from parent) - include header for AUTOMOC
set(EVENT_BUS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/event_bus_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/event_bus_service.h
)

# Test: EventBus
add_executable(test_event_bus
    test_event_bus.cpp
    ${EVENT_BUS_SOURCES}
)

target_include_directories(test_event_bus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(test_event_bus PRIVATE
    Qt6::Core
    Qt6::Test
    MPF::foundation-sdk
)

# Register test with CTest
add_test(NAME EventBusTest COMMAND test_event_bus)

# Verbose output on failure
set_tests_properties(EventBusTest PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAIL!"
)

# Plugin Dependencies Test
set(PLUGIN_DEPS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin_metadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/plugin_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/plugin_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/plugin_metadata.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/service_registry.h
)

add_executable(test_plugin_dependencies
    test_plugin_dependencies.cpp
    ${PLUGIN_DEPS_SOURCES}
)

target_include_directories(test_plugin_dependencies PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(test_plugin_dependencies PRIVATE
    Qt6::Core
    Qt6::Test
    MPF::foundation-sdk
)

add_test(NAME PluginDependenciesTest COMMAND test_plugin_dependencies)

set_tests_properties(PluginDependenciesTest PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAIL!"
)
