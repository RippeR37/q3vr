#define VR_BHAPTICS_IMPLEMENTATION
#include "vr_bhaptics.h"

#ifdef USE_BHAPTICS

#include "vr_haptics.h"
#include "vr_clientinfo.h"
#include "vr_base.h"

#include "../qcommon/qcommon.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define BHAPTICS_LIBRARY_NAME "haptic_library.dll"
typedef HMODULE bhaptics_lib_handle;
#define BHAPTICS_LOAD(lib) LoadLibraryA(lib)
#define BHAPTICS_LOAD_FUNC GetProcAddress
#define BHAPTICS_CLOSE(lib) FreeLibrary(lib)
#elif defined(__linux__)
#include <dlfcn.h>
#define BHAPTICS_LIBRARY_NAME "libhaptic_library.so"
typedef void* bhaptics_lib_handle;
#define BHAPTICS_LOAD(lib) dlopen(lib, RTLD_LAZY)
#define BHAPTICS_LOAD_FUNC dlsym
#define BHAPTICS_CLOSE(lib) dlclose(lib)
#else
#undef USE_BHAPTICS
#endif
#endif

#ifdef USE_BHAPTICS

#define BH_POS_HAND_L 6
#define BH_POS_HAND_R 7
#define BH_POS_HEAD 4
#define BH_POS_VEST_FRONT 201
#define BH_POS_VEST_BACK 202
#define BH_POS_ARM_L 10
#define BH_POS_ARM_R 11

typedef struct bh_point_s
{
	float x;
	float y;
	int intensity;
	int motorCount;
} bh_point_t;

typedef void (*bh_init_f)(const char* appId, const char* appName);
typedef void (*bh_destroy_f)(void);
typedef void (*bh_turnoff_f)(void);
typedef void (*bh_submit_path_array_f)(const char* key, int pos, bh_point_t* points, size_t length, int durationMillis);

extern cvar_t *vr_bhaptics;
extern cvar_t *vr_righthanded;
extern vr_clientinfo_t vr;

static qboolean bhapticsInitialized = qfalse;
static bhaptics_lib_handle bhapticsLibrary = NULL;
static bh_init_f bhInitialise = NULL;
static bh_destroy_f bhDestroy = NULL;
static bh_turnoff_f bhTurnOff = NULL;
static bh_submit_path_array_f bhSubmitPathArray = NULL;

static float Clamp01(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}
	if (value > 1.0f)
	{
		return 1.0f;
	}
	return value;
}

static float NormalizeYaw(float yaw)
{
	float normalized = yaw;
	while (normalized < 0.0f)
	{
		normalized += 360.0f;
	}
	while (normalized >= 360.0f)
	{
		normalized -= 360.0f;
	}
	return normalized;
}

static int ScaleSuitIntensity(int baseIntensity)
{
	float scaled = (float)baseIntensity * VR_GetHapticsIntensity();
	if (scaled < 0.0f)
	{
		scaled = 0.0f;
	}
	if (scaled > 100.0f)
	{
		scaled = 100.0f;
	}
	return (int)scaled;
}

static int ScaledEdgeIntensity(int intensity)
{
	int scaled = (int)((float)intensity * 0.6f);
	return (scaled < 1 && intensity > 0) ? 1 : scaled;
}

static float HeightAs01(float height)
{
	return Clamp01(0.5f + (height * 0.5f));
}

static int VestSideForYaw(float yaw)
{
	float normalized = NormalizeYaw(yaw);
	return (normalized > 90.0f && normalized < 270.0f) ? BH_POS_VEST_BACK : BH_POS_VEST_FRONT;
}

static size_t BuildImpactCluster(bh_point_t* points, float x, float y, int intensity, int motorCount, float spread)
{
	const int edgeIntensity = ScaledEdgeIntensity(intensity);

	points[0].x = x;
	points[0].y = y;
	points[0].intensity = intensity;
	points[0].motorCount = motorCount;

	points[1].x = Clamp01(x - spread);
	points[1].y = y;
	points[1].intensity = edgeIntensity;
	points[1].motorCount = motorCount;

	points[2].x = Clamp01(x + spread);
	points[2].y = y;
	points[2].intensity = edgeIntensity;
	points[2].motorCount = motorCount;

	points[3].x = x;
	points[3].y = Clamp01(y + spread);
	points[3].intensity = edgeIntensity;
	points[3].motorCount = motorCount;

	points[4].x = x;
	points[4].y = Clamp01(y - spread);
	points[4].intensity = edgeIntensity;
	points[4].motorCount = motorCount;

	return 5;
}

static qboolean LoadBhapticsLibrary(void)
{
	if (bhapticsLibrary)
	{
		Com_DPrintf("bHaptics: library already loaded\n");
		return qtrue;
	}

	Com_Printf("bHaptics: loading %s...\n", BHAPTICS_LIBRARY_NAME);
	bhapticsLibrary = BHAPTICS_LOAD(BHAPTICS_LIBRARY_NAME);
	if (!bhapticsLibrary)
	{
		Com_Printf("bHaptics: unable to load %s\n", BHAPTICS_LIBRARY_NAME);
		return qfalse;
	}

	bhInitialise = (bh_init_f)BHAPTICS_LOAD_FUNC(bhapticsLibrary, "Initialise");
	bhDestroy = (bh_destroy_f)BHAPTICS_LOAD_FUNC(bhapticsLibrary, "Destroy");
	bhTurnOff = (bh_turnoff_f)BHAPTICS_LOAD_FUNC(bhapticsLibrary, "TurnOff");
	bhSubmitPathArray = (bh_submit_path_array_f)BHAPTICS_LOAD_FUNC(bhapticsLibrary, "SubmitPathArray");

	if (!bhInitialise || !bhDestroy || !bhTurnOff || !bhSubmitPathArray)
	{
		Com_Printf("bHaptics: missing required exports in %s\n", BHAPTICS_LIBRARY_NAME);
		BHAPTICS_CLOSE(bhapticsLibrary);
		bhapticsLibrary = NULL;
		return qfalse;
	}

	Com_Printf("bHaptics: successfully loaded %s\n", BHAPTICS_LIBRARY_NAME);
	return qtrue;
}

static void SubmitVestImpact(const char* key, float yaw, float height, int intensity, int durationMs)
{
	const float radians = NormalizeYaw(yaw) * (float)M_PI / 180.0f;
	const float x = Clamp01(0.5f + (sinf(radians) * 0.5f));
	const float y = HeightAs01(height);
	bh_point_t impactPoints[5];
	size_t pointCount = BuildImpactCluster(impactPoints, x, y, intensity, 6, 0.12f);

	bhSubmitPathArray(key, VestSideForYaw(yaw), impactPoints, pointCount, durationMs);
}

static void SubmitHandPulse(const char* key, qboolean rightHand, int intensity, int durationMs)
{
	bh_point_t handPoint;
	handPoint.x = 0.5f;
	handPoint.y = 0.5f;
	handPoint.intensity = intensity;
	handPoint.motorCount = 3;

	bhSubmitPathArray(key, rightHand ? BH_POS_HAND_R : BH_POS_HAND_L, &handPoint, 1, durationMs);
}

static void SubmitSleevePulse(const char* key, qboolean rightHand, int intensity, int durationMs)
{
	bh_point_t sleevePoint;
	sleevePoint.x = 0.5f;
	sleevePoint.y = 0.5f;
	sleevePoint.intensity = intensity;
	sleevePoint.motorCount = 3;

	bhSubmitPathArray(key, rightHand ? BH_POS_ARM_R : BH_POS_ARM_L, &sleevePoint, 1, durationMs);
}

static void SubmitVisorImpact(const char* key, float yaw, float height, int intensity, int durationMs)
{
	const float radians = NormalizeYaw(yaw) * (float)M_PI / 180.0f;
	const float x = Clamp01(0.5f + (sinf(radians) * 0.5f));
	const float y = HeightAs01(height);
	bh_point_t visorPoints[5];
	size_t pointCount = BuildImpactCluster(visorPoints, x, y, intensity, 4, 0.08f);

	bhSubmitPathArray(key, BH_POS_HEAD, visorPoints, pointCount, durationMs);
}

void VR_Bhaptics_Init(void)
{
	if (bhapticsInitialized)
	{
		Com_DPrintf("bHaptics: already initialized\n");
		return;
	}

	if (!vr_bhaptics || vr_bhaptics->integer == 0)
	{
		Com_DPrintf("bHaptics: disabled by vr_bhaptics\n");
		return;
	}

	if (!LoadBhapticsLibrary())
	{
		return;
	}

	bhInitialise("q3vr", "Quake 3 VR");
	bhapticsInitialized = qtrue;
	Com_Printf("bHaptics: initialized player connection\n");
}

void VR_Bhaptics_Shutdown(void)
{
	if (!bhapticsInitialized)
	{
		Com_DPrintf("bHaptics: shutdown skipped (not initialized)\n");
		return;
	}

	if (bhTurnOff)
	{
		bhTurnOff();
	}
	if (bhDestroy)
	{
		bhDestroy();
	}
	if (bhapticsLibrary)
	{
		BHAPTICS_CLOSE(bhapticsLibrary);
		bhapticsLibrary = NULL;
	}
	bhInitialise = NULL;
	bhDestroy = NULL;
	bhTurnOff = NULL;
	bhSubmitPathArray = NULL;

	Com_Printf("bHaptics: shutdown completed\n");
	bhapticsInitialized = qfalse;
}

void VR_Bhaptics_UpdateEnabled(void)
{
	if (!vr_bhaptics)
	{
		return;
	}

	if (vr_bhaptics->integer != 0)
	{
		VR_Bhaptics_Init();
	}
	else if (bhapticsInitialized)
	{
		VR_Bhaptics_Shutdown();
	}
}

void VR_Bhaptics_HandleEvent(const char* event, int position, int intensity, float yaw, float height)
{
	if (!event || !vr_bhaptics || vr_bhaptics->integer == 0)
	{
		return;
	}

	if (VR_GetHapticsIntensity() <= 0.0f)
	{
		return;
	}

	if (!bhapticsInitialized)
	{
		return;
	}

	const int scaledIntensity = ScaleSuitIntensity(intensity);
	if (scaledIntensity <= 0)
	{
		return;
	}

	const char* eventName = event;
	const char* headerSeparator = strchr(event, ':');
	if (headerSeparator)
	{
		eventName = headerSeparator + 1;
	}

	if (strncmp(eventName, "pickup_", 7) == 0)
	{
		SubmitVestImpact("bhaptics_pickup", 0.0f, 0.3f, scaledIntensity, 130);
		return;
	}

	if (strcmp(eventName, "weapon_switch") == 0)
	{
		SubmitHandPulse("bhaptics_weapon_switch", vr_righthanded && vr_righthanded->integer, scaledIntensity, 110);
		return;
	}

	if (strcmp(eventName, "menu_move") == 0)
	{
		SubmitHandPulse("bhaptics_menu_move", !vr.menuLeftHanded, scaledIntensity / 2, 60);
		return;
	}

	if (strcmp(eventName, "jump_start") == 0)
	{
		SubmitVestImpact("bhaptics_jump_start", yaw, height, scaledIntensity, 140);
		return;
	}

	if (strcmp(eventName, "jump_landing") == 0)
	{
		SubmitVestImpact("bhaptics_jump_land", yaw, height, scaledIntensity, 200);
		return;
	}

	if (strcmp(eventName, "spark") == 0 || strcmp(eventName, "shield_break") == 0)
	{
		SubmitVestImpact("bhaptics_shield", yaw, height, scaledIntensity, 220);
		SubmitVisorImpact("bhaptics_visor_impact", yaw, height, scaledIntensity, 160);
		return;
	}

	if (strcmp(eventName, "bullet") == 0 || strcmp(eventName, "shotgun") == 0 || strcmp(eventName, "fireball") == 0)
	{
		SubmitVestImpact("bhaptics_impact", yaw, height, scaledIntensity, 210);
		SubmitVisorImpact("bhaptics_visor_impact", yaw, height, scaledIntensity, 170);
		return;
	}

	if (strstr(eventName, "_fire") != NULL)
	{
		qboolean weaponRight = vr_righthanded && vr_righthanded->integer;
		if (position == 1)
		{
			weaponRight = qtrue;
		}
		else if (position == 2)
		{
			weaponRight = qfalse;
		}

		if (weaponRight)
		{
			SubmitHandPulse("bhaptics_fire_right", qtrue, scaledIntensity, 140);
			SubmitSleevePulse("bhaptics_sleeve_fire_right", qtrue, scaledIntensity, 140);
		}
		else
		{
			SubmitHandPulse("bhaptics_fire_left", qfalse, scaledIntensity, 140);
			SubmitSleevePulse("bhaptics_sleeve_fire_left", qfalse, scaledIntensity, 140);
		}
		return;
	}

	SubmitVestImpact("bhaptics_generic", yaw, height, scaledIntensity, 120);
}

#else

void VR_Bhaptics_Init(void) {}
void VR_Bhaptics_Shutdown(void) {}
void VR_Bhaptics_HandleEvent(const char* event, int position, int intensity, float yaw, float height)
{
	(void)event;
	(void)position;
	(void)intensity;
	(void)yaw;
	(void)height;
}

#endif
