#include "vr_events.h"

#include <stdio.h>

#include "../client/client.h"
#include "vr_macros.h"
#include "vr_gameplay.h"
#include "vr_session.h"

void _VR_HandleSessionStateChange(VR_App* app, XrSessionState newState);

XrBool32 VR_ProcessXrEvents(VR_App* app)
{
	XrEventDataBuffer eventDataBuffer = {};
	GLboolean recenter = GL_FALSE;

	// Poll for events
	for (;;)
	{
		XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
		baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
		baseEventHeader->next = NULL;
		XrResult r;
		OXR(r = xrPollEvent(app->Instance, &eventDataBuffer));
		if (r != XR_SUCCESS)
		{
			break;
		}

		switch (baseEventHeader->type)
		{
			case XR_TYPE_EVENT_DATA_EVENTS_LOST:
				XrEventDataEventsLost *eventsLost = (XrEventDataEventsLost*)baseEventHeader;
				printf("[OpenXR][EVT_EVENTS_LOST] Lost %u events\n", eventsLost->lostEventCount);
				break;

			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
				const XrEventDataInstanceLossPending* instance_loss_pending_event = (XrEventDataInstanceLossPending*)(baseEventHeader);
				printf("[OpenXR][EVT_INSTANCE_LOSS_PENDING] Shutting down at %lld...\n", instance_loss_pending_event->lossTime);
				// TODO(ripper37): we should probably quit here?
			} break;

			case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
				printf("[OpenXR][EVT_INTERACTION_PROFILE_CHANGED]\n");
				break;

			case XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB: {
				const XrEventDataDisplayRefreshRateChangedFB* refresh_rate_changed_event = (XrEventDataDisplayRefreshRateChangedFB*)(baseEventHeader);
				printf("[OpenXR][EVT_DISPLAY_REFRESH_RATE_CHANGED] Update %f => %f\n",
					refresh_rate_changed_event->fromDisplayRefreshRate, refresh_rate_changed_event->toDisplayRefreshRate);
				app->Renderer.RefreshRate = refresh_rate_changed_event->toDisplayRefreshRate;
				Cvar_SetValue("vr_refreshrate", refresh_rate_changed_event->toDisplayRefreshRate);
			} break;

			case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
				XrEventDataReferenceSpaceChangePending* ref_space_change_event = (XrEventDataReferenceSpaceChangePending*)(baseEventHeader);
				printf("[OpenXR][EVT_REFERENCE_SPACE_CHANGE_PENDING] Updated space type %d\n", ref_space_change_event->referenceSpaceType);
				if (ref_space_change_event->session == app->Session)
				{
					recenter = GL_TRUE;
				}
			} break;

			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
				const XrEventDataSessionStateChanged* session_state_changed_event = (XrEventDataSessionStateChanged*)(baseEventHeader);
				printf("[OpenXR][EVT_SESSION_STATE_CHANGED] Session state changed: %d\n",
					session_state_changed_event->state);
				_VR_HandleSessionStateChange(app, session_state_changed_event->state);
			} break;

			default:
				printf("xrPollEvent: Unknown event\n");
				break;
		}
	}
	return recenter;
}

void _VR_HandleSessionStateChange(VR_App* app, XrSessionState newState)
{
	switch (newState)
	{
		case XR_SESSION_STATE_FOCUSED:
			app->Focused = XR_TRUE;
			app->Visible = XR_TRUE;
			break;

		case XR_SESSION_STATE_VISIBLE:
			app->Focused = XR_FALSE;
			app->Visible = XR_TRUE;
			break;

		case XR_SESSION_STATE_SYNCHRONIZED:
			if (app->Visible)
			{
				// Loss of visibility, let's pause the game
				VR_Gameplay_OpenMenuAndPauseIfPossible();
			}
			app->Visible = XR_FALSE;
			break;

		case XR_SESSION_STATE_READY:
			app->Visible = XR_FALSE;
			CHECK(!app->SessionActive, "");
			XR_CHECK(VR_BeginSession(app->Session), "Failed to begin XR session");
			app->SessionActive = XR_TRUE;
			break;

		case XR_SESSION_STATE_STOPPING:
			app->Visible = XR_FALSE;
			CHECK(app->SessionActive, "");
			XR_CHECK(VR_EndSession(app->Session), "Failed to end XR session");
			app->SessionActive = XR_FALSE;
			break;

		default:
			break;
	}
}
