if(NOT BUILD_CLIENT)
    return()
endif()

option(USE_BHAPTICS "Enable bHaptics support" ON)

if(NOT USE_BHAPTICS)
    return()
endif()

set(BHAPTICS_LIB_DIR ${CMAKE_SOURCE_DIR}/code/thirdparty/libs)

if(WIN32)
    set(BHAPTICS_DLL ${BHAPTICS_LIB_DIR}/win64/haptic_library.dll)
    set(BHAPTICS_LIB ${BHAPTICS_LIB_DIR}/win64/haptic_library.lib)
    if(NOT EXISTS ${BHAPTICS_DLL} OR NOT EXISTS ${BHAPTICS_LIB})
        message(WARNING "bHaptics Win64 libraries not found in code/thirdparty/libs/win64; disabling bHaptics support")
        return()
    endif()

    add_library(bhaptics SHARED IMPORTED)
    set_target_properties(bhaptics PROPERTIES
        IMPORTED_IMPLIB ${BHAPTICS_LIB}
        IMPORTED_LOCATION ${BHAPTICS_DLL})

    list(APPEND CLIENT_DEPLOY_LIBRARIES ${BHAPTICS_DLL})
elseif(UNIX)
    set(BHAPTICS_SO ${BHAPTICS_LIB_DIR}/linux/libhaptic_library.so)
    if(NOT EXISTS ${BHAPTICS_SO})
        message(WARNING "bHaptics Linux library not found in code/thirdparty/libs/linux; disabling bHaptics support")
        return()
    endif()

    add_library(bhaptics SHARED IMPORTED)
    set_target_properties(bhaptics PROPERTIES
        IMPORTED_LOCATION ${BHAPTICS_SO})

    list(APPEND CLIENT_DEPLOY_LIBRARIES ${BHAPTICS_SO})
else()
    message(WARNING "Unsupported platform for bHaptics; disabling bHaptics support")
    return()
endif()

list(APPEND VR_LIBRARIES bhaptics)
list(APPEND CLIENT_DEFINITIONS USE_BHAPTICS)
