include_guard(GLOBAL)

set(VR_SOURCES
    ${SOURCE_DIR}/vr/vr_base.c
    ${SOURCE_DIR}/vr/vr_cvars.c
    ${SOURCE_DIR}/vr/vr_debug.c
    ${SOURCE_DIR}/vr/vr_events.c
    ${SOURCE_DIR}/vr/vr_gameplay.c
    ${SOURCE_DIR}/vr/vr_haptics.c
    ${SOURCE_DIR}/vr/vr_bhaptics.c
    ${SOURCE_DIR}/vr/vr_input.c
    ${SOURCE_DIR}/vr/vr_instance.c
    ${SOURCE_DIR}/vr/vr_math.c
    ${SOURCE_DIR}/vr/vr_render_loop.c
    ${SOURCE_DIR}/vr/vr_renderer.c
    ${SOURCE_DIR}/vr/vr_session.c
    ${SOURCE_DIR}/vr/vr_spaces.c
    ${SOURCE_DIR}/vr/vr_swapchains.c
    ${SOURCE_DIR}/vr/vr_virtual_screen.c
)

find_package(OpenGL REQUIRED)
list(APPEND VR_LIBRARIES ${OPENGL_LIBRARIES})
