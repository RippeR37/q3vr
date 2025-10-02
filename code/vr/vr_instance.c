#include "vr_instance.h"

#include <string.h>
#include <stdio.h>

#include "../qcommon/q_shared.h"

#include "vr_macros.h"

XrResult VR_CreateInstance(const char* app_name, XrVersion api_version, uint32_t extensionsCount, const char* const* extensions, XrInstance* instance)
{
	XrApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	Q_strncpyz(appInfo.applicationName, app_name, sizeof(appInfo.applicationName));
	appInfo.applicationVersion = 1;
	Q_strncpyz(appInfo.engineName, app_name, sizeof(appInfo.engineName));
	appInfo.engineVersion = 1;
	appInfo.apiVersion = api_version;

	XrInstanceCreateInfo instanceCreateInfo;
	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.next = NULL;
	instanceCreateInfo.createFlags = 0;
	instanceCreateInfo.applicationInfo = appInfo;
	instanceCreateInfo.enabledApiLayerCount = 0;
	instanceCreateInfo.enabledApiLayerNames = NULL;
	instanceCreateInfo.enabledExtensionCount = extensionsCount;
	instanceCreateInfo.enabledExtensionNames = extensions;

	return xrCreateInstance(&instanceCreateInfo, instance);
}

XrResult VR_GetHMDSystem(XrInstance instance, XrSystemId* systemId)
{
	XrSystemGetInfo systemGetInfo;
	memset(&systemGetInfo, 0, sizeof(systemGetInfo));
	systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	systemGetInfo.next = NULL;
	systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

	return xrGetSystem(instance, &systemGetInfo, systemId);
}

XrResult VR_GetVersionRequirements(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsOpenGLKHR* requirements)
{
	PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
	
	XrResult result = xrGetInstanceProcAddr(
		instance,
		"xrGetOpenGLGraphicsRequirementsKHR",
		(PFN_xrVoidFunction*)(&pfnGetOpenGLGraphicsRequirementsKHR));
	if (!XR_SUCCEEDED(result))
	{
		return result;
	}

	requirements->type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
	return pfnGetOpenGLGraphicsRequirementsKHR(instance, systemId, requirements);
}

XrResult VR_GetSystemProperties(XrInstance instance, XrSystemId systemId, VR_SystemProperties* systemProperties)
{
	systemProperties->SystemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
	systemProperties->SystemProperties.next = NULL;

	XR_CHECK(
		xrGetSystemProperties(instance, systemId, &systemProperties->SystemProperties),
		"Failed to get SystemProperties");

	XR_CHECK(
		VR_GetVersionRequirements(instance, systemId, &systemProperties->GraphicsRequirements),
		"Failed to get OpenXR<->OpenGL system requirements");

	return XR_SUCCESS;
}
