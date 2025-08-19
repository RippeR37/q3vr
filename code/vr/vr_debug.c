#include "vr_debug.h"

#include <stdio.h>

#include "../renderergl2/tr_local.h"

#include "vr_base.h"
#include "vr_macros.h"

//
// Debug API Layers/Extensions listing
//

void VR_ListAPILayers( void )
{
	uint32_t layersCount = 0;
	XrResult result = xrEnumerateApiLayerProperties(0, &layersCount, NULL);

	if (result != XR_SUCCESS)
	{
		fprintf(stderr, "Failed to get API layers count: %d.", result);
		exit(1);
	}

	XrApiLayerProperties* layers = (XrApiLayerProperties*)malloc(sizeof(XrApiLayerProperties) * layersCount);
	if (!layers)
	{
		fprintf(stderr, "Failed to allocate memory for API layers\n");
		exit(1);
	}

	// Initialize type and next fields
	for (uint32_t i = 0; i < layersCount; ++i)
	{
		layers[i].type = XR_TYPE_API_LAYER_PROPERTIES;
		layers[i].next = NULL;
	}
	// Get the extension list
	result = xrEnumerateApiLayerProperties(layersCount, &layersCount, layers);
	if (result != XR_SUCCESS)
	{
		fprintf(stderr, "Failed to enumerate extensions: %d\n", result);
		free(layers);
		exit(1);
	}

	// Print extensions
	printf("Available OpenXR Layers:\n");
	for (uint32_t i = 0; i < layersCount; ++i)
	{
		printf("- %s (%s)\n", layers[i].layerName, layers[i].description);
	}

	free(layers);
}

void VR_ListExtensions( void )
{
	uint32_t extensionCount = 0;
	XrResult result = xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);

	if (result != XR_SUCCESS)
	{
		fprintf(stderr, "Failed to get extension count: %d.", result);
		exit(1);
	}

	XrExtensionProperties* extensions = (XrExtensionProperties*)malloc(sizeof(XrExtensionProperties) * extensionCount);
	if (!extensions)
	{
		fprintf(stderr, "Failed to allocate memory for extensions\n");
		exit(1);
	}

	// Initialize type and next fields
	for (uint32_t i = 0; i < extensionCount; ++i)
	{
		extensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		extensions[i].next = NULL;
	}
	// Get the extension list
	result = xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensions);
	if (result != XR_SUCCESS)
	{
		fprintf(stderr, "Failed to enumerate extensions: %d\n", result);
		free(extensions);
		exit(1);
	}

	// Print extensions
	printf("Available OpenXR Extensions:\n");
	for (uint32_t i = 0; i < extensionCount; ++i)
	{
		printf("- %s (Version %u)\n", extensions[i].extensionName, extensions[i].extensionVersion);
	}

	free(extensions);
}


const char* GetXRErrorString(XrResult result)
{
	VR_Engine* engine = VR_GetEngine();

	if (!engine || !engine->appState.Instance)
	{
		return "<UNKNOWN_NO_XR_INSTANCE>";
	}

	static char string[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString(engine->appState.Instance, result, string);
	return string;
}


//
// DebugUtilsMessenger
//

XrBool32 OpenXRMessageCallbackFunction(
	XrDebugUtilsMessageSeverityFlagsEXT messageSeverity, 
	XrDebugUtilsMessageTypeFlagsEXT messageType, 
	const XrDebugUtilsMessengerCallbackDataEXT *pCallbackData, 
	void *pUserData)
{
	fprintf(stderr, "[OpenXR][%llu][%llu][%s] %s - %s\n", 
		messageSeverity,
		messageType,
		((pCallbackData->messageId) ? pCallbackData->messageId : ""),
		((pCallbackData->functionName) ? pCallbackData->functionName : ""),
		((pCallbackData->message) ? pCallbackData->message : "")
	);
	return 0;
}

void VR_CreateDebugUtilsMessenger(XrInstance instance, XrDebugUtilsMessengerEXT* messenger)
{
	XrDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI;
	debugUtilsMessengerCI.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugUtilsMessengerCI.next = NULL;
	debugUtilsMessengerCI.messageSeverities = 
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | 
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugUtilsMessengerCI.messageTypes = 
		XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
		XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
		// XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
	debugUtilsMessengerCI.userCallback = (PFN_xrDebugUtilsMessengerCallbackEXT)OpenXRMessageCallbackFunction;
	debugUtilsMessengerCI.userData = NULL;

	// Load xrCreateDebugUtilsMessengerEXT() function pointer as it is not default loaded by the OpenXR loader.
	PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT;
	
	XR_CHECK(
		xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction *)&xrCreateDebugUtilsMessengerEXT),
		"Failed to get DebugUtilsMessenger proc");
	XR_CHECK(
		xrCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCI, messenger),
		"Failed to create DebugUtilsMessenger");
}

void VR_DestroyDebugUtilsMessenger(XrInstance instance, XrDebugUtilsMessengerEXT* messenger)
{
	PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT;
	if (XR_SUCCESS == xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction *)&xrDestroyDebugUtilsMessengerEXT))
	{
		xrDestroyDebugUtilsMessengerEXT(*messenger);
		*messenger = XR_NULL_HANDLE;
	}
}

//
// OpenGL part
//

void APIENTRY VR_GL_DebugLogSink(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

void VR_GL_RegisterDebugLogSinkIfEnabled(void)
{
#if VR_ENABLE_GL_DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	qglDebugMessageCallback(VR_GL_DebugLogSink, 0);
#endif
}

#if VR_ENABLE_GL_DEBUG
void APIENTRY VR_GL_DebugLogSink(GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR || VR_ENABLE_GL_DEBUG_VERBOSE)
	{
		char typeStr[128];
		switch (type)
		{
			case GL_DEBUG_TYPE_ERROR: sprintf(typeStr, "ERROR"); break;
			case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: sprintf(typeStr, "DEPRECATED_BEHAVIOR"); break;
			case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: sprintf(typeStr, "UNDEFINED_BEHAVIOR"); break;
			case GL_DEBUG_TYPE_PORTABILITY: sprintf(typeStr, "PORTABILITY"); break;
			case GL_DEBUG_TYPE_PERFORMANCE: sprintf(typeStr, "PERFORMANCE"); break;
			case GL_DEBUG_TYPE_MARKER: sprintf(typeStr, "MARKER"); break;
			case GL_DEBUG_TYPE_PUSH_GROUP: sprintf(typeStr, "PUSH_GROUP"); break;
			case GL_DEBUG_TYPE_POP_GROUP: sprintf(typeStr, "POP_GROUP"); break;
			default: sprintf(typeStr, "OTHER"); break;
		}

		char severinityStr[128];
		switch (severity)
		{
			case GL_DEBUG_SEVERITY_HIGH: sprintf(severinityStr, "HIGH"); break;
			case GL_DEBUG_SEVERITY_MEDIUM: sprintf(severinityStr, "MEDIUM"); break;
			case GL_DEBUG_SEVERITY_LOW: sprintf(severinityStr, "LOW"); break;
			default: return; sprintf(severinityStr, "VERBOSE"); break;
		}

		fprintf(stderr, "[%s] GL issue - %s: %s\n", severinityStr, typeStr, message);
	}
}
#endif
