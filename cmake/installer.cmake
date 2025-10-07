get_directory_property(INSTALL_TARGETS DIRECTORY
    ${CMAKE_SOURCE_DIR} BUILDSYSTEM_TARGETS)

# Iterate over all the targets that have an INSTALL_DESTINATION
# property (set by set_output_dirs) and call install() on them
foreach(TARGET IN LISTS INSTALL_TARGETS)
    get_target_property(DESTINATION ${TARGET} INSTALL_DESTINATION)
    if(NOT DESTINATION)
        continue()
    endif()

    install(TARGETS ${TARGET}
        RUNTIME DESTINATION ${DESTINATION} COMPONENT game_engine
        LIBRARY DESTINATION ${DESTINATION} COMPONENT game_engine
        BUNDLE DESTINATION ${DESTINATION} COMPONENT game_engine
        ARCHIVE EXCLUDE_FROM_ALL)
endforeach()

if(NOT CPACK_GENERATOR)
    return()
endif()

set(CPACK_PACKAGE_NAME "Quake 3 VR")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Quake 3 VR")
set(CPACK_PACKAGE_EXECUTABLES ${CLIENT_NAME} ${PROJECT_NAME})
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${PRODUCT_VERSION}-installer)
set(CPACK_PACKAGE_CONTACT ${CONTACT_EMAIL})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/COPYING.txt)

set(CPACK_COMPONENTS_ALL
		game_engine
		point_release_patch
		q3a_demo)

set(CPACK_COMPONENT_GAME_ENGINE_DISPLAY_NAME "Quake 3 VR engine")
set(CPACK_COMPONENT_GAME_ENGINE_DESCRIPTION "Game engine binaries and data files.\n\nRequires assets from the full game or the demo to run.")
set(CPACK_COMPONENT_GAME_ENGINE_REQUIRED TRUE)
set(CPACK_COMPONENT_GAME_ENGINE_HIDDEN FALSE)

set(CPACK_COMPONENT_POINT_RELEASE_PATCH_DISPLAY_NAME "Patch v1.32")
set(CPACK_COMPONENT_POINT_RELEASE_PATCH_DESCRIPTION "Recommended data files for Release Point patch (v1.32). Licensed under EULA.\n\nRequired for multiplayer.")
set(CPACK_COMPONENT_POINT_RELEASE_PATCH_DISABLED FALSE)

set(CPACK_COMPONENT_Q3A_DEMO_DISPLAY_NAME "Demo assets")
set(CPACK_COMPONENT_Q3A_DEMO_DESCRIPTION "Optional data files for Quake 3 Arena demo.\n\nDo NOT install if you own full version of the game.")
set(CPACK_COMPONENT_Q3A_DEMO_DISABLED TRUE)

include(CPack)
