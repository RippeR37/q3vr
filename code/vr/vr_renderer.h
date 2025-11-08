#ifndef __VR_RENDERER
#define __VR_RENDERER

#include "../qcommon/q_shared.h"

#include "vr_types.h"

void VR_GetResolution( VR_Engine* engine, int *pWidth, int *pHeight );

void VR_InitRenderer( VR_Engine* engine );
void VR_DestroyRenderer( VR_Engine* engine );

void VR_ProcessFrame( VR_Engine* engine );

// If the OpenXR instance is re-created during VR_ProcessFrame() this function
// will be called to restore/clean up any state that's expected to be set for
// Com_Frame() to proceed and will also update state needed for the rest of
// VR_ProcessFrame to proceed
void VR_Renderer_RestoreState( VR_Engine* engine );

// Submit VR frame during loading if needed (returns qtrue if a frame was submitted)
qboolean VR_Renderer_SubmitLoadingFrame(VR_Engine* engine);

#endif
