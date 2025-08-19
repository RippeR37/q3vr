if(NOT BUILD_CLIENT)
    return()
endif()

find_package(OpenXR CONFIG REQUIRED)

list(APPEND CLIENT_LIBRARIES OpenXR::openxr_loader)
list(APPEND COMMON_LIBRARIES OpenXR::headers)
