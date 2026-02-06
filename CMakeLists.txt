cmake_minimum_required(VERSION 3.21)
project(mpf-host VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find dependencies
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

# Set Qt policies to avoid warnings
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)  # QML resource prefix
    qt_policy(SET QTP0004 NEW)  # qmldir for extra directories
endif()

# SDK (installed via CMAKE_PREFIX_PATH)
find_package(MPF REQUIRED)

# UI Components - MUST be linked by host to avoid cross-DLL heap issues
# Plugins should NOT link this; they access it via QML_IMPORT_PATH
find_package(MPFUIComponents QUIET)
if(MPFUIComponents_FOUND)
    message(STATUS "Found MPFUIComponents - will be loaded by host")
endif()

# Generate version header
configure_file(
    cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/mpf/version.h
    @ONLY
)

# Generate SDK paths header
# MPF_PREFIX and MPF_QML_PATH are set by find_package(MPF)
if(MPF_QML_PATH AND EXISTS "${MPF_QML_PATH}")
    set(MPF_SDK_HAS_QML_PATH 1)
else()
    set(MPF_SDK_HAS_QML_PATH 0)
endif()

configure_file(
    cmake/sdk_paths.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/mpf/sdk_paths.h
    @ONLY
)

message(STATUS "MPF SDK prefix: ${MPF_PREFIX}")
message(STATUS "MPF SDK QML path: ${MPF_QML_PATH}")

# Application
add_executable(mpf-host
    # Main
    src/main.cpp
    src/application.cpp
    
    # Core (moved from SDK)
    src/service_registry.cpp
    src/logger.cpp
    src/plugin_metadata.cpp
    
    # Services
    src/plugin_manager.cpp
    src/plugin_loader.cpp
    src/navigation_service.cpp
    src/settings_service.cpp
    src/theme_service.cpp
    src/menu_service.cpp
    src/event_bus_service.cpp
    src/qml_context.cpp
    
    # Headers
    include/application.h
    include/service_registry.h
    include/logger.h
    include/plugin_metadata.h
    include/plugin_manager.h
    include/plugin_loader.h
    include/navigation_service.h
    include/settings_service.h
    include/theme_service.h
    include/menu_service.h
    include/event_bus_service.h
    include/qml_context.h
)

target_include_directories(mpf-host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(mpf-host PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    MPF::foundation-sdk
    # UI Components - link here to ensure single heap for QML components
    $<$<TARGET_EXISTS:MPF::mpf-ui-components>:MPF::mpf-ui-components>
)

# QML files - just add new files to this list
set(HOST_QML_FILES
    qml/Main.qml
    qml/SideMenu.qml
    qml/MenuItemCustom.qml
    qml/ErrorDialog.qml
)

set(HOST_RESOURCES
    qml/images/logo.svg
)

# Auto-flatten paths: qml/Foo.qml -> Foo.qml, qml/sub/Bar.qml -> sub/Bar.qml
foreach(file ${HOST_QML_FILES} ${HOST_RESOURCES})
    string(REGEX REPLACE "^qml/" "" alias "${file}")
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias})
endforeach()

qt_add_qml_module(mpf-host
    URI MPF.Host
    VERSION 1.0
    RESOURCE_PREFIX /
    QML_FILES ${HOST_QML_FILES}
    RESOURCES ${HOST_RESOURCES}
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/MPF/Host
)

# Output directories  
set_target_properties(mpf-host PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create output directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config)

# ============================================
# Development environment configuration
# ============================================
# Auto-detect SDK path for Qt Creator debugging
# Priority: MPF_SDK_ROOT env var > ~/.mpf-sdk/current

# Find user home directory
if(WIN32)
    set(USER_HOME "$ENV{USERPROFILE}")
    string(REPLACE "\\" "/" USER_HOME "${USER_HOME}")
else()
    set(USER_HOME "$ENV{HOME}")
endif()

set(MPF_SDK_DEFAULT "${USER_HOME}/.mpf-sdk/current")

# Check if SDK exists at default location
if(EXISTS "${MPF_SDK_DEFAULT}/bin")
    message(STATUS "Found MPF SDK at: ${MPF_SDK_DEFAULT}")
    
    # For Visual Studio: set debugger environment
    if(MSVC)
        set_target_properties(mpf-host PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "MPF_SDK_ROOT=${MPF_SDK_DEFAULT}\nPATH=${MPF_SDK_DEFAULT}/bin;${MPF_SDK_DEFAULT}/lib;%PATH%\nQML_IMPORT_PATH=${MPF_SDK_DEFAULT}/qml"
        )
    endif()
    
    # Generate Qt Creator environment file
    # User can copy these to: Projects -> Run -> Environment
    file(WRITE ${CMAKE_BINARY_DIR}/qt-creator-env.txt
"# Qt Creator Run Environment Configuration
# Copy these to: Projects -> Run -> Environment -> Add
#
# For Windows:
MPF_SDK_ROOT=${MPF_SDK_DEFAULT}
PATH=${MPF_SDK_DEFAULT}/bin;${MPF_SDK_DEFAULT}/lib;\${PATH}
QML_IMPORT_PATH=${MPF_SDK_DEFAULT}/qml

# If you linked plugins via mpf-dev, also add:
# MPF_PLUGIN_PATH=<your-plugin-build>/plugins/mpf
# QML_IMPORT_PATH=<your-plugin-build>/qml;${MPF_SDK_DEFAULT}/qml
")
    message(STATUS "Generated Qt Creator env file: ${CMAKE_BINARY_DIR}/qt-creator-env.txt")
else()
    message(STATUS "MPF SDK not found at ${MPF_SDK_DEFAULT}")
    message(STATUS "Install SDK with: mpf-dev setup")
endif()

# ============================================
# Install
# ============================================
install(TARGETS mpf-host
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/qml/
    DESTINATION qml
)
