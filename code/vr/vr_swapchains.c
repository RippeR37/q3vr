#include "vr_swapchains.h"

#include "../renderergl2/tr_local.h"

#include "vr_base.h"
#include "vr_macros.h"

extern cvar_t *vr_desktopContentFit;
extern cvar_t *vr_desktopMenuStyle;
extern cvar_t *vr_desktopMode;

//
// Helpers
//

XrViewConfigurationType VR_GetBestViewConfiguration(XrInstance instance, XrSystemId systemId)
{
	uint32_t viewConfigurationCount = 0;
	XR_CHECK(
		xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigurationCount, NULL),
		"Failed to count View Configurations");

	XrViewConfigurationType* viewConfigurations = calloc(viewConfigurationCount, sizeof(XrViewConfigurationType));
	XR_CHECK(
		xrEnumerateViewConfigurations(instance, systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations),
		"Failed to enumerate View Configurations");

	for (int idx = 0; idx < viewConfigurationCount; ++idx)
	{
		if (viewConfigurations[idx] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
		{
			free(viewConfigurations);
			return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		}
	}
	
	free(viewConfigurations);
	return XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
}

uint32_t VR_GetViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfig, XrViewConfigurationView** views)
{
	uint32_t viewCount = 0;
	XR_CHECK(
		xrEnumerateViewConfigurationViews(instance, systemId, viewConfig, 0, &viewCount, NULL), 
		"Failed to count ViewConfiguration Views");

	*views = calloc(viewCount, sizeof(XrViewConfigurationView));
	for (uint32_t idx = 0; idx < viewCount; ++idx)
	{
		(*views)[idx].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	}
	XR_CHECK(
		xrEnumerateViewConfigurationViews(instance, systemId, viewConfig, viewCount, &viewCount, *views), 
		"Failed to enumerate ViewConfiguration Views");

	return viewCount;
}

uint32_t VR_GetSwapchainFormats(XrSession session, int64_t** formats)
{
	uint32_t formatCount = 0;
	XR_CHECK(
		xrEnumerateSwapchainFormats(session, 0, &formatCount, NULL), 
		"Failed to count Swapchain formats");
	
	*formats = calloc(formatCount, sizeof(int64_t));
	XR_CHECK(
		xrEnumerateSwapchainFormats(session, formatCount, &formatCount, *formats),
		"Failed to enumerate Swapchain formats");

	return formatCount;
}

XrBool32 HasFormatInList(int64_t* formats, uint32_t formatCount, int64_t format)
{
	for (uint32_t idx = 0; idx < formatCount; ++idx)
	{
		if (formats[idx] == format)
		{
			return XR_TRUE;
		}
	}
	return XR_FALSE;
}

int64_t VR_GetBestColorSwapchainFormat(int64_t* formats, uint32_t formatCount)
{
	int64_t preferredColorFormats[] =
	{
		// Preferred
		GL_SRGB8_ALPHA8,

		// These will most likely be problematic due to lack of sRGB
		GL_RGBA16F,
		GL_RGBA8,
		GL_RGB10_A2,
		GL_RGBA8_SNORM,
	};
	const uint32_t preferredCount = sizeof(preferredColorFormats) / sizeof(preferredColorFormats[0]);

	for (size_t idx = 0; idx < preferredCount; ++idx)
	{
		if (HasFormatInList(formats, formatCount, preferredColorFormats[idx]))
		{
			if (idx >= 1)
			{
				fprintf(stderr, "WARNING: non-SRGB format chosen, colors may be wrong or washed out!\n");
				for (size_t fidx = 0; fidx < formatCount; ++fidx)
				{
					fprintf(stderr, "  available format: %llx\n", formats[fidx]);
				}
			}
			return preferredColorFormats[idx];
		}
	}

	CHECK(XR_FALSE, "Could not find any preferrable swapchain color format");
	return formats[0];
}

int64_t VR_GetBestDepthSwapchainFormat(int64_t* formats, uint32_t formatCount)
{
	int64_t preferredDepthFormats[] =
	{
		GL_DEPTH_COMPONENT32F,
		GL_DEPTH_COMPONENT32,
		GL_DEPTH_COMPONENT24,
		GL_DEPTH_COMPONENT16,
	};
	const uint32_t preferredCount = sizeof(preferredDepthFormats) / sizeof(preferredDepthFormats[0]);

	for (size_t idx = 0; idx < preferredCount; ++idx)
	{
		if (HasFormatInList(formats, formatCount, preferredDepthFormats[idx]))
		{
			return preferredDepthFormats[idx];
		}
	}

	CHECK(XR_FALSE, "Could not find any preferrable swapchain depth format");
	return formats[0];
}

// Framebuffer creation
GLuint VR_CreateImageView(GLuint colorImage, GLuint depthImage, uint32_t viewCount)
{
	const int baseMipLevel = 0;
	const int baseArrayLayer = 0;

	GLuint framebuffer = 0;
	qglGenFramebuffers(1, &framebuffer);
	CHECK(framebuffer != 0, "Failed to create GL framebuffer");

	qglBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	qglFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorImage, baseMipLevel, baseArrayLayer, viewCount);
	qglFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthImage, baseMipLevel, baseArrayLayer, viewCount);

	GLenum result = qglCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	CHECK(result == GL_FRAMEBUFFER_COMPLETE, "Failed to create complete Framebuffer");
	
	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	return framebuffer;
}

GLuint VR_CreateEyeImageView(GLuint colorImage, uint32_t eyeLayer)
{
	GLuint framebuffer;
	qglGenFramebuffers(1, &framebuffer);
	CHECK(framebuffer != 0, "Failed to create GL desktop framebuffer");

	qglBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	qglFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorImage, 0, eyeLayer);

	GLenum result = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
	CHECK(result == GL_FRAMEBUFFER_COMPLETE, "Failed to create complete desktop Framebuffer");

	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	return framebuffer;
}

GLuint VR_CreateVirtualScreenImageView(GLuint colorImage)
{
	GLuint framebuffer;
	qglGenFramebuffers(1, &framebuffer);
	CHECK(framebuffer != 0, "Failed to create GL virtual screen framebuffer");

	qglBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	qglFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorImage, 0);

	GLenum result = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
	CHECK(result == GL_FRAMEBUFFER_COMPLETE, "Failed to create complete virtual screen Framebuffer");

	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	return framebuffer;
}

GLuint VR_CreateImage2D(GLint format, GLsizei width, GLsizei height)
{
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	// Allocate storage matching swapchain
	glTexImage2D(
		GL_TEXTURE_2D,
		0,                   // mip level
		GL_RGB8,             // internal format matches swapchain
		width,
		height,
		0,                   // border
		GL_RGB,              // data
		GL_UNSIGNED_BYTE,    // type
		NULL                 // no initial data
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}

// Create a single-layer swapchain for screen overlays (quad layer)
void VR_CreateScreenOverlaySwapchain(XrSession session, int64_t format, const XrViewConfigurationView* view, VR_SwapchainInfo* swapchain_info, int supersampledWidth, int supersampledHeight)
{
	XrSwapchainCreateInfo swapchainCI;
	swapchainCI.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
	swapchainCI.next = NULL;
	swapchainCI.createFlags = 0;
	swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCI.format = format;
	swapchainCI.sampleCount = 1;
	swapchainCI.width = supersampledWidth;
	swapchainCI.height = supersampledHeight;
	swapchainCI.faceCount = 1;
	swapchainCI.arraySize = 1;  // Single layer, NOT multiview
	swapchainCI.mipCount = 1;

	XR_CHECK(
		xrCreateSwapchain(session, &swapchainCI, &swapchain_info->swapchain),
		"Failed to create screen overlay Swapchain");
	swapchain_info->swapchainFormat = swapchainCI.format;
	swapchain_info->width = swapchainCI.width;
	swapchain_info->height = swapchainCI.height;

	// Enumerate images
	uint32_t imageCount = 0;
	XR_CHECK(
		xrEnumerateSwapchainImages(swapchain_info->swapchain, 0, &imageCount, NULL),
		"Failed to count screen overlay Swapchain Images");

	XrSwapchainImageOpenGLKHR* swapchainImagesGL = calloc(imageCount, sizeof(XrSwapchainImageOpenGLKHR));
	for (uint32_t idx = 0; idx < imageCount; ++idx)
	{
		swapchainImagesGL[idx].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
	}
	XrSwapchainImageBaseHeader* swapchainImages = (XrSwapchainImageBaseHeader*)swapchainImagesGL;
	XR_CHECK(
		xrEnumerateSwapchainImages(swapchain_info->swapchain, imageCount, &imageCount, swapchainImages),
		"Failed to enumerate screen overlay Swapchain Images");

	swapchain_info->imageCount = imageCount;
	swapchain_info->images = calloc(imageCount, sizeof(uint32_t));
	for (uint32_t idx = 0; idx < imageCount; ++idx)
	{
		swapchain_info->images[idx] = swapchainImagesGL[idx].image;
	}
	swapchain_info->virtualScreenImage = 0; // Not used for overlay

	free(swapchainImagesGL);
}

// Create a simple 2D framebuffer for the screen overlay
GLuint VR_CreateScreenOverlayFramebuffer(GLuint colorImage)
{
	GLuint framebuffer;
	qglGenFramebuffers(1, &framebuffer);
	CHECK(framebuffer != 0, "Failed to create GL screen overlay framebuffer");

	qglBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	qglFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorImage, 0);

	GLenum result = qglCheckFramebufferStatus(GL_FRAMEBUFFER);
	CHECK(result == GL_FRAMEBUFFER_COMPLETE, "Failed to create complete screen overlay Framebuffer");

	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	return framebuffer;
}

void VR_CreateSwapchain(XrSession session, XrBool32 isColor, int64_t format, const XrViewConfigurationView* view, uint32_t viewCount, VR_SwapchainInfo* swapchain_info, int supersampledWidth, int supersampledHeight)
{
	XrSwapchainCreateInfo swapchainCI;
	swapchainCI.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
	swapchainCI.next = NULL;
	swapchainCI.createFlags = 0;
	if (isColor)
	{
		swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	}
	else
	{
		swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	swapchainCI.format = format;
	swapchainCI.sampleCount = 1;
	swapchainCI.width = supersampledWidth;
	swapchainCI.height = supersampledHeight;
	swapchainCI.faceCount = 1;
	swapchainCI.arraySize = viewCount;
	swapchainCI.mipCount = 1;
	
	XR_CHECK(
		xrCreateSwapchain(session, &swapchainCI, &swapchain_info->swapchain), 
		(isColor ? "Failed to create color Swapchain" : "Failed to create depth Swapchain"));
	swapchain_info->swapchainFormat = swapchainCI.format;
	swapchain_info->width = swapchainCI.width;
	swapchain_info->height = swapchainCI.height;

	//
	// Images
	//

	uint32_t imageCount = 0;
	XR_CHECK(
		xrEnumerateSwapchainImages(swapchain_info->swapchain, 0, &imageCount, NULL), 
		"Failed to count Swapchain Images");

	XrSwapchainImageOpenGLKHR* swapchainImagesGL = calloc(imageCount, sizeof(XrSwapchainImageOpenGLKHR));
	for (uint32_t idx = 0; idx < imageCount; ++idx)
	{
		swapchainImagesGL[idx].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
	}
	XrSwapchainImageBaseHeader* swapchainImages = (XrSwapchainImageBaseHeader*)swapchainImagesGL;
	XR_CHECK(
		xrEnumerateSwapchainImages(swapchain_info->swapchain, imageCount, &imageCount, swapchainImages),
		"Failed to enumerate Color Swapchain Images");

	swapchain_info->imageCount = imageCount;
	swapchain_info->images = calloc(imageCount, sizeof(uint32_t));
	for (uint32_t idx = 0; idx < imageCount; ++idx)
	{
		swapchain_info->images[idx] = swapchainImagesGL[idx].image;
		// we need to render virtual menus to separate images/FBOs and then re-render as textued quad/cylinder to XR swapchains
	}

	// Create a side texture that we will render menus etc. to and then we will use as source for actual frames in VR
	if (isColor)
	{
		swapchain_info->virtualScreenImage = VR_CreateImage2D(format, supersampledWidth, supersampledHeight);
	}

	free(swapchainImagesGL);
}


//
// Usable
//

float VR_GetSupersamplingFactor(void)
{
	// Some sane limits in case someone overridden cvars manually
	float supersampling = Cvar_VariableValue("vr_superSampling");
	if (supersampling < 0.5f) {
		supersampling = 0.5f;
	} else if (supersampling > 4.0f) {
		supersampling = 4.0f;
	}
	return supersampling;
}

void VR_GetRecommendedResolution(XrInstance instance, XrSystemId systemId, int* width, int* height, int* maxWidth, int* maxHeight)
{
	const XrViewConfigurationType viewConfigurationType = VR_GetBestViewConfiguration(instance, systemId);
	CHECK(
		viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM,
		"No required view configuration type supported");
	
	XrViewConfigurationView* views = NULL;
	VR_GetViewConfigurationViews(instance, systemId, viewConfigurationType, &views);

	*width = views[0].recommendedImageRectWidth;
	*height = views[0].recommendedImageRectHeight;

	if (maxWidth)
	{
		*maxWidth = views[0].maxImageRectWidth;
	}
	if (maxHeight)
	{
		*maxHeight = views[0].maxImageRectHeight;
	}

	free(views);
}

void VR_GetSupersampledResolution(XrInstance instance, XrSystemId systemId, int* width, int* height)
{
	int maxWidth = 0, maxHeight = 0;
	VR_GetRecommendedResolution(instance, systemId, width, height, &maxWidth, &maxHeight);

	const float supersamplingFactor = VR_GetSupersamplingFactor();
	int supersampledWidth = *width * supersamplingFactor;
	int supersampledHeight = *height * supersamplingFactor;

	if (supersampledWidth > maxWidth || supersampledHeight > maxHeight)
	{
		const float adjustedSupersamplingFactor = MIN(maxWidth / (float)*width, maxHeight / (float)*height);
		supersampledWidth = *width * adjustedSupersamplingFactor;
		supersampledHeight = *height * adjustedSupersamplingFactor;
	}

	*width = supersampledWidth;
	*height = supersampledHeight;
}

VR_SwapchainInfos VR_CreateSwapchains(XrInstance instance, XrSystemId systemId, XrSession session)
{
	const XrViewConfigurationType viewConfigurationType = VR_GetBestViewConfiguration(instance, systemId);
	CHECK(
		viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM,
		"No required view configuration type supported");

	XrViewConfigurationView* views = NULL;
	const uint32_t viewCount = VR_GetViewConfigurationViews(instance, systemId, viewConfigurationType, &views);

	int64_t* formats = NULL;
	const uint32_t formatCount = VR_GetSwapchainFormats(session, &formats);
	const int64_t colorFormat = VR_GetBestColorSwapchainFormat(formats, formatCount);
	const int64_t depthFormat = VR_GetBestDepthSwapchainFormat(formats, formatCount);
	free(formats);
	fprintf(stderr, "[OpenXR] Chosen GL formats: {color: %llx, depth: %llx}\n", colorFormat, depthFormat);

	//
	// OpenGL Multiview rendering feature sanity check
	//
	for (uint32_t idx = 0; idx < viewCount; ++idx)
	{
		CHECK(
			views[0].recommendedImageRectWidth == views[idx].recommendedImageRectWidth,
			"Failed sanity check for same image sizes in OpenGL Multiview rendering");
		CHECK(
			views[0].recommendedImageRectHeight == views[idx].recommendedImageRectHeight,
			"Failed sanity check for same image sizes in OpenGL Multiview rendering");
	}

	int supersampledWidth = views[0].recommendedImageRectWidth;
	int supersampledHeight = views[0].recommendedImageRectHeight;
	VR_GetSupersampledResolution(instance, systemId, &supersampledWidth, &supersampledHeight);

	//
	// Swapchains
	//
	VR_SwapchainInfos swapchains = {.viewCount = viewCount};
	VR_CreateSwapchain(session, XR_TRUE,  colorFormat, &views[0], viewCount, &swapchains.color, supersampledWidth, supersampledHeight);
	VR_CreateSwapchain(session, XR_FALSE, depthFormat, &views[0], viewCount, &swapchains.depth, supersampledWidth, supersampledHeight);
	fprintf(stderr, "[OpenXR] Created color and depth swapchains: %dx%d, %u images\n", swapchains.color.width, swapchains.color.height, swapchains.color.imageCount);

	// Create screen overlay swapchain (single-layer for quad layer)
	VR_CreateScreenOverlaySwapchain(session, colorFormat, &views[0], &swapchains.screenOverlay, supersampledWidth, supersampledHeight);
	fprintf(stderr, "[OpenXR] Created screen overlay swapchain: %dx%d, %u images\n",
		swapchains.screenOverlay.width, swapchains.screenOverlay.height, swapchains.screenOverlay.imageCount);

	//
	// Framebuffers
	//
	CHECK(swapchains.color.imageCount == swapchains.depth.imageCount, "");
	swapchains.framebuffers = calloc(swapchains.color.imageCount, sizeof(GLuint));
	swapchains.eyeFramebuffers = calloc(viewCount, sizeof(GLuint*));
	for (uint32_t view = 0; view < viewCount; ++view)
	{
		swapchains.eyeFramebuffers[view] = calloc(swapchains.color.imageCount, sizeof(GLuint));
	}

	for (uint32_t idx = 0; idx < swapchains.color.imageCount; ++idx)
	{
		swapchains.framebuffers[idx] = VR_CreateImageView(swapchains.color.images[idx], swapchains.depth.images[idx], viewCount);
		for (uint32_t view = 0; view < viewCount; ++view)
		{
			swapchains.eyeFramebuffers[view][idx] = VR_CreateEyeImageView(swapchains.color.images[idx], view);
		}
	}
	swapchains.virtualScreenFramebuffer = VR_CreateVirtualScreenImageView(swapchains.color.virtualScreenImage);

	// Create screen overlay framebuffer (use first swapchain image)
	swapchains.screenOverlayFramebuffer = VR_CreateScreenOverlayFramebuffer(swapchains.screenOverlay.images[0]);

	free(views);
	return swapchains;
}

void VR_DestroySwapchain(VR_SwapchainInfo* swapchain)
{
	free(swapchain->images);
	swapchain->imageCount = 0;
	swapchain->images = NULL;
	
	glDeleteTextures(1, &swapchain->virtualScreenImage);
	swapchain->virtualScreenImage = 0;

	XR_CHECK(
		xrDestroySwapchain(swapchain->swapchain),
		"Failed to destroy XR swapchain");
	swapchain->swapchain = XR_NULL_HANDLE;
}

void VR_DestroySwapchains(VR_SwapchainInfos* swapchains)
{
	for (uint32_t idx = 0; idx < swapchains->color.imageCount; ++idx)
	{
		qglDeleteFramebuffers(1, &swapchains->framebuffers[idx]);
		for (uint32_t view = 0; view < swapchains->viewCount; ++view)
		{
			qglDeleteFramebuffers(1, &swapchains->eyeFramebuffers[view][idx]);
		}
	}
	free(swapchains->framebuffers);
	for (uint32_t view = 0; view < swapchains->viewCount; ++view)
	{
		free(swapchains->eyeFramebuffers[view]);
	}
	free(swapchains->eyeFramebuffers);
	swapchains->framebuffers = NULL;
	swapchains->eyeFramebuffers = NULL;

	// Destroy screen overlay framebuffer
	if (swapchains->screenOverlayFramebuffer)
	{
		qglDeleteFramebuffers(1, &swapchains->screenOverlayFramebuffer);
		swapchains->screenOverlayFramebuffer = 0;
	}

	VR_DestroySwapchain(&swapchains->color);
	VR_DestroySwapchain(&swapchains->depth);
	VR_DestroySwapchain(&swapchains->screenOverlay);
}

void VR_Swapchains_BindFramebuffers(VR_SwapchainInfos* swapchains, uint32_t swapchainColorIndex, uint32_t swapchainDepthIndex)
{
	if (!swapchains)
	{
		qglBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		return;
	}

	CHECK(
		swapchainColorIndex < swapchains->color.imageCount,
		"Invalid swapchainImageIndex value - out of bounds");
	CHECK(
		swapchainDepthIndex < swapchains->depth.imageCount,
		"Invalid swapchainImageIndex value - out of bounds");

	qglBindFramebuffer(GL_DRAW_FRAMEBUFFER, swapchains->framebuffers[swapchainColorIndex]);
	qglFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, swapchains->color.images[swapchainColorIndex], 0, 0, 2);
	qglFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, swapchains->depth.images[swapchainDepthIndex], 0, 0, 2);
}

void VR_Swapchains_BlitXRToMainFbo(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex, XrDesktopViewConfiguration viewConfig, qboolean useVirtualScreen)
{
	// Skip desktop mirroring if disabled
	if (vr_desktopMode && vr_desktopMode->integer == 0)
	{
		return;
	}

	// If using virtual screen, blit it directly instead of the VR eye views
	if (useVirtualScreen && (!vr_desktopMenuStyle || vr_desktopMenuStyle->integer == 0))
	{
		const VR_Engine* engine = VR_GetEngine();
		const GLuint defaultFBO = 0;

		// Clear the framebuffer
		qglBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFBO);
		qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		qglClear(GL_COLOR_BUFFER_BIT);

		// Calculate 4:3 aspect ratio destination rectangle
		const float targetAspect = 4.0f / 3.0f;
		const float windowAspect = (float)engine->window.width / (float)engine->window.height;

		int dstX, dstY, dstWidth, dstHeight;
		if (windowAspect > targetAspect)
		{
			// Window is wider than 4:3 - pillarbox left/right
			dstHeight = engine->window.height;
			dstWidth = (int)(dstHeight * targetAspect);
			dstX = (engine->window.width - dstWidth) / 2;
			dstY = 0;
		}
		else
		{
			// Window is taller than 4:3 - letterbox top/bottom
			dstWidth = engine->window.width;
			dstHeight = (int)(dstWidth / targetAspect);
			dstX = 0;
			dstY = (engine->window.height - dstHeight) / 2;
		}

		// Blit the virtual screen texture directly to the desktop window
		qglBlitNamedFramebuffer(
			swapchains->virtualScreenFramebuffer,
			defaultFBO,
			0, 0, swapchains->color.width, swapchains->color.height,
			dstX, dstY, dstX + dstWidth, dstY + dstHeight,
			GL_COLOR_BUFFER_BIT,
			GL_LINEAR);
		return;
	}

	// We need to map VR swapchain image size ("VR frame") to desktop window.
	// We know that `windowSize <= vrFrameSize`, so let's scale it nicely.

	const VR_Engine* engine = VR_GetEngine();

	const int maxParts = 2;
	const int parts = (swapchains->viewCount > 1) ? (1 + (viewConfig == BOTH_EYES)) : 1;

	// Clear the default framebuffer to prevent old images from showing
	// (especially when switching from both eyes to one eye in fullscreen)
	const GLuint defaultFBO = 0;
	qglBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFBO);
	qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	qglClear(GL_COLOR_BUFFER_BIT);

	// This fills the window/screen with one or both eyes possibly cropping sides
	// or top/bottom of the source image to preserve aspect ratio.
	if (!vr_desktopContentFit || vr_desktopContentFit->integer == 1)
	{
		for (uint32_t part = 0; part < maxParts; ++part)
		{
			if (!(viewConfig & (part + 1)))
			{
				continue;
			}
			const float tX = swapchains->color.width;
			const float tY = swapchains->color.height;
			const float wX = engine->window.width / parts;
			const float wY = engine->window.height;

			const float ratioT = tX / tY;
			const float ratioW = wX / wY;

			const int leftOffset = (parts == 2) ? part * wX : 0;

			int cutX = 0, cutY = 0, offsetX = 0, offsetY = 0;
			if (ratioT > ratioW)
			{
				// texture is wider, crop sides
				cutY = tY;
				cutX = cutY * ratioW;
				offsetX = (tX - cutX) / 2;
			}
			else
			{
				// texture is taller, crop top/bottom
				cutX = tX;
				cutY = cutX / ratioW;
				offsetY = (tY - cutY) / 2;
			}

			qglBlitNamedFramebuffer(
				swapchains->eyeFramebuffers[part][swapchainImageIndex],
				defaultFBO, 
				offsetX, offsetY, offsetX + cutX, offsetY + cutY,
				leftOffset, 0, leftOffset+ engine->window.width / parts, engine->window.height,
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST);
		}
		return;
	}

	// This fits entire source image inside the window/screen preserving aspect
	// ratio, which may result in black bars on sides or top/bottom area.
	for (uint32_t part = 0; part < maxParts; ++part)
	{
		if (!(viewConfig & (part + 1)))
		{
			continue;
		}
		const float tX = swapchains->color.width;
		const float tY = swapchains->color.height;
		const float wX = engine->window.width / parts;
		const float wY = engine->window.height;

		const float ratioT = tX / tY;
		const float ratioW = wX / wY;

		const int leftOffset = (parts == 2) ? part * wX : 0;

		// Use full source texture
		int srcX = 0, srcY = 0;
		int srcWidth = tX, srcHeight = tY;

		// Calculate destination dimensions to fit source while maintaining aspect ratio
		int dstX, dstY, dstWidth, dstHeight;
		if (ratioT > ratioW)
		{
			// Source is wider than window - fit to width, letterbox top/bottom
			dstWidth = wX;
			dstHeight = dstWidth / ratioT;
			dstX = leftOffset;
			dstY = (wY - dstHeight) / 2;
		}
		else
		{
			// Source is taller than window - fit to height, pillarbox left/right
			dstHeight = wY;
			dstWidth = dstHeight * ratioT;
			dstX = leftOffset + (wX - dstWidth) / 2;
			dstY = 0;
		}

		qglBlitNamedFramebuffer(
			swapchains->eyeFramebuffers[part][swapchainImageIndex],
			defaultFBO,
			srcX, srcY, srcX + srcWidth, srcY + srcHeight,
			dstX, dstY, dstX + dstWidth, dstY + dstHeight,
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST);
	}
}

void VR_Swapchains_BlitXRToVirtualScreen(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex)
{
	extern vr_clientinfo_t vr;

	// Always crop to 4:3 since the virtual screen display surface is 4:3 aspect ratio
	// (see _VR_GetVirtualScreenModelMatrix which scales Y by 3/4)
	// Without this, the full VR framebuffer would be squished to fit the 4:3 surface

	// Calculate the maximum 4:3 area that fits within the framebuffer
	// For ultra-wide headsets (e.g., Pimax 8KX with ~2:1 ratio), we may be height-limited
	int fbWidth = swapchains->color.width;
	int fbHeight = swapchains->color.height;

	int srcWidth, srcHeight, srcX, srcY;
	int heightFromWidth = (fbWidth * 3) / 4;  // 4:3 height if we use full width
	int widthFromHeight = (fbHeight * 4) / 3; // 4:3 width if we use full height

	if (heightFromWidth <= fbHeight) {
		// Normal case: width-limited, full width fits with 4:3 height
		srcWidth = fbWidth;
		srcHeight = heightFromWidth;
		srcX = 0;
	} else {
		// Ultra-wide case: height-limited, constrain width to fit 4:3
		srcHeight = fbHeight;
		srcWidth = widthFromHeight;
		srcX = (fbWidth - srcWidth) / 2; // Center horizontally
	}

	// Calculate vertical offset to center the crop on the optical center rather than
	// the geometric center. VR headsets have asymmetric FOV (more down-look than up-look).
	//
	// The projection center offset in virtual 480-height coords is: 240 * m9
	// where m9 = (tanUp + tanDown) / (tanUp - tanDown)
	// Scale this to framebuffer pixels using the same ratio as HUD code uses.
	float geometricCenterY = fbHeight / 2.0f;
	float opticalOffset = 0.0f;

	float tanUp = tanf(vr.fov_angle_up);
	float tanDown = tanf(vr.fov_angle_down);
	float tanHeight = tanUp - tanDown;

	if (fabsf(tanHeight) > 0.001f) {
		float m9 = (tanUp + tanDown) / tanHeight;
		// Offset in virtual 480 coords: 240 * m9 (negative when optical center is above geometric)
		// Scale to framebuffer pixels: multiply by (srcHeight / 480)
		// But Y coords are flipped (OpenGL Y=0 at bottom vs screen Y=0 at top), so negate
		opticalOffset = -240.0f * m9 * (srcHeight / 480.0f);
	}

	// Center the crop around the optical center, not the geometric center
	srcY = (int)(geometricCenterY - srcHeight / 2.0f + opticalOffset);

	// Clamp to valid framebuffer bounds (should only be needed for optical offset adjustment)
	if (srcY < 0) srcY = 0;
	if (srcY + srcHeight > fbHeight) {
		srcY = fbHeight - srcHeight;
	}

	// Blit the source region to fill the entire virtual screen texture
	qglBlitNamedFramebuffer(
		swapchains->eyeFramebuffers[0][swapchainImageIndex],
		swapchains->virtualScreenFramebuffer,
		srcX, srcY, srcX + srcWidth, srcY + srcHeight,
		0, 0, swapchains->color.width, swapchains->color.height,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);
}

void VR_Swapchains_Acquire(VR_SwapchainInfos* swapchainInfos, uint32_t* colorIndex, uint32_t* depthIndex)
{
	XrSwapchain swapchains[2] = {swapchainInfos->color.swapchain, swapchainInfos->depth.swapchain};
	uint32_t* indexPtrs[2] = {colorIndex, depthIndex};
	for (uint32_t idx = 0; idx < 2; ++idx)
	{
		XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};

		XR_CHECK(
			xrAcquireSwapchainImage(swapchains[idx], &acquireInfo, indexPtrs[idx]),
			"Failed to acquire swapchain image");

		XrSwapchainImageWaitInfo waitInfo;
		waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
		waitInfo.next = NULL;
		waitInfo.timeout = XR_INFINITE_DURATION;
		CHECK(
			!XR_FAILED(xrWaitSwapchainImage(swapchains[idx], &waitInfo)), 
			"Failed to wait for swapchain image");
	}
}

void VR_Swapchains_Release(VR_SwapchainInfos* swapchainInfos)
{
	XrSwapchain swapchains[2] = {swapchainInfos->color.swapchain, swapchainInfos->depth.swapchain};

	for (uint32_t idx = 0; idx < 2; ++idx)
	{
		XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
		XR_CHECK(
			xrReleaseSwapchainImage(swapchains[idx], &releaseInfo),
			"Failed to release swapchain image");
	}
}

//
// Screen overlay (quad layer) swapchain functions
//

void VR_Swapchains_AcquireOverlay(VR_SwapchainInfo* overlay, uint32_t* index)
{
	XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};

	XR_CHECK(
		xrAcquireSwapchainImage(overlay->swapchain, &acquireInfo, index),
		"Failed to acquire overlay swapchain image");

	XrSwapchainImageWaitInfo waitInfo;
	waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
	waitInfo.next = NULL;
	waitInfo.timeout = XR_INFINITE_DURATION;
	CHECK(
		!XR_FAILED(xrWaitSwapchainImage(overlay->swapchain, &waitInfo)),
		"Failed to wait for overlay swapchain image");
}

void VR_Swapchains_ReleaseOverlay(VR_SwapchainInfo* overlay)
{
	XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
	XR_CHECK(
		xrReleaseSwapchainImage(overlay->swapchain, &releaseInfo),
		"Failed to release overlay swapchain image");
}

void VR_Swapchains_BindOverlayFramebuffer(VR_SwapchainInfos* swapchains, uint32_t index)
{
	if (!swapchains)
	{
		qglBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		return;
	}

	// Bind the overlay framebuffer and re-attach the correct swapchain image
	qglBindFramebuffer(GL_DRAW_FRAMEBUFFER, swapchains->screenOverlayFramebuffer);
	qglFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		swapchains->screenOverlay.images[index], 0);
}

GLuint VR_Swapchains_GetOverlayFramebuffer(VR_SwapchainInfos* swapchains)
{
	return swapchains ? swapchains->screenOverlayFramebuffer : 0;
}
