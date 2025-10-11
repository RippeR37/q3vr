#ifndef __VR_SAFE_TYPES
#define __VR_SAFE_TYPES

// Rules:
// * no SDL
// * no OpenGL
// * no OpenXR

typedef enum
{
	VRFM_THIRDPERSON_1, // Camera will auto move to keep up with player
	VRFM_THIRDPERSON_2,	// Camera is completely free movement with the thumbstick
	VRFM_FIRSTPERSON,   // Obvious isn't it?..
	VRFM_NUM_FOLLOWMODES,

	VRFM_QUERY = 99	// Used to query which mode is active
} VR_FollowMode;

typedef enum
{
	WS_CONTROLLER,
	WS_HMD,
	WS_ALTKEY,
	WS_PREVNEXT
} VR_WeaponSelectorType;

typedef enum
{
	VR_Button_A           = 0x00000001, // Set for trigger pulled on the Gear VR and Go Controllers
	VR_Button_B           = 0x00000002,
	VR_Button_RThumb      = 0x00000004,
	VR_Button_RShoulder   = 0x00000008,

	VR_Button_X           = 0x00000100,
	VR_Button_Y           = 0x00000200,
	VR_Button_LThumb      = 0x00000400,
	VR_Button_LShoulder   = 0x00000800,

	VR_Button_Up          = 0x00010000,
	VR_Button_Down        = 0x00020000,
	VR_Button_Left        = 0x00040000,
	VR_Button_Right       = 0x00080000,
	VR_Button_Enter       = 0x00100000, //< Set for touchpad click on the Go Controller, menu
	// button on Left Quest Controller
	VR_Button_Back        = 0x00200000, //< Back button on the Go Controller (only set when
	// a short press comes up)
	VR_Button_Thumbrest   = 0x01000000, //< Thumbrest touch sensor
	VR_Button_GripTrigger = 0x04000000, //< grip trigger engaged
	VR_Button_Trackpad    = 0x08000000, //< trackpad engaged
	VR_Button_Trigger     = 0x20000000, //< Index Trigger engaged
	VR_Button_Joystick    = 0x80000000, //< Click of the Joystick

	VR_Button_EnumSize = 0x7fffffff
} VR_Button;

#endif
