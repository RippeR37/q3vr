#include "vr_input.h"

#include "../qcommon/qcommon.h"
#include "../client/keycodes.h"
#include "../client/client.h"

#include "vr_base.h"
#include "vr_clientinfo.h"
#include "vr_gameplay.h"
#include "vr_haptics.h"
#include "vr_macros.h"
#include "vr_math.h"
#include "vr_virtual_screen.h"

#ifdef USE_INTERNAL_SDL
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include <openxr/openxr.h>
#include "common/xr_linear.h"

//OpenXR
XrPath leftHandPath;
XrPath rightHandPath;
XrAction handPoseLeftAction;
XrAction handPoseRightAction;
XrAction indexLeftAction;
XrAction indexRightAction;
XrAction menuAction;
XrAction buttonAAction;
XrAction buttonBAction;
XrAction buttonXAction;
XrAction buttonYAction;
XrAction gripLeftAction;
XrAction gripRightAction;
XrAction trackpadLeftAction;
XrAction trackpadRightAction;
XrAction moveOnLeftJoystickAction;
XrAction moveOnRightJoystickAction;
XrAction thumbstickLeftClickAction;
XrAction thumbstickRightClickAction;
XrAction thumbrestLeftTouchAction;
XrAction thumbrestRightTouchAction;
XrAction vibrateLeftFeedback;
XrAction vibrateRightFeedback;
XrActionSet runningActionSet;
XrSpace leftControllerAimSpace = XR_NULL_HANDLE;
XrSpace rightControllerAimSpace = XR_NULL_HANDLE;

qboolean inputInitialized = qfalse;
qboolean useSimpleProfile = qfalse;

enum {
	VR_TOUCH_AXIS_UP = 1 << 0,
	VR_TOUCH_AXIS_UPRIGHT = 1 << 1,
	VR_TOUCH_AXIS_RIGHT = 1 << 2,
	VR_TOUCH_AXIS_DOWNRIGHT = 1 << 3,
	VR_TOUCH_AXIS_DOWN = 1 << 4,
	VR_TOUCH_AXIS_DOWNLEFT = 1 << 5,
	VR_TOUCH_AXIS_LEFT = 1 << 6,
	VR_TOUCH_AXIS_UPLEFT = 1 << 7,
	VR_TOUCH_AXIS_TRIGGER_INDEX = 1 << 8,
};

typedef struct {
	uint32_t buttons;
	uint32_t axisButtons;
} vrController_t;

extern vr_clientinfo_t vr;

static vrController_t leftController;
static vrController_t rightController;
static int in_vrEventTime = 0;
static double lastframetime = 0;

static float triggerPressedThreshold = 0.75f;
static float triggerReleasedThreshold = 0.5f;

static float thumbstickPressedThreshold = 0.5f;
static float thumbstickReleasedThreshold = 0.4f;

extern cvar_t *cl_sensitivity;
extern cvar_t *m_pitch;
extern cvar_t *m_yaw;

#ifndef EPSILON
#define EPSILON 0.001f
#endif

extern cvar_t *vr_righthanded;
extern cvar_t *vr_switchThumbsticks;
extern cvar_t *vr_snapturn;
extern cvar_t *vr_directionMode;
extern cvar_t *vr_weaponPitch;
extern cvar_t *vr_heightAdjust;
extern cvar_t *vr_twoHandedWeapons;
extern cvar_t *vr_refreshrate;
extern cvar_t *vr_weaponScope;
extern cvar_t *vr_hapticIntensity;

qboolean alt_key_mode_active = qfalse;

void rotateAboutOrigin(float x, float y, float rotation, vec2_t out)
{
	out[0] = cosf(DEG2RAD(-rotation)) * x + sinf(DEG2RAD(-rotation)) * y;
	out[1] = cosf(DEG2RAD(-rotation)) * y - sinf(DEG2RAD(-rotation)) * x;
}

static float length(float x, float y)
{
	return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

void NormalizeAngles(vec3_t angles)
{
	while (angles[0] >= 90) angles[0] -= 180;
	while (angles[1] >= 180) angles[1] -= 360;
	while (angles[2] >= 180) angles[2] -= 360;
	while (angles[0] < -90) angles[0] += 180;
	while (angles[1] < -180) angles[1] += 360;
	while (angles[2] < -180) angles[2] += 360;
}

void GetAnglesFromVectors(const XrVector3f forward, const XrVector3f right, const XrVector3f up, vec3_t angles)
{
	float sr, sp, sy, cr, cp, cy;

	sp = -forward.z;

	float cp_x_cy = forward.x;
	float cp_x_sy = forward.y;
	float cp_x_sr = -right.z;
	float cp_x_cr = up.z;

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (fabs(cy) > EPSILON)
	{
		cp = cp_x_cy / cy;
	}
	else if (fabs(sy) > EPSILON)
	{
		cp = cp_x_sy / sy;
	}
	else if (fabs(sr) > EPSILON)
	{
		cp = cp_x_sr / sr;
	}
	else if (fabs(cr) > EPSILON)
	{
		cp = cp_x_cr / cr;
	}
	else
	{
		cp = cos(asin(sp));
	}

	float pitch = atan2(sp, cp);

	angles[0] = pitch / (M_PI*2.f / 360.f);
	angles[1] = yaw / (M_PI*2.f / 360.f);
	angles[2] = roll / (M_PI*2.f / 360.f);

	NormalizeAngles(angles);
}

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out)
{
	XrMatrix4x4f mat;
	XrMatrix4x4f_CreateFromQuaternion(&mat, &q);

	if (rotation[0] != 0.0f || rotation[1] != 0.0f || rotation[2] != 0.0f)
	{
		XrMatrix4x4f rotation_matrix, multiply_result;
		XrMatrix4x4f_CreateRotation(&rotation_matrix, rotation[0], rotation[1], rotation[2]);
		XrMatrix4x4f_Multiply(&multiply_result, &mat, &rotation_matrix);
		mat = multiply_result;
	}

	XrVector4f v1 = {0, 0, -1, 0};
	XrVector4f v2 = {1, 0, 0, 0};
	XrVector4f v3 = {0, 1, 0, 0};

	XrVector4f forwardInVRSpace, rightInVRSpace, upInVRSpace;
	XrMatrix4x4f_TransformVector4f(&forwardInVRSpace, &mat, &v1);
	XrMatrix4x4f_TransformVector4f(&rightInVRSpace, &mat, &v2);
	XrMatrix4x4f_TransformVector4f(&upInVRSpace, &mat, &v3);

	XrVector3f forward = {-forwardInVRSpace.z, -forwardInVRSpace.x, forwardInVRSpace.y};
	XrVector3f right = {-rightInVRSpace.z, -rightInVRSpace.x, rightInVRSpace.y};
	XrVector3f up = {-upInVRSpace.z, -upInVRSpace.x, upInVRSpace.y};

	XrVector3f_Normalize(&forward);
	XrVector3f_Normalize(&right);
	XrVector3f_Normalize(&up);

	GetAnglesFromVectors(forward, right, up, out);
}

static qboolean IN_GetInputAction(const char* inputName, char* action)
{
	char cvarname[256];
	Com_sprintf(cvarname, 256, "vr_button_map_%s%s", inputName, alt_key_mode_active ? "_ALT" : "");
	char * val = Cvar_VariableString(cvarname);
	if (val && strlen(val) > 0)
	{
		Com_sprintf(action, 256, "%s", val);
		return qtrue;
	}

	//If we didn't find something for this input and the alt key is active, then see if the un-alt key has a function
	if (alt_key_mode_active)
	{
		Com_sprintf(cvarname, 256, "vr_button_map_%s", inputName);
		char * val = Cvar_VariableString(cvarname);
		if (val && strlen(val) > 0)
		{
			Com_sprintf(action, 256, "%s", val);
			return qtrue;
		}
	}

	return qfalse;
}

// Returns true in case active input should be auto-repeated (now only applicable for smooth-turn)
static qboolean IN_SendInputAction(const char* action, qboolean inputActive, float axisValue, qboolean thumbstickAxis)
{
	if (action)
	{
		//handle our special actions first
		if (strcmp(action, "blank") == 0)
		{
			// Empty function used to block alt fallback on unmapped alt buttons or
			// force 8-way mapping mode of thumbstick without assigning actual action
		}
		else if (strcmp(action, "+alt") == 0)
		{
			alt_key_mode_active = inputActive;
		}
		else if (strcmp(action, "+weapon_stabilise") == 0)
		{
			//stabilised weapon only triggered when controllers close enough (40cm) to each other
			if (inputActive)
			{
				vec3_t l;
				VectorSubtract(vr.weaponposition, vr.offhandposition, l);
				vr.weapon_stabilised = VectorLength(l) < 0.4f;
			}
			else
			{
				vr.weapon_stabilised = qfalse;
			}
		}
		else if (strcmp(action, "+weapon_select") == 0)
		{
			vr.weapon_select = inputActive;
			if (inputActive)
			{
				int selectorType = (int) Cvar_VariableValue("vr_weaponSelectorMode");
				vr.weapon_select_using_thumbstick = (selectorType == WS_HMD);
				vr.weapon_select_autoclose = vr.weapon_select_using_thumbstick && thumbstickAxis;
			}
			else
			{
				vr.weapon_select_using_thumbstick = qfalse;
				vr.weapon_select_autoclose = qfalse;
				Cbuf_AddText("weapon_select");
			}
		}
		else if (action[0] == '+')
		{
			char command[256];
			Com_sprintf(command, sizeof(command), "%s%s\n", inputActive ? "+" : "-", action + 1);
			Cbuf_AddText(command);
		}
		else if (inputActive)
		{
			if (strcmp(action, "turnleft") == 0)
			{
				if (vr_snapturn->integer > 0) // snap turn
				{
					int snap = 45;
					if (vr_snapturn->integer > 1)
					{
						snap = vr_snapturn->integer;
					}
					CL_SnapTurn(-snap);
				}
				else // yaw (smooth turn)
				{
					float value = (axisValue > 0.0f ? axisValue : 1.0f) * cl_sensitivity->value * m_yaw->value;
					Com_QueueEvent(in_vrEventTime, SE_MOUSE, -value, 0, 0, NULL);
					return qtrue;
				}
			}
			else if (strcmp(action, "turnright") == 0)
			{
				if (vr_snapturn->integer > 0) // snap turn
				{
					int snap = 45;
					if (vr_snapturn->integer > 1)
					{
						snap = vr_snapturn->integer;
					}
					CL_SnapTurn(snap);
				}
				else // yaw (smooth turn)
				{
					float value = (axisValue > 0.0f ? axisValue : 1.0f) * cl_sensitivity->value * m_yaw->value;
					Com_QueueEvent(in_vrEventTime, SE_MOUSE, value, 0, 0, NULL);
					return qtrue;
				}
			}
			else if (strcmp(action, "uturn") == 0)
			{
				CL_SnapTurn(180);
			}
			else
			{
				char command[256];
				Com_sprintf(command, sizeof(command), "%s\n", action);
				Cbuf_AddText(command);
			}
		}
	}
	return qfalse;
}

static void IN_ActivateInput(uint32_t * inputGroup, int inputFlag)
{
	*inputGroup |= inputFlag;
}

static void IN_DeactivateInput(uint32_t * inputGroup, int inputFlag)
{
	*inputGroup &= ~inputFlag;
}

static qboolean IN_InputActivated(uint32_t * inputGroup, int inputFlag)
{
	return (*inputGroup & inputFlag);
}

static void IN_HandleActiveInput(uint32_t * inputGroup, int inputFlag, char* inputName, float axisValue, qboolean thumbstickAxis)
{
	if (IN_InputActivated(inputGroup, inputFlag))
	{
		// Input is already in activated state, nothing to do
		return;
	}
	char action[256];
	if (IN_GetInputAction(inputName, action))
	{
		// Activate input action
		if (!IN_SendInputAction(action, qtrue, axisValue, thumbstickAxis))
		{
			// Action should not be repeated, mark input as activated
			IN_ActivateInput(inputGroup, inputFlag);
		}
	}
	else
	{
		// No assigned action -> mark input as activated
		// (to avoid unnecessary action lookup next time)
		IN_ActivateInput(inputGroup, inputFlag);
	}
}

static void IN_HandleInactiveInput(uint32_t * inputGroup, int inputFlag, char* inputName, float axisValue, qboolean thumbstickAxis)
{
	if (!IN_InputActivated(inputGroup, inputFlag))
	{
		// Input is not in activated state, nothing to do
		return;
	}
	char action[256];
	if (IN_GetInputAction(inputName, action))
	{
		// Deactivate input action and remove input activated state
		IN_SendInputAction(action, qfalse, axisValue, thumbstickAxis);
		IN_DeactivateInput(inputGroup, inputFlag);
	}
	else
	{
		// No assigned action -> just remove input activated state
		IN_DeactivateInput(inputGroup, inputFlag);
	}
}

void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
	if (!VR_AreHapticsEnabled())
	{
		return;
	}

	int weaponFireChannel = vr.weapon_stabilised ? 3 : (vr_righthanded->integer ? 2 : 1);
	if (strcmp(event, "pickup_shield") == 0 ||
		strcmp(event, "pickup_weapon") == 0 ||
		strstr(event, "pickup_item") != NULL)
	{
		VR_Vibrate(100, 3, 0.6);
	}
	else if (strcmp(event, "weapon_switch") == 0)
	{
		VR_Vibrate(150, vr_righthanded->integer ? 2 : 1, 0.6);
	}
	else if (strcmp(event, "shotgun") == 0 || strcmp(event, "fireball") == 0)
	{
		VR_Vibrate(250, 3, 0.85);
	}
	else if (strcmp(event, "bullet") == 0)
	{
		VR_Vibrate(150, 3, 0.65);
	}
	else if (strcmp(event, "chainsaw_fire") == 0)
	{
		VR_Vibrate(250, weaponFireChannel, 0.9);
	}
	else if (strcmp(event, "tesla_fire") == 0 || strcmp(event, "machinegun_fire") == 0)
	{
		VR_Vibrate(150, weaponFireChannel, 0.5);
	}
	else if (strcmp(event, "plasmagun_fire") == 0)
	{
		VR_Vibrate(90, weaponFireChannel, 0.75);
	}
	else if (strcmp(event, "shotgun_fire") == 0)
	{
		VR_Vibrate(150, weaponFireChannel, 1.0);
	}
	else if (strcmp(event, "rocket_fire") == 0 ||
		strcmp(event, "railgun_fire") == 0 ||
		strcmp(event, "bfg_fire") == 0 ||
		strcmp(event, "handgrenade_fire") == 0 )
	{
		VR_Vibrate(250, weaponFireChannel, 1.0);
	}
	else if (strcmp(event, "selector_icon") == 0)
	{
		VR_Vibrate(50, (vr_righthanded->integer ? 2 : 1), 0.6);
	}
	else if (strcmp(event, "menu_move") == 0) 
	{
		VR_Vibrate(30, (vr.menuLeftHanded ? 1 : 2), 0.3);
	}
}

XrSpace CreateActionSpace(XrAction poseAction, XrPath subactionPath)
{
	XrActionSpaceCreateInfo asci = {};
	asci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
	asci.action = poseAction;
	asci.poseInActionSpace.orientation.w = 1.0f;
	asci.subactionPath = subactionPath;
	XrSpace actionSpace = XR_NULL_HANDLE;
	OXR(xrCreateActionSpace(VR_GetEngine()->appState.Session, &asci, &actionSpace));
	return actionSpace;
}

XrActionSuggestedBinding ActionSuggestedBinding(XrAction action, const char* bindingString)
{
	XrActionSuggestedBinding asb;
	asb.action = action;
	XrPath bindingPath;
	OXR(xrStringToPath(VR_GetEngine()->appState.Instance, bindingString, &bindingPath));
	asb.binding = bindingPath;
	return asb;
}

XrActionSet CreateActionSet(int priority, const char* name, const char* localizedName)
{
	XrActionSetCreateInfo asci = {};
	asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	asci.next = NULL;
	asci.priority = priority;
	strcpy(asci.actionSetName, name);
	strcpy(asci.localizedActionSetName, localizedName);
	XrActionSet actionSet = XR_NULL_HANDLE;
	OXR(xrCreateActionSet(VR_GetEngine()->appState.Instance, &asci, &actionSet));
	return actionSet;
}

XrAction CreateAction(
	XrActionSet actionSet,
	XrActionType type,
	const char* actionName,
	const char* localizedName,
	int countSubactionPaths,
	XrPath* subactionPaths)
{
	printf("CreateAction %s, %d", actionName, countSubactionPaths);

	XrActionCreateInfo aci = {};
	aci.type = XR_TYPE_ACTION_CREATE_INFO;
	aci.next = NULL;
	aci.actionType = type;
	if (countSubactionPaths > 0)
	{
		aci.countSubactionPaths = countSubactionPaths;
		aci.subactionPaths = subactionPaths;
	}
	strcpy(aci.actionName, actionName);
	strcpy(aci.localizedActionName, localizedName ? localizedName : actionName);
	XrAction action = XR_NULL_HANDLE;
	OXR(xrCreateAction(actionSet, &aci, &action));
	return action;
}

qboolean ActionPoseIsActive(XrAction action, XrPath subactionPath)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;
	getInfo.subactionPath = subactionPath;

	XrActionStatePose state = {};
	state.type = XR_TYPE_ACTION_STATE_POSE;
	OXR(xrGetActionStatePose(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state.isActive != XR_FALSE;
}

XrActionStateFloat GetActionStateFloat(XrAction action)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;

	XrActionStateFloat state = {};
	state.type = XR_TYPE_ACTION_STATE_FLOAT;

	OXR(xrGetActionStateFloat(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state;
}

XrActionStateBoolean GetActionStateBoolean(XrAction action)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;

	XrActionStateBoolean state = {};
	state.type = XR_TYPE_ACTION_STATE_BOOLEAN;

	OXR(xrGetActionStateBoolean(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state;
}

XrActionStateVector2f GetActionStateVector2(XrAction action)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;

	XrActionStateVector2f state = {};
	state.type = XR_TYPE_ACTION_STATE_VECTOR2F;

	OXR(xrGetActionStateVector2f(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state;
}

void VR_InitInstanceInput( VR_Engine* engine )
{
	// Actions
	runningActionSet = CreateActionSet(1, "running_action_set", "Action Set used on main loop");
	indexLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_left", "Index left", 0, NULL);
	indexRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_right", "Index right", 0, NULL);
	menuAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu_action", "Menu", 0, NULL);
	buttonAAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "Button A", 0, NULL);
	buttonBAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "Button B", 0, NULL);
	buttonXAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "Button X", 0, NULL);
	buttonYAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Button Y", 0, NULL);
	gripLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_left", "Grip left", 0, NULL);
	gripRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_right", "Grip right", 0, NULL);
	trackpadLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trackpad_left", "Trackpad left", 0, NULL);
	trackpadRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trackpad_right", "Trackpad right", 0, NULL);
	moveOnLeftJoystickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_left_joy", "Move on left Joy", 0, NULL);
	moveOnRightJoystickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_right_joy", "Move on right Joy", 0, NULL);
	thumbstickLeftClickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_left", "Thumbstick left", 0, NULL);
	thumbstickRightClickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_right", "Thumbstick right", 0, NULL);
	thumbrestLeftTouchAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbrest_left_touch", "Thumbrest Left Touch", 0, NULL);
	thumbrestRightTouchAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbrest_right_touch", "Thumbrest Right Touch", 0, NULL);
	vibrateLeftFeedback = CreateAction(runningActionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_left_feedback", "Vibrate Left Controller Feedback", 0, NULL);
	vibrateRightFeedback = CreateAction(runningActionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_right_feedback", "Vibrate Right Controller Feedback", 0, NULL);

	OXR(xrStringToPath(engine->appState.Instance, "/user/hand/left", &leftHandPath));
	OXR(xrStringToPath(engine->appState.Instance, "/user/hand/right", &rightHandPath));
	handPoseLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_left", NULL, 1, &leftHandPath);
	handPoseRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_right", NULL, 1, &rightHandPath);

	XrPath interactionProfilePath = XR_NULL_PATH;
	XrPath interactionProfilePathValveIndex = XR_NULL_PATH;
	XrPath interactionProfilePathOculusTouch = XR_NULL_PATH;
	XrPath interactionProfilePathKHRSimple = XR_NULL_PATH;
	OXR(xrStringToPath(engine->appState.Instance, "/interaction_profiles/valve/index_controller", &interactionProfilePathValveIndex));
	OXR(xrStringToPath(engine->appState.Instance, "/interaction_profiles/oculus/touch_controller", &interactionProfilePathOculusTouch));
	OXR(xrStringToPath(engine->appState.Instance, "/interaction_profiles/khr/simple_controller", &interactionProfilePathKHRSimple));

	// Toggle this to force simple as a first choice, otherwise use it as a last resort
	if (useSimpleProfile)
	{
		printf("xrSuggestInteractionProfileBindings found bindings for Khronos SIMPLE controller");
		interactionProfilePath = interactionProfilePathKHRSimple;
	}
	else
	{
		// Query Set
		XrActionSet queryActionSet = CreateActionSet(1, "query_action_set", "Action Set used to query device caps");
		XrAction dummyAction = CreateAction(queryActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "dummy_action", "Dummy Action", 0, NULL);

		// Map bindings
		XrActionSuggestedBinding bindings[1];
		int currBinding = 0;
		bindings[currBinding++] = ActionSuggestedBinding(dummyAction, "/user/hand/right/input/system/click");

		XrInteractionProfileSuggestedBinding suggestedBindings = {};
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.next = NULL;
		suggestedBindings.suggestedBindings = bindings;
		suggestedBindings.countSuggestedBindings = currBinding;


		const XrPath interactionProfiles[] =
		{
			interactionProfilePathOculusTouch,
			interactionProfilePathValveIndex,
			interactionProfilePathKHRSimple,
		};
		const char* interactionProfileNames[] =
		{
			"Oculus Quest",
			"Valve Index",
			"Simple",
		};
		const size_t profilesCount = sizeof(interactionProfiles)/sizeof(interactionProfiles[0]);

		// Try until found supported one
		for (size_t profileIdx = 0; profileIdx < profilesCount; ++profileIdx)
		{
			suggestedBindings.interactionProfile = interactionProfiles[profileIdx];
			XrResult suggestTouchResult = xrSuggestInteractionProfileBindings(engine->appState.Instance, &suggestedBindings);
			if (XR_SUCCESS == suggestTouchResult)
			{
				printf("[OpenXR] Found supported bindings for %s controller\n", interactionProfileNames[profileIdx]);
				interactionProfilePath = interactionProfiles[profileIdx];
				break;
			}
		}

		if (interactionProfilePath == XR_NULL_PATH)
		{
			CHECK(XR_FALSE, "Failed to find supported controller bindings");
		}
	}

	// Action creation
	{
		// Map bindings
		XrActionSuggestedBinding bindings[32]; // large enough for all profiles
		int currBinding = 0;

		{
			if (interactionProfilePath == interactionProfilePathValveIndex)
			{
				bindings[currBinding++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/trigger/value");
				bindings[currBinding++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/trigger/value");
				bindings[currBinding++] = ActionSuggestedBinding(menuAction, "/user/hand/left/input/system/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonXAction, "/user/hand/left/input/a/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonYAction, "/user/hand/left/input/b/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonAAction, "/user/hand/right/input/a/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonBAction, "/user/hand/right/input/b/click");
				bindings[currBinding++] = ActionSuggestedBinding(gripLeftAction, "/user/hand/left/input/squeeze/value");
				bindings[currBinding++] = ActionSuggestedBinding(gripRightAction, "/user/hand/right/input/squeeze/value");
				bindings[currBinding++] = ActionSuggestedBinding(trackpadLeftAction, "/user/hand/left/input/trackpad/force");
				bindings[currBinding++] = ActionSuggestedBinding(trackpadRightAction, "/user/hand/right/input/trackpad/force");
				bindings[currBinding++] = ActionSuggestedBinding(moveOnLeftJoystickAction, "/user/hand/left/input/thumbstick");
				bindings[currBinding++] = ActionSuggestedBinding(moveOnRightJoystickAction, "/user/hand/right/input/thumbstick");
				bindings[currBinding++] = ActionSuggestedBinding(thumbstickLeftClickAction, "/user/hand/left/input/thumbstick/click");
				bindings[currBinding++] = ActionSuggestedBinding(thumbstickRightClickAction, "/user/hand/right/input/thumbstick/click");
				bindings[currBinding++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
				bindings[currBinding++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
				bindings[currBinding++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/aim/pose");
				bindings[currBinding++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/aim/pose");
			}
			else if (interactionProfilePath == interactionProfilePathOculusTouch)
			{
				bindings[currBinding++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/trigger");
				bindings[currBinding++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/trigger");
				bindings[currBinding++] = ActionSuggestedBinding(menuAction, "/user/hand/left/input/menu/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonXAction, "/user/hand/left/input/x/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonYAction, "/user/hand/left/input/y/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonAAction, "/user/hand/right/input/a/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonBAction, "/user/hand/right/input/b/click");
				bindings[currBinding++] = ActionSuggestedBinding(gripLeftAction, "/user/hand/left/input/squeeze/value");
				bindings[currBinding++] = ActionSuggestedBinding(gripRightAction, "/user/hand/right/input/squeeze/value");
				bindings[currBinding++] = ActionSuggestedBinding(moveOnLeftJoystickAction, "/user/hand/left/input/thumbstick");
				bindings[currBinding++] = ActionSuggestedBinding(moveOnRightJoystickAction, "/user/hand/right/input/thumbstick");
				bindings[currBinding++] = ActionSuggestedBinding(thumbstickLeftClickAction, "/user/hand/left/input/thumbstick/click");
				bindings[currBinding++] = ActionSuggestedBinding(thumbstickRightClickAction, "/user/hand/right/input/thumbstick/click");
				bindings[currBinding++] = ActionSuggestedBinding(thumbrestLeftTouchAction, "/user/hand/left/input/thumbrest/touch");
				bindings[currBinding++] = ActionSuggestedBinding(thumbrestRightTouchAction, "/user/hand/right/input/thumbrest/touch");
				bindings[currBinding++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
				bindings[currBinding++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
				bindings[currBinding++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/aim/pose");
				bindings[currBinding++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/aim/pose");
			}
			else if (interactionProfilePath == interactionProfilePathKHRSimple)
			{
				bindings[currBinding++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/select/click");
				bindings[currBinding++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/select/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonAAction, "/user/hand/left/input/menu/click");
				bindings[currBinding++] = ActionSuggestedBinding(buttonXAction, "/user/hand/right/input/menu/click");
				bindings[currBinding++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
				bindings[currBinding++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
				bindings[currBinding++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/aim/pose");
				bindings[currBinding++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/aim/pose");
			}
		}

		XrInteractionProfileSuggestedBinding suggestedBindings = {};
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.next = NULL;
		suggestedBindings.interactionProfile = interactionProfilePath;
		suggestedBindings.suggestedBindings = bindings;
		suggestedBindings.countSuggestedBindings = currBinding;
		OXR(xrSuggestInteractionProfileBindings(engine->appState.Instance, &suggestedBindings));
	}
}

void VR_InitSessionInput( VR_Engine* engine )
{
	if (inputInitialized)
	{
		return;
	}

	memset(&leftController, 0, sizeof(leftController));
	memset(&rightController, 0, sizeof(rightController));

	leftControllerAimSpace = CreateActionSpace(handPoseLeftAction, leftHandPath);
	rightControllerAimSpace = CreateActionSpace(handPoseRightAction, rightHandPath);

	// Enumerate actions
	XrPath actionPathsBuffer[32];
	char stringBuffer[256];
	XrAction actionsToEnumerate[] = {
		indexLeftAction,
		indexRightAction,
		menuAction,
		buttonAAction,
		buttonBAction,
		buttonXAction,
		buttonYAction,
		gripLeftAction,
		gripRightAction,
		trackpadLeftAction,
		trackpadRightAction,
		moveOnLeftJoystickAction,
		moveOnRightJoystickAction,
		thumbstickLeftClickAction,
		thumbstickRightClickAction,
		thumbrestLeftTouchAction,
		thumbrestRightTouchAction,
		vibrateLeftFeedback,
		vibrateRightFeedback,
		handPoseLeftAction,
		handPoseRightAction
	};
	for (size_t i = 0; i < sizeof(actionsToEnumerate) / sizeof(actionsToEnumerate[0]); ++i)
	{
		XrBoundSourcesForActionEnumerateInfo enumerateInfo = {};
		enumerateInfo.type = XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO;
		enumerateInfo.next = NULL;
		enumerateInfo.action = actionsToEnumerate[i];

		// Get Count
		uint32_t countOutput = 0;
		OXR(xrEnumerateBoundSourcesForAction(
			engine->appState.Session, &enumerateInfo, 0 /* request size */, &countOutput, NULL));
		printf(
			"xrEnumerateBoundSourcesForAction action=%lld count=%u\n",
			(long long)enumerateInfo.action,
			countOutput);

		if (countOutput < 32)
		{
			OXR(xrEnumerateBoundSourcesForAction(
				engine->appState.Session, &enumerateInfo, 32, &countOutput, actionPathsBuffer));
			for (uint32_t a = 0; a < countOutput; ++a)
			{
				XrInputSourceLocalizedNameGetInfo nameGetInfo = {};
				nameGetInfo.type = XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO;
				nameGetInfo.next = NULL;
				nameGetInfo.sourcePath = actionPathsBuffer[a];
				nameGetInfo.whichComponents = 
					XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
					XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
					XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

				uint32_t stringCount = 0u;
				OXR(xrGetInputSourceLocalizedName(engine->appState.Session, &nameGetInfo, 0, &stringCount, NULL));
				if (stringCount < 256)
				{
					OXR(xrGetInputSourceLocalizedName(engine->appState.Session, &nameGetInfo, 256, &stringCount, stringBuffer));
					char pathStr[256];
					uint32_t strLen = 0;
					OXR(xrPathToString(engine->appState.Instance, actionPathsBuffer[a], (uint32_t)sizeof(pathStr), &strLen, pathStr));
					printf(
						"  -> path = %lld `%s` -> `%s`\n",
						(long long)actionPathsBuffer[a],
						pathStr,
						stringBuffer);
				}
			}
		}
	}

	XrSessionActionSetsAttachInfo attachInfo = {};
	attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	attachInfo.next = NULL;
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &runningActionSet;
	XR_CHECK(xrAttachSessionActionSets(engine->appState.Session, &attachInfo), "");

	inputInitialized = qtrue;
}

void VR_DestroySessionInput( VR_Engine* engine )
{
	// This will allow to recreate session-specific OpenXR input objects
	inputInitialized = qfalse;
}

static void IN_VRController( qboolean isRightController, XrPosef pose )
{
	//Set gun angles - We need to calculate all those we might need (including adjustments) for the client to then take its pick
	vec3_t rotation = {0};
	if (isRightController == (vr_righthanded->integer != 0))
	{
		//Set gun angles - We need to calculate all those we might need (including adjustments) for the client to then take its pick
		rotation[PITCH] = vr_weaponPitch->value;
		QuatToYawPitchRoll(pose.orientation, rotation, vr.weaponangles);

		VectorSubtract(vr.weaponangles_last, vr.weaponangles, vr.weaponangles_delta);
		VectorCopy(vr.weaponangles, vr.weaponangles_last);

		///Weapon location relative to view
		vr.weaponposition[0] = pose.position.x;
		vr.weaponposition[1] = pose.position.y + vr_heightAdjust->value;
		vr.weaponposition[2] = pose.position.z;

		VectorCopy(vr.weaponoffset_last[1], vr.weaponoffset_last[0]);
		VectorCopy(vr.weaponoffset, vr.weaponoffset_last[1]);
		VectorSubtract(vr.weaponposition, vr.hmdposition, vr.weaponoffset);
	}
	else
	{
		QuatToYawPitchRoll(pose.orientation, rotation, vr.offhandangles2); // used for off-hand direction mode
		rotation[PITCH] = vr_weaponPitch->value;
		QuatToYawPitchRoll(pose.orientation, rotation, vr.offhandangles);

		///location relative to view
		vr.offhandposition[0] = pose.position.x;
		vr.offhandposition[1] = pose.position.y + vr_heightAdjust->value;
		vr.offhandposition[2] = pose.position.z;

		VectorCopy(vr.offhandoffset_last[1], vr.offhandoffset_last[0]);
		VectorCopy(vr.offhandoffset, vr.offhandoffset_last[1]);
		VectorSubtract(vr.offhandposition, vr.hmdposition, vr.offhandoffset);
	}

	if (vr.virtual_screen || cl.snap.ps.pm_type == PM_INTERMISSION)
	{
		vr.weapon_zoomed = qfalse;
		if (vr.menuCursorX && vr.menuCursorY)
		{
			float yaw;
			float pitch;
			if (vr.menuLeftHanded)
			{
				yaw = (vr_righthanded->integer != 0) ? vr.offhandangles[YAW] : vr.weaponangles[YAW];
				pitch = (vr_righthanded->integer != 0) ? vr.offhandangles[PITCH] : vr.weaponangles[PITCH];
			}
			else
			{
				yaw = (vr_righthanded->integer != 0) ? vr.weaponangles[YAW] : vr.offhandangles[YAW];
				pitch = (vr_righthanded->integer != 0) ? vr.weaponangles[PITCH] : vr.offhandangles[PITCH];
			}
			int x = 320 - tan((yaw - vr.menuYaw) * (M_PI*2 / 360)) * 800;
			int y = 240 + tan((pitch + vr_weaponPitch->value) * (M_PI*2 / 360)) * 800;

			static int lastMenuCursorX = 320;
			static int lastMenuCursorY = 240;

			// lepr from old position to new position by given factor
			// this is needed to avoid artifacts when moving rapidly hand or HMD
			const float factor = 0.5f;
			x = factor * x + (1.0f - factor) * lastMenuCursorX;
			y = factor * y + (1.0f - factor) * lastMenuCursorY;

			lastMenuCursorX = *vr.menuCursorX = x;
			lastMenuCursorY = *vr.menuCursorY = y;

			Com_QueueEvent(in_vrEventTime, SE_MOUSE, 0, 0, 0, NULL);
		}
	}
	else
	{
		vr.weapon_zoomed = vr_weaponScope->integer &&
												vr.weapon_stabilised &&
												(cl.snap.ps.weapon == WP_RAILGUN) &&
												(VectorLength(vr.weaponoffset) < 0.24f) &&
												cl.snap.ps.stats[STAT_HEALTH] > 0;

		if (vr_twoHandedWeapons->integer && vr.weapon_stabilised)
		{
			if (vr_twoHandedWeapons->integer == 2) // Virtual gun stock
			{
				// Offset to the appropriate eye a little bit
				vec2_t xy;
				rotateAboutOrigin(Cvar_VariableValue("cg_stereoSeparation") / 2.0f, 0.0f, -vr.hmdorientation[YAW], xy);
				float x = vr.offhandposition[0] - (vr.hmdposition[0] + xy[0]);
				float y = vr.offhandposition[1] - (vr.hmdposition[1] - 0.1f); // Use a point lower
				float z = vr.offhandposition[2] - (vr.hmdposition[2] + xy[1]);

				float zxDist = length(x, z);

				if (zxDist != 0.0f && z != 0.0f)
				{
					VectorSet(vr.weaponangles, -RadiansToDegrees(atanf(y / zxDist)), -RadiansToDegrees(atan2f(x, -z)), 0);
				}
			}
			else // Basic two-handed
			{
				// Apply smoothing to the weapon hand
				vec3_t smooth_weaponoffset;
				VectorAdd(vr.weaponoffset, vr.weaponoffset_last[0], smooth_weaponoffset);
				VectorAdd(smooth_weaponoffset, vr.weaponoffset_last[1],smooth_weaponoffset);
				VectorScale(smooth_weaponoffset, 1.0f/3.0f, smooth_weaponoffset);

				vec3_t vec;
				VectorSubtract(vr.offhandoffset, smooth_weaponoffset, vec);

				float zxDist = length(vec[0], vec[2]);

				if (zxDist != 0.0f && vec[2] != 0.0f)
				{
					VectorSet(vr.weaponangles, -RadiansToDegrees(atanf(vec[1] / zxDist)), -RadiansToDegrees(atan2f(vec[0], -vec[2])), vr.weaponangles[ROLL] / 2.0f); // Dampen roll on stabilised weapon
				}
			}
		}
	}
}

static qboolean IN_VRJoystickUse8WayMapping( void )
{
	char action[256];
	return 
		IN_GetInputAction("RTHUMBFORWARDRIGHT", action)
		|| IN_GetInputAction("RTHUMBBACKRIGHT", action)
		|| IN_GetInputAction("RTHUMBBACKLEFT", action)
		|| IN_GetInputAction("RTHUMBFORWARDLEFT", action);
}

static void IN_VRJoystickHandle4WayMapping( uint32_t * inputGroup, float joystickAngle, float joystickValue )
{
	if (joystickAngle >= 315.0 || joystickAngle < 45.0) // UP
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
		// Activate UP
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
	}
	else if (joystickAngle < 135.0) // RIGHT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
		// Activate RIGHT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
	}
	else if (joystickAngle < 225.0) // DOWN
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
		// Activate DOWN
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
	}
	else // LEFT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
		// Activate LEFT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
	}
}

static void IN_VRJoystickHandle8WayMapping( uint32_t * inputGroup, float joystickAngle, float joystickValue )
{
	if (joystickAngle > 337.5 || joystickAngle < 22.5) // UP
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
		// Activate UP
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
	}
	else if (joystickAngle < 67.5) // UP-RIGHT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
		// Activate UP-RIGHT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
	}
	else if (joystickAngle < 112.5) // RIGHT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
		// Activate RIGHT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
	}
	else if (joystickAngle < 157.5) // DOWN-RIGHT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
		// Activate DOWN-RIGHT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
	}
	else if (joystickAngle < 202.5) // DOWN
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
		// Activate DOWN
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
	}
	else if (joystickAngle < 247.5) // DOWN-LEFT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
		// Activate DOWN-LEFT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
	}
	else if (joystickAngle < 292.5) // LEFT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
		// Activate LEFT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
	}
	else // UP-LEFT
	{
		// Deactivate neighboring inputs
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
		IN_HandleInactiveInput(inputGroup, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
		// Activate UP-LEFT
		IN_HandleActiveInput(inputGroup, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
	}
}

static void IN_VRJoystick( qboolean isRightController, float joystickX, float joystickY )
{
	vrController_t* controller = isRightController == qtrue ? &rightController : &leftController;

	vr.thumbstick_location[isRightController][0] = joystickX;
	vr.thumbstick_location[isRightController][1] = joystickY;

	if (vr.virtual_screen || cl.snap.ps.pm_type == PM_INTERMISSION)
	{

		// Use thumbstick UP/DOWN as PAGEUP/PAGEDOWN in menus
		if (joystickY > thumbstickPressedThreshold)
		{
			if (!IN_InputActivated(&controller->axisButtons, VR_TOUCH_AXIS_UP))
			{
				IN_ActivateInput(&controller->axisButtons, VR_TOUCH_AXIS_UP);
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_PGUP, qtrue, 0, NULL);
			}
		}
		else if (joystickY < -thumbstickPressedThreshold)
		{
			if (!IN_InputActivated(&controller->axisButtons, VR_TOUCH_AXIS_DOWN))
			{
				IN_ActivateInput(&controller->axisButtons, VR_TOUCH_AXIS_DOWN);
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_PGDN, qtrue, 0, NULL);
			}
		}
		else if (joystickY < thumbstickReleasedThreshold && joystickY > -thumbstickReleasedThreshold)
		{
			if (IN_InputActivated(&controller->axisButtons, VR_TOUCH_AXIS_UP))
			{
				IN_DeactivateInput(&controller->axisButtons, VR_TOUCH_AXIS_UP);
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_PGUP, qfalse, 0, NULL);
			}
			if (IN_InputActivated(&controller->axisButtons, VR_TOUCH_AXIS_DOWN))
			{
				IN_DeactivateInput(&controller->axisButtons, VR_TOUCH_AXIS_DOWN);
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_PGDN, qfalse, 0, NULL);
			}
		}
	}
	else
	{
		if (isRightController == (vr_switchThumbsticks->integer != 0))
		{
			vec3_t positional;
			VectorClear(positional);

			vec2_t joystick;
			if ( vr.use_fake_6dof )
			{
				//multiplayer game
				if (!vr_directionMode->integer)
				{
					//HMD Based
					rotateAboutOrigin(joystickX, joystickY, vr.hmdorientation[YAW], joystick);
				}
				else
				{
					//Off-hand based
					rotateAboutOrigin(joystickX, joystickY, vr.offhandangles2[YAW], joystick);
				}
			}
			else
			{
				//Positional movement speed correction for when we are not hitting target framerate
				const float refresh = VR_GetEngine()->appState.Renderer.RefreshRate;
				float multiplier = (float)((1000.0 / refresh) / (in_vrEventTime - lastframetime));

				float factor = (refresh / 72.0F) * 10.0f; // adjust positional factor based on refresh rate
				rotateAboutOrigin(
					-vr.hmdposition_delta[0] * factor * multiplier,
					vr.hmdposition_delta[2] * factor * multiplier,
					-vr.hmdorientation[YAW], positional);

				if (!vr_directionMode->integer)
				{
					//HMD Based
					joystick[0] = joystickX;
					joystick[1] = joystickY;
				}
				else
				{
					//Off-hand based
					rotateAboutOrigin(joystickX, joystickY, vr.offhandangles2[YAW] - vr.hmdorientation[YAW], joystick);
				}
			}

			//sideways
			Com_QueueEvent(in_vrEventTime, SE_JOYSTICK_AXIS, 0, joystick[0] * 127.0f + positional[0] * 127.0f, 0, NULL);

			//forward/back
			Com_QueueEvent(in_vrEventTime, SE_JOYSTICK_AXIS, 1, joystick[1] * 127.0f + positional[1] * 127.0f, 0, NULL);
		}

		// In case thumbstick is used by weapon wheel (is in HMD/thumbstick mode), ignore standard thumbstick inputs
		else if (!vr.weapon_select_using_thumbstick)
		{
			float joystickValue = length(joystickX, joystickY);
			if (joystickValue < thumbstickReleasedThreshold)
			{
				// Joystick within threshold -> disable all inputs
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_UP, "RTHUMBFORWARD", joystickValue, qtrue);
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_UPRIGHT, "RTHUMBFORWARDRIGHT", joystickValue, qtrue);
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_RIGHT, "RTHUMBRIGHT", joystickValue, qtrue);
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_DOWNRIGHT, "RTHUMBBACKRIGHT", joystickValue, qtrue);
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_DOWN, "RTHUMBBACK", joystickValue, qtrue);
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_DOWNLEFT, "RTHUMBBACKLEFT", joystickValue, qtrue);
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_LEFT, "RTHUMBLEFT", joystickValue, qtrue);
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_UPLEFT, "RTHUMBFORWARDLEFT", joystickValue, qtrue);
			}
			else if (joystickValue > thumbstickPressedThreshold)
			{
				float joystickAngle = AngleNormalize360(RAD2DEG(atan2(joystickX, joystickY)));
				if (IN_VRJoystickUse8WayMapping())
				{
						IN_VRJoystickHandle8WayMapping(&controller->axisButtons, joystickAngle, joystickValue);
				}
				else
				{
						IN_VRJoystickHandle4WayMapping(&controller->axisButtons, joystickAngle, joystickValue);
				}
			}
		}
	}
}

static void IN_VRTriggers( qboolean isRightController, float triggerValue )
{
	vrController_t* controller = isRightController == qtrue ? &rightController : &leftController;

	if (VR_Gameplay_ShouldRenderInVirtualScreen() || cl.snap.ps.pm_type == PM_INTERMISSION)
	{
		// Triggers are used for menu navigation in screen mode or in intermission
		if (triggerValue > triggerPressedThreshold && !IN_InputActivated(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX))
		{
			IN_ActivateInput(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX);
			if ((isRightController && !vr.menuLeftHanded) || (!isRightController && vr.menuLeftHanded))
			{
				// Active controller confirms selection
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_MOUSE1, qtrue, 0, NULL);
				VR_Vibrate(200, vr.menuLeftHanded ? 1 : 2, 0.8);
			}
			else
			{
				// Inactive controller becomes active one
				vr.menuLeftHanded = !vr.menuLeftHanded;
			}
		}
		else if (triggerValue < triggerReleasedThreshold && IN_InputActivated(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX))
		{
			IN_DeactivateInput(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX);
			if ((isRightController && !vr.menuLeftHanded) || (!isRightController && vr.menuLeftHanded))
			{
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_MOUSE1, qfalse, 0, NULL);
			}
		}
	}
	else
	{
		// Primary trigger
		if (isRightController == (vr_righthanded->integer != 0))
		{
			if (triggerValue > triggerPressedThreshold)
			{
				IN_HandleActiveInput(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX, "PRIMARYTRIGGER", triggerValue, qfalse);
			}
			else if (triggerValue < triggerReleasedThreshold)
			{
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX, "PRIMARYTRIGGER", triggerValue, qfalse);
			}
		}

		// Off hand trigger
		if (isRightController != (vr_righthanded->integer != 0))
		{
			if (triggerValue > triggerPressedThreshold)
			{
				IN_HandleActiveInput(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX, "SECONDARYTRIGGER", triggerValue, qfalse);
			}
			else if (triggerValue < triggerReleasedThreshold)
			{
				IN_HandleInactiveInput(&controller->axisButtons, VR_TOUCH_AXIS_TRIGGER_INDEX, "SECONDARYTRIGGER", triggerValue, qfalse);
			}
		}
	}
}

static void IN_VRButtons( qboolean isRightController, uint32_t buttons )
{
	vrController_t* controller = isRightController == qtrue ? &rightController : &leftController;

	// Menu button
	if ((buttons & VR_Button_Enter) && !IN_InputActivated(&controller->buttons, VR_Button_Enter))
	{
		IN_ActivateInput(&controller->buttons, VR_Button_Enter);
		Com_QueueEvent(in_vrEventTime, SE_KEY, K_ESCAPE, qtrue, 0, NULL);
	}
	else if (!(buttons & VR_Button_Enter) && IN_InputActivated(&controller->buttons, VR_Button_Enter))
	{
		IN_DeactivateInput(&controller->buttons, VR_Button_Enter);
		Com_QueueEvent(in_vrEventTime, SE_KEY, K_ESCAPE, qfalse, 0, NULL);
	}

	if (isRightController == !vr_righthanded->integer)
	{
		if (buttons & VR_Button_GripTrigger)
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_GripTrigger, "SECONDARYGRIP", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_GripTrigger, "SECONDARYGRIP", 0, qfalse);
		}
	}
	else
	{
		if (buttons & VR_Button_GripTrigger)
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_GripTrigger, "PRIMARYGRIP", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_GripTrigger, "PRIMARYGRIP", 0, qfalse);
		}
	}

	// Trackpad
	if (isRightController == !vr_righthanded->integer)
	{
		if (buttons & VR_Button_Trackpad)
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_Trackpad, "SECONDARYTRACKPAD", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_Trackpad, "SECONDARYTRACKPAD", 0, qfalse);
		}
	}
	else
	{
		if (buttons & VR_Button_Trackpad)
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_Trackpad, "PRIMARYTRACKPAD", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_Trackpad, "PRIMARYTRACKPAD", 0, qfalse);
		}
	}

	if (isRightController == !vr_righthanded->integer)
	{
		if (buttons & VR_Button_LThumb)
		{
			if (!IN_InputActivated(&controller->buttons, VR_Button_LThumb))
			{
				// Initiate position reset for fake 6DoF
				vr.realign = 3;
			}
			IN_HandleActiveInput(&controller->buttons, VR_Button_LThumb, "SECONDARYTHUMBSTICK", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_LThumb, "SECONDARYTHUMBSTICK", 0, qfalse);
		}
	}
	else
	{
		if (buttons & VR_Button_RThumb)
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_RThumb, "PRIMARYTHUMBSTICK", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_RThumb, "PRIMARYTHUMBSTICK", 0, qfalse);
		}
	}

	if (buttons & VR_Button_A)
	{
		if (cl.snap.ps.pm_flags & PMF_FOLLOW)
		{
			// Go back to free spectator mode if following player
			if (!IN_InputActivated(&controller->buttons, VR_Button_A))
			{
				IN_ActivateInput(&controller->buttons, VR_Button_A);
				Cbuf_AddText("cmd team spectator\n");
			}
		}
		else if (VR_Gameplay_ShouldRenderInVirtualScreen() || cl.snap.ps.pm_type == PM_INTERMISSION)
		{
			// Skip server search in the server menu
			if (!IN_InputActivated(&controller->buttons, VR_Button_A))
			{
				IN_ActivateInput(&controller->buttons, VR_Button_A);
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_SPACE, qtrue, 0, NULL);
			}
		}
		else
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_A, "A", 0, qfalse);
		}
	}
	else
	{
		if (VR_Gameplay_ShouldRenderInVirtualScreen() || cl.snap.ps.pm_type == PM_INTERMISSION)
		{
			// Skip server search in the server menu
			if (IN_InputActivated(&controller->buttons, VR_Button_A))
			{
				IN_DeactivateInput(&controller->buttons, VR_Button_A);
				Com_QueueEvent(in_vrEventTime, SE_KEY, K_SPACE, qfalse, 0, NULL);
			}
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_A, "A", 0, qfalse);
		}
	}

	if (buttons & VR_Button_B)
	{
		if (!IN_InputActivated(&controller->buttons, VR_Button_B))
		{
			VR_VirtualScreen_ResetPosition();
		}
		IN_HandleActiveInput(&controller->buttons, VR_Button_B, "B", 0, qfalse);
	}
	else
	{
		IN_HandleInactiveInput(&controller->buttons, VR_Button_B, "B", 0, qfalse);
	}

	if (buttons & VR_Button_X)
	{
		if (cl.snap.ps.pm_flags & PMF_FOLLOW)
		{
			// Switch follow mode
			if (!IN_InputActivated(&controller->buttons, VR_Button_X))
			{
				IN_ActivateInput(&controller->buttons, VR_Button_X);
				// For now disable other follow modes, as they don't operate as they should
				// vr.follow_mode = (vr.follow_mode+1) % VRFM_NUM_FOLLOWMODES;
				// if (vr.follow_mode == VRFM_THIRDPERSON_1)
				// {
					Cbuf_ExecuteText(EXEC_APPEND, "follow\n");
				// }

				// Initiate realign
				vr.realign = 3;
			}
		}
		else
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_X, "X", 0, qfalse);
		}
	}
	else
	{
		IN_HandleInactiveInput(&controller->buttons, VR_Button_X, "X", 0, qfalse);
	}

	if (buttons & VR_Button_Y)
	{
		IN_HandleActiveInput(&controller->buttons, VR_Button_Y, "Y", 0, qfalse);
	}
	else
	{
		IN_HandleInactiveInput(&controller->buttons, VR_Button_Y, "Y", 0, qfalse);
	}

	if (isRightController == !vr_righthanded->integer)
	{
		if (buttons & VR_Button_Thumbrest)
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_Thumbrest, "SECONDARYTHUMBREST", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_Thumbrest, "SECONDARYTHUMBREST", 0, qfalse);
		}
	}
	else
	{
		if (buttons & VR_Button_Thumbrest)
		{
			IN_HandleActiveInput(&controller->buttons, VR_Button_Thumbrest, "PRIMARYTHUMBREST", 0, qfalse);
		}
		else
		{
			IN_HandleInactiveInput(&controller->buttons, VR_Button_Thumbrest, "PRIMARYTHUMBREST", 0, qfalse);
		}
	}
}

void VR_ProcessInputActions( void )
{
	vr.virtual_screen = VR_Gameplay_ShouldRenderInVirtualScreen();

	VR_ProcessHaptics();

	//button mapping
	uint32_t lButtons = 0;
	if (GetActionStateBoolean(menuAction).currentState) lButtons |= VR_Button_Enter;
	if (GetActionStateBoolean(buttonXAction).currentState) lButtons |= VR_Button_X;
	if (GetActionStateBoolean(buttonYAction).currentState) lButtons |= VR_Button_Y;
	if (GetActionStateFloat(gripLeftAction).currentState > 0.5f) lButtons |= VR_Button_GripTrigger;
	if (GetActionStateFloat(trackpadLeftAction).currentState > 0.3f) lButtons |= VR_Button_Trackpad;
	if (GetActionStateBoolean(thumbstickLeftClickAction).currentState) lButtons |= VR_Button_LThumb;
	if (GetActionStateBoolean(thumbrestLeftTouchAction).currentState) lButtons |= VR_Button_Thumbrest;
	IN_VRButtons(qfalse, lButtons);
	uint32_t rButtons = 0;
	if (GetActionStateBoolean(buttonAAction).currentState) rButtons |= VR_Button_A;
	if (GetActionStateBoolean(buttonBAction).currentState) rButtons |= VR_Button_B;
	if (GetActionStateFloat(gripRightAction).currentState > 0.5f) rButtons |= VR_Button_GripTrigger;
	if (GetActionStateFloat(trackpadRightAction).currentState > 0.3f) rButtons |= VR_Button_Trackpad;
	if (GetActionStateBoolean(thumbstickRightClickAction).currentState) rButtons |= VR_Button_RThumb;
	if (GetActionStateBoolean(thumbrestRightTouchAction).currentState) rButtons |= VR_Button_Thumbrest;
	IN_VRButtons(qtrue, rButtons);

	//index finger click
	XrActionStateBoolean indexState;
	indexState = GetActionStateBoolean(indexLeftAction);
	IN_VRTriggers(qfalse, indexState.currentState ? 1 : 0);
	indexState = GetActionStateBoolean(indexRightAction);
	IN_VRTriggers(qtrue, indexState.currentState ? 1 : 0);

	//thumbstick
	XrActionStateVector2f moveJoystickState;
	moveJoystickState = GetActionStateVector2(moveOnLeftJoystickAction);
	IN_VRJoystick(qfalse, moveJoystickState.currentState.x, moveJoystickState.currentState.y);
	moveJoystickState = GetActionStateVector2(moveOnRightJoystickAction);
	IN_VRJoystick(qtrue, moveJoystickState.currentState.x, moveJoystickState.currentState.y);

	lastframetime = in_vrEventTime;
	in_vrEventTime = Sys_Milliseconds( );
}

void IN_VRSyncActions( VR_Engine* engine )
{
	// sync action data
	XrActiveActionSet activeActionSet = {};
	activeActionSet.actionSet = runningActionSet;
	activeActionSet.subactionPath = XR_NULL_PATH;

	XrActionsSyncInfo syncInfo = {};
	syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
	syncInfo.next = NULL;
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeActionSet;
	XR_CHECK(xrSyncActions(engine->appState.Session, &syncInfo), "failed to sync actions");
}

void IN_VRUpdateControllers( VR_Engine* engine, XrTime predictedDisplayTime )
{
	//get controller poses
	XrAction controller[] = {handPoseLeftAction, handPoseRightAction};
	XrPath subactionPath[] = {leftHandPath, rightHandPath};
	XrSpace controllerSpace[] = {leftControllerAimSpace, rightControllerAimSpace};
	for (int i = 0; i < 2; i++)
	{
		if (ActionPoseIsActive(controller[i], subactionPath[i]))
		{
			XrSpaceLocation loc = {};
			loc.type = XR_TYPE_SPACE_LOCATION;
			OXR(xrLocateSpace(controllerSpace[i], engine->appState.CurrentSpace, predictedDisplayTime, &loc));

			engine->appState.TrackedController[i].Active = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
			engine->appState.TrackedController[i].Pose = loc.pose;
		}
		else
		{
			XrPosef posef_identity = {};
			posef_identity.orientation.w = 1.0;

			engine->appState.TrackedController[i].Active = false;
			engine->appState.TrackedController[i].Pose = posef_identity;
		}
	}

	//apply controller poses
	if (engine->appState.TrackedController[0].Active)
	{
		IN_VRController(qfalse, engine->appState.TrackedController[0].Pose);
	}
	if (engine->appState.TrackedController[1].Active)
	{
		IN_VRController(qtrue, engine->appState.TrackedController[1].Pose);
	}
}

void IN_VRUpdateHMD( XrView* views, uint32_t viewCount, XrFovf* fov )
{
	// Set FOV
	memset(fov, 0, sizeof(XrFovf));
	for (uint32_t view = 0; view < viewCount; view++)
	{
		fov->angleLeft += views[view].fov.angleLeft / (float)(viewCount);
		fov->angleRight += views[view].fov.angleRight / (float)(viewCount);
		fov->angleUp += views[view].fov.angleUp / (float)(viewCount);
		fov->angleDown += views[view].fov.angleDown / (float)(viewCount);
	}
	vr.fov_x = (fabs(fov->angleLeft) + fabs(fov->angleRight)) * 180.0f / M_PI;
	vr.fov_y = (fabs(fov->angleUp) + fabs(fov->angleDown)) * 180.0f / M_PI;

	// Get center HMD pose (view center) and center rotation (middle rotation)
	XrVector3f hmdPosition = views[0].pose.position;
	XrQuaternionf hmdRotation = views[0].pose.orientation;
	if (viewCount > 1)
	{
		CHECK(viewCount == 2, "Unexpected number of views");
		XrVector3f_Lerp(&hmdPosition, &views[0].pose.position, &views[1].pose.position, 0.5f);
		XrQuaternionf_Lerp(&hmdRotation, &views[0].pose.orientation, &views[1].pose.orientation, 0.5f);
	}

	// Last position and orientation
	vec3_t hmdposition_last, hmdorientation_last;
	VectorCopy(vr.hmdposition, hmdposition_last);
	VectorCopy(vr.hmdorientation, hmdorientation_last);

	// We extract Yaw, Pitch, Roll instead of directly using the orientation
	// to allow "additional" yaw manipulation with mouse/controller.
	vec3_t rotation = {0, 0, 0};
	QuatToYawPitchRoll(hmdRotation, rotation, vr.hmdorientation);

	// Position
	VectorSet(vr.hmdposition, hmdPosition.x, hmdPosition.y + vr_heightAdjust->value, hmdPosition.z);

	//Position delta
	VectorSubtract(hmdposition_last, vr.hmdposition, vr.hmdposition_delta);

	//Orientation
	VectorSubtract(hmdorientation_last, vr.hmdorientation, vr.hmdorientation_delta);

	// View yaw delta
	const float clientview_yaw = vr.clientviewangles[YAW] - vr.hmdorientation[YAW];
	vr.clientview_yaw_delta = vr.clientview_yaw_last - clientview_yaw;
	vr.clientview_yaw_last = clientview_yaw;
}
