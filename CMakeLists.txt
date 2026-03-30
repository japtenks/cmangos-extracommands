#
# ExtraCommands Module
# Adds extended GM commands not present in the base CMaNGOS core.
#
set(LIBRARY_NAME extracommands)

file(GLOB extracommands_source
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)
source_group("extracommands" FILES ${extracommands_source})

set(LIBRARY_SRCS ${extracommands_source})

include_directories(${CMAKE_SOURCE_DIR}/src/modules/modules/src)
add_definitions(-DENABLE_MODULES)
add_definitions(-DENABLE_EXTRACOMMANDS)

# Playerbots support — enables botstrat, botdbreset, nearbystrategies, live guildflavor
if(BUILD_PLAYERBOTS)
    include_directories(${CMAKE_SOURCE_DIR}/src/modules/playerbot)
    add_definitions(-DHAVE_PLAYERBOTS)
endif()

# Expansion flags
if(${CMAKE_PROJECT_NAME} MATCHES "Classic")
    add_definitions(-DEXPANSION=0)
endif()
if(${CMAKE_PROJECT_NAME} MATCHES "TBC")
    add_definitions(-DEXPANSION=1)
endif()
if(${CMAKE_PROJECT_NAME} MATCHES "WoTLK")
    add_definitions(-DEXPANSION=2)
endif()

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SRCS})
target_link_libraries(${LIBRARY_NAME}
    modules
    shared
    RecastNavigation::Detour
)

if(UNIX)
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD"))
        target_link_libraries(${LIBRARY_NAME} dl)
    endif()
endif()

set_target_properties(${LIBRARY_NAME} PROPERTIES PROJECT_LABEL "extracommands")
set_target_properties(${LIBRARY_NAME} PROPERTIES FOLDER "Modules")

# Install config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/extracommands.conf.dist.in
    ${CMAKE_CURRENT_BINARY_DIR}/extracommands.conf.dist
)
if(NOT CONF_INSTALL_DIR)
    set(CONF_INSTALL_DIR ${CONF_DIR})
endif()
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/extracommands.conf.dist DESTINATION ${CONF_INSTALL_DIR})
