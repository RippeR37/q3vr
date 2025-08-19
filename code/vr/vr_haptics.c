#include "vr_haptics.h"

#include "../qcommon/qcommon.h"

#include "vr_base.h"
#include "vr_types.h"

extern cvar_t *vr_hapticIntensity;
extern XrAction vibrateLeftFeedback;
extern XrAction vibrateRightFeedback;

qboolean VR_AreHapticsEnabled(void)
{
	return VR_GetHapticsIntensity() > 0.1f ? qtrue : qfalse;
}

float VR_GetHapticsIntensity(void)
{
	return vr_hapticIntensity->value;
}

//0 = left, 1 = right
float vibration_channel_duration[2] = {0.0f, 0.0f};
float vibration_channel_intensity[2] = {0.0f, 0.0f};

void VR_Vibrate(int duration, int chan, float intensity)
{
	for (int i = 0; i < 2; ++i)
	{
		int channel = (i + 1) & chan;
		if (channel)
		{
			if (vibration_channel_duration[channel-1] > 0.0f)
				return;

			if (vibration_channel_duration[channel-1] == -1.0f && duration != 0.0f)
				return;

			vibration_channel_duration[channel-1] = duration;
			vibration_channel_intensity[channel-1] = intensity * VR_GetHapticsIntensity();
		}
	}
}

void VR_ProcessHaptics(void)
{
	const XrSession session = VR_GetEngine()->appState.Session;

	static float lastFrameTime = 0.0f;
	float timestamp = (float)(Sys_Milliseconds( ));
	float frametime = timestamp - lastFrameTime;
	lastFrameTime = timestamp;

	const int64_t miliToNano = 1000 * 1000;

	for (int i = 0; i < 2; ++i)
	{
		if (vibration_channel_duration[i] > 0.0f || vibration_channel_duration[i] == -1.0f) 
		{
			// fire haptics using output action
			XrHapticVibration vibration = {};
			vibration.type = XR_TYPE_HAPTIC_VIBRATION;
			vibration.next = NULL;
			vibration.amplitude = vibration_channel_intensity[i];
			vibration.duration = vibration_channel_duration[i] * miliToNano;
			vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
			XrHapticActionInfo hapticActionInfo = {};
			hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
			hapticActionInfo.next = NULL;
			hapticActionInfo.action = i == 0 ? vibrateLeftFeedback : vibrateRightFeedback;
			OXR(xrApplyHapticFeedback(session, &hapticActionInfo, (const XrHapticBaseHeader*)&vibration));

			if (vibration_channel_duration[i] != -1.0f)
			{
				vibration_channel_duration[i] -= frametime;

				if (vibration_channel_duration[i] < 0.0f)
				{
					vibration_channel_duration[i] = 0.0f;
					vibration_channel_intensity[i] = 0.0f;
				}
			}
		}
		else
		{
			// Stop haptics
			XrHapticActionInfo hapticActionInfo = {};
			hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
			hapticActionInfo.next = NULL;
			hapticActionInfo.action = i == 0 ? vibrateLeftFeedback : vibrateRightFeedback;
			OXR(xrStopHapticFeedback(session, &hapticActionInfo));
		}
	}
}
