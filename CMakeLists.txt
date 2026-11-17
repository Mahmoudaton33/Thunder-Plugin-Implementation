cmake_minimum_required(VERSION 3.3)
project(HelloWorld)

find_package(WPEFramework)

set(PLUGIN_NAME HelloWorld)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_HELLOWORLD_AUTOSTART "false" CACHE STRING "Activate the plugin when WPEFramework starts")
set(PLUGIN_HELLOWORLD_STARTUPORDER "" CACHE STRING "Activation order among autostart plugins")
set(PLUGIN_HELLOWORLD_GREETING "Hello" CACHE STRING "Default greeting word")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
    HelloWorld.cpp
    Module.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

string(TOLOWER ${NAMESPACE} STORAGE_DIRECTORY)
install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})