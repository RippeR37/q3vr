#include "vr_virtual_screen.h"

#include "common/xr_linear.h"

extern cvar_t *vr_virtualScreenMode;
extern cvar_t *vr_virtualScreenShape;

typedef enum
{
	CURVED = 0,
	FLAT = 1,
} virtualScreenShape_t;

// Flat VirtualScreen
float quadVertices[] =
{
	// position            uv
	 0.5f,  0.5f, 0.0f,    1.0f, 1.0f,  // top right
	 0.5f, -0.5f, 0.0f,    1.0f, 0.0f,  // bottom right
	-0.5f, -0.5f, 0.0f,    0.0f, 0.0f,  // bottom left
	-0.5f,  0.5f, 0.0f,    0.0f, 1.0f,  // top left 
};
unsigned int quadIndices[] =
{
	0, 1, 3,   // first triangle
	1, 2, 3,   // second triangle
};

const char* vsVertexShaderSource =
	"#version 430 core\n"
	"\n"
	"#define NUM_VIEWS 2\n"
	"#extension GL_OVR_multiview2 : enable\n"
	"layout(num_views=NUM_VIEWS) in;\n"
	"\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 1) in vec2 aTexCoord;\n"
	"\n"
	"out vec2 TexCoord;\n"
	"\n"
	"layout (location = 2) uniform mat4 model;\n"
	"layout (location = 3) uniform mat4 view[2];\n"
	"layout (location = 5) uniform mat4 proj;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    gl_Position = (proj * (view[gl_ViewID_OVR] * (model * vec4(aPos.x, aPos.y, aPos.z, 1.0))));\n"
	"    TexCoord = aTexCoord;\n"
	"}\n";
const char* vsFragmentShaderSource =
	"#version 330 core\n"
	"out vec4 FragColor;\n"
	"\n"
	"in vec2 TexCoord;\n"
	"\n"
	"uniform sampler2D renderedTexture;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    FragColor = texture(renderedTexture, TexCoord);\n"
	"}\n";

const char* floorVertexShaderSource =
	"#version 430 core\n"
	"\n"
	"#define NUM_VIEWS 2\n"
	"#extension GL_OVR_multiview2 : enable\n"
	"layout(num_views=NUM_VIEWS) in;\n"
	"\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 1) in vec2 aTexCoord;\n"
	"\n"
	"out vec2 Pos;\n"
	"\n"
	"layout (location = 2) uniform mat4 model;\n"
	"layout (location = 3) uniform mat4 view[2];\n"
	"layout (location = 5) uniform mat4 proj;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    gl_Position = (proj * (view[gl_ViewID_OVR] * (model * vec4(aPos.xzy, 1.0))));\n"
	"    Pos = aTexCoord - vec2(0.5);"
	"}\n";
const char* floorFragmentShaderSource =
	"#version 330 core\n"
	"out vec4 FragColor;\n"
	"\n"
	"in vec2 Pos;\n"
	"\n"
	"uniform vec3 camera;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    vec2 P = Pos * 30.0;\n"
	"\n"
	"    vec2 f = fract(P);\n"
	"    vec2 d = min(f, 1.0 - f);\n"
	"    vec2 w = max(fwidth(P) * 1.5, vec2(1e-6));\n"
	"    vec2 a = 1.0 - smoothstep(vec2(0.0), w, d);\n"
	"    float lineAlpha = max(a.x, a.y);\n"
	"    float dotRadius = 0.02;\n"
	"    float dist = length(d);\n"
	"    float dotAlpha =  1.0 - smoothstep(0.0, dotRadius, dist);\n"
	"\n"
	"    vec4 background = vec4(0.0);\n"
	"    vec4 gridGrey = vec4(vec3(0.075), 1.0);\n"
	"    vec4 dotWhite = vec4(vec3(0.5), 1.0);\n"
	"\n"
	"    vec4 col1 = mix(background, gridGrey, lineAlpha);\n"
	"    vec4 col2 = mix(background, dotWhite, dotAlpha);\n"
	"\n"
	"    float distToCamera = length(vec3(P.x, 0.0, P.y) - camera);\n"
	"\n"
	"    FragColor = max(col1, col2);\n"
	"\n"
	"    FragColor.gb *= 1.0 - (distToCamera / 5);\n"
	"    FragColor.a *= 1.0 - (distToCamera / 15);\n"
	"}\n";

int cylinderVertexCount = 0;
int cylinderIndexCount = 0;
int quadVertexCount = sizeof(quadVertices)/sizeof(quadVertices[0]);
int quadIndexCount = sizeof(quadIndices)/sizeof(quadIndices[0]);

unsigned int cylinderVBO = 0;
unsigned int cylinderVAO = 0;
unsigned int cylinderEBO = 0;
unsigned int quadVBO = 0;
unsigned int quadVAO = 0;
unsigned int quadEBO = 0;

unsigned int vsShaderProgram = 0;
unsigned int floorShaderProgram = 0;

unsigned int _VR_CreateAndCompileShader(GLenum shaderType, const GLchar** source)
{
	const unsigned int shader = qglCreateShader(shaderType);
	CHECK(shader != 0, "Failed to create shader");
	qglShaderSource(shader, 1, source, NULL);
	qglCompileShader(shader);

	int success;
	char infoLog[512];
	qglGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success)
	{
		qglGetShaderInfoLog(shader, 512, NULL, infoLog);
		fprintf(stderr, "Failed to compile shader:\n%s\n", infoLog);
		exit(1);
	}
	return shader;
}

unsigned int _VR_CreateAndLinkShaderProgram(unsigned int vertexShader, unsigned int fragmentShader)
{
	const unsigned int program = qglCreateProgram();
	qglAttachShader(program, vertexShader);
	qglAttachShader(program, fragmentShader);
	qglLinkProgram(program);

	int success;
	char infoLog[512];
	qglGetProgramiv(program, GL_LINK_STATUS, &success);
	if(!success)
	{
		qglGetProgramInfoLog(program, 512, NULL, infoLog);
		fprintf(stderr, "Failed to link program:\n%s\n", infoLog);
		exit(1);
	}
	return program;
}

void generateCylinderSectionSimple(
	float radius, float height, float arcAngle, int segments,
	float** outVertices, int* outVertexCount,
	unsigned int** outIndices, int* outIndexCount)
{
	int vertsPerRow = segments + 1;
	int totalVerts = vertsPerRow * 2;
	int floatsPerVert = 5;

	*outVertexCount = totalVerts;
	*outVertices = (float*)malloc(totalVerts * floatsPerVert * sizeof(float));

	int totalIndices = segments * 6;
	*outIndexCount = totalIndices;
	*outIndices = (unsigned int*)malloc(totalIndices * sizeof(unsigned int));

	float* vptr = *outVertices;
	unsigned int* iptr = *outIndices;

	float startAngle = -arcAngle * 0.5f;
	float dTheta = arcAngle / (float)segments;

	for (int i = 0; i <= segments; i++)
	{
		float theta = startAngle + i * dTheta;

		float x = radius * sinf(theta);
		float z = -radius * cosf(theta);
		float u = (float)i / (float)segments;

		*vptr++ = x; *vptr++ =  height * 0.5f; *vptr++ = z;
		*vptr++ = u; *vptr++ = 1.0f;

		*vptr++ = x; *vptr++ = -height * 0.5f; *vptr++ = z;
		*vptr++ = u; *vptr++ = 0.0f;
	}

	for (int i = 0; i < segments; i++)
	{
		int top0 = i * 2;
		int bot0 = top0 + 1;
		int top1 = top0 + 2;
		int bot1 = bot0 + 2;

		*iptr++ = top0; *iptr++ = top1; *iptr++ = bot0;
		*iptr++ = bot0; *iptr++ = top1; *iptr++ = bot1;
	}
}

unsigned int _VR_CreateVaoAndProgram(
	unsigned int* VAO, 
	unsigned int* VBO, 
	unsigned int* EBO,
	float* vertices,
	unsigned int vertexCount,
	unsigned int* indices,
	unsigned int indexCount,
	const char* vertexShaderSrc,
	const char* framgnetShaderSrc)
{
	// VAO
	qglGenVertexArrays(1, VAO);
	CHECK(VAO != 0, "Failed to create VAO");
	qglBindVertexArray(*VAO);
	// VBO
	qglGenBuffers(1, VBO);
	qglBindBuffer(GL_ARRAY_BUFFER, *VBO);
	qglBufferData(GL_ARRAY_BUFFER, vertexCount * 5 * sizeof(float), vertices, GL_STATIC_DRAW);
	qglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	qglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	qglEnableVertexAttribArray(0);
	qglEnableVertexAttribArray(1);
	// EBO
	qglGenBuffers(1, EBO);
	CHECK(EBO != 0, "Failed to create EBO");
	qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *EBO);
	qglBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, GL_STATIC_DRAW);
	// Shaders
	unsigned int vertexShader = _VR_CreateAndCompileShader(GL_VERTEX_SHADER, &vertexShaderSrc);
	unsigned int fragmentShader = _VR_CreateAndCompileShader(GL_FRAGMENT_SHADER, &framgnetShaderSrc);
	unsigned int shaderProgram = _VR_CreateAndLinkShaderProgram(vertexShader, fragmentShader);
	qglDeleteShader(vertexShader);
	qglDeleteShader(fragmentShader);

	return shaderProgram;
}

void VR_VirtualScreen_Init(void)
{
	CHECK(cylinderVBO == 0, "Can be called only once");
	CHECK(cylinderVBO == 0, "Can be called only once");

	// Virtual screen
	float* cylinderVertices;
	unsigned int* cylinderIndices;
	generateCylinderSectionSimple(2.5, 3.5f, M_PI / 2.0f, 64, &cylinderVertices, &cylinderVertexCount, &cylinderIndices, &cylinderIndexCount);

	vsShaderProgram = _VR_CreateVaoAndProgram(
		&cylinderVAO, &cylinderVBO, &cylinderEBO,
		cylinderVertices, cylinderVertexCount,
		cylinderIndices, cylinderIndexCount,
		vsVertexShaderSource, vsFragmentShaderSource
	);

	free(cylinderVertices);
	free(cylinderIndices);

	// Floor
	floorShaderProgram = _VR_CreateVaoAndProgram(
		&quadVAO, &quadVBO, &quadEBO, 
		quadVertices, quadVertexCount,
		quadIndices, quadIndexCount,
		floorVertexShaderSource, floorFragmentShaderSource
	);

	// Unbind VAO to ensure nobody modifies it
	qglBindVertexArray(0);
}

void VR_VirtualScreen_Destroy(void)
{
	qglDeleteProgram(vsShaderProgram);
	qglDeleteProgram(floorShaderProgram);
	vsShaderProgram = 0;
	floorShaderProgram = 0;

	qglDeleteBuffers(1, &cylinderEBO);
	qglDeleteBuffers(1, &cylinderVBO);
	qglDeleteVertexArrays(1, &cylinderVAO);
	cylinderEBO = 0;
	cylinderVBO = 0;
	cylinderVAO = 0;

	qglDeleteBuffers(1, &quadEBO);
	qglDeleteBuffers(1, &quadVBO);
	qglDeleteVertexArrays(1, &quadVAO);
	quadEBO = 0;
	quadVBO = 0;
	quadVAO = 0;
}

static inline void XrQuaternionf_Transform(
	XrVector3f* result,
	const XrQuaternionf* q,
	const XrVector3f* v)
{
	// Quaternion components
	const float qx = q->x;
	const float qy = q->y;
	const float qz = q->z;
	const float qw = q->w;

	// t = 2 * cross(q.xyz, v)
	const float tx = 2.0f * (qy * v->z - qz * v->y);
	const float ty = 2.0f * (qz * v->x - qx * v->z);
	const float tz = 2.0f * (qx * v->y - qy * v->x);

	// v' = v + qw * t + cross(q.xyz, t)
	result->x = v->x + qw * tx + (qy * tz - qz * ty);
	result->y = v->y + qw * ty + (qz * tx - qx * tz);
	result->z = v->z + qw * tz + (qx * ty - qy * tx);
}

XrVector3f GetPositionInFront(const XrPosef* pose, float distance)
{
	// Forward vector in local/headset space
	XrVector3f forward = { 0.0f, 0.0f, -1.0f }; // OpenXR uses -Z as forward

	// Rotate it by headset orientation
	XrVector3f rotatedForward;
	XrQuaternionf_Transform(&rotatedForward, &pose->orientation, &forward);

	// Project to XZ plane (remove Y component to keep same height)
	rotatedForward.y = 0.0f;

	// Normalize to unit vector
	XrVector3f_Normalize(&rotatedForward);

	// Scale by desired distance
	rotatedForward.x *= distance;
	rotatedForward.y *= distance;
	rotatedForward.z *= distance;

	// Final world position = headset pos + forward offset
	XrVector3f result = {
		pose->position.x + rotatedForward.x,
		pose->position.y, // keep same height
		pose->position.z + rotatedForward.z
	};

	return result;
}

static inline XrQuaternionf YawFacingQuaternion(
	const XrVector3f* from,   // plane position
	const XrVector3f* to)     // headset position
{
	// Direction vector (XZ plane only)
	float dx = to->x - from->x;
	float dz = to->z - from->z;

	// Normalize
	float len = sqrtf(dx*dx + dz*dz);
	if (len < 1e-6f)
	{
		// fallback: no rotation
		return (XrQuaternionf){0,0,0,1};
	}
	dx /= len;
	dz /= len;

	// Yaw angle (atan2: X then Z)
	float yaw = atan2f(dx, dz);

	// Quaternion from yaw rotation around Y
	float half = yaw * 0.5f;
	return (XrQuaternionf){
		0.0f,
		sinf(half),
		0.0f,
		cosf(half)
	};
}

int positionsInitialized = 0;
int updateTarget = 1;
XrVector3f currentPosition;
XrVector3f targetPosition;
const float targetDistance = 2.5f;

void EnsureNewPositionInExpectedDistance(XrVector3f* hmdPosition, XrVector3f* vsPosition)
{
	XrVector3f virtualScreenToHmd;
	XrVector3f_Sub(&virtualScreenToHmd, vsPosition, hmdPosition);
	const float distance = XrVector3f_Length(&virtualScreenToHmd);

	const float delta = (distance > targetDistance ? (distance - targetDistance) : (targetDistance - distance));
	if (delta > 0.001)
	{
		XrVector3f scaledVirtualScreenToHmd;
		XrVector3f_Normalize(&virtualScreenToHmd);
		XrVector3f_Scale(&scaledVirtualScreenToHmd, &virtualScreenToHmd, targetDistance);
		XrVector3f_Add(vsPosition, hmdPosition, &scaledVirtualScreenToHmd);
	}
}

XrVector3f GetCurrentVirtualScreenPosition(XrPosef* leftEyePose)
{
	XrVector3f positionInFront = GetPositionInFront(leftEyePose, targetDistance);

	if (!positionsInitialized)
	{
		currentPosition = positionInFront;
		targetPosition = positionInFront;
	}
	else if (vr_virtualScreenMode->integer == 1)
	{
		XrVector3f lastToCurrentVec;
		XrVector3f_Sub(&lastToCurrentVec, &targetPosition, &positionInFront);

		const float updateTargetDist = XrVector3f_Length(&lastToCurrentVec);
		if (updateTargetDist < 0.1f)
		{
			updateTarget = false;
		}
		else if (updateTargetDist > 1.5f || updateTarget)
		{
			targetPosition = positionInFront;
			updateTarget = true;

			if (updateTargetDist > 3.0f)
			{
				// We're too far, just teleport it in front, we probably just started or switched to VS.
				currentPosition = targetPosition;
			}
		}

		XrVector3f lerped;
		XrVector3f_Lerp(&lerped, &currentPosition, &targetPosition, 0.01);
		EnsureNewPositionInExpectedDistance(&leftEyePose->position, &lerped);
		currentPosition = lerped;
	}
	
	positionsInitialized = 1;
	return currentPosition;
}

XrQuaternionf lastKnownVirtualScreenOrientation = { 0.0f, 0.0f, 0.0f, 1.0f };

void _VR_GetVirtualScreenModelMatrix(XrMatrix4x4f* model, XrPosef* leftEyePose)
{
	// Base 4:3 aspect ratio for the virtual screen content
	float aspectRatioCoeff = 3.0f / 4.0f;

	// Correct for FOV asymmetry: when fov_y > fov_x, vertical angles are compressed
	// in the projection, making content appear shorter. We compensate by making the
	// quad taller by the FOV aspect ratio.
	if (vr.fov_x > 0.0f && vr.fov_y > 0.0f)
	{
		float fovAspect = vr.fov_y / vr.fov_x;
		aspectRatioCoeff *= fovAspect;
	}

	XrVector3f translation = GetCurrentVirtualScreenPosition(leftEyePose);
	XrQuaternionf rotation = YawFacingQuaternion(&translation, &leftEyePose->position);
	XrVector3f scale = {3.0f, 3.0f * aspectRatioCoeff, 3.0f};

	lastKnownVirtualScreenOrientation = rotation;
	
	// Fixup to lower slightly, do it after all computations so it won't affect recomputations
	translation.y -= 0.2;

	XrMatrix4x4f_CreateTranslationRotationScale(model, &translation, &rotation, &scale);
}

void _VR_GetVirtualScreenFloorModelMatrix(XrMatrix4x4f* model)
{
	XrVector3f translation = { 0.0f, 0.0f, 0.0f };
	XrQuaternionf rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
	XrVector3f scale = { 30.0f, 30.0f, -30.0f };

	// Align the floor with actual stage space instead of initial HMD rotation
	XrVector3f upAxis = { 0.0f, 1.0f, 0.0f };
	XrQuaternionf_CreateFromAxisAngle(&rotation, &upAxis, -vr.recenterYaw);
	
	XrMatrix4x4f_CreateTranslationRotationScale(model, &translation, &rotation, &scale);
}

void _VR_GetVirtualScreenViewMatrix(XrMatrix4x4f* result, XrVector3f* translation, XrQuaternionf* rotation)
{
	XrMatrix4x4f rotationMatrix, translationMatrix, viewMatrix;
	XrMatrix4x4f_CreateFromQuaternion(&rotationMatrix, rotation);
	XrMatrix4x4f_CreateTranslation(&translationMatrix, translation->x, translation->y, translation->z);
	XrMatrix4x4f_Multiply(&viewMatrix, &translationMatrix, &rotationMatrix);
	XrMatrix4x4f_Invert(result, &viewMatrix);
}

void VR_VirtualScreen_ResetPosition(void)
{
	positionsInitialized = 0;
	updateTarget = 1;
}

void VR_VirtualScreen_Draw(XrFovf fov, XrPosef* left, XrPosef* right, GLuint virtualScreenImage)
{
	GLuint previousVAO, previousProgram, previousTexture;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint*)&previousVAO);
	glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&previousProgram);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&previousTexture);

	const GLboolean blendingEnabled = glIsEnabled(GL_BLEND);
	if (!blendingEnabled)
	{
		qglEnable(GL_BLEND);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	GLboolean depthMask;
	qglGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
	if (depthMask == GL_TRUE)
	{
		qglDepthMask(GL_FALSE);
	}

	// Compute Model, View(s) and Projection matrices
	XrMatrix4x4f model, view[2], projection;

	XrMatrix4x4f_CreateIdentity(&model);
	XrMatrix4x4f_CreateIdentity(&view[0]);
	XrMatrix4x4f_CreateIdentity(&view[1]);
	XrMatrix4x4f_CreateIdentity(&projection);

	_VR_GetVirtualScreenViewMatrix(&view[0], &left->position, &left->orientation);
	_VR_GetVirtualScreenViewMatrix(&view[1], &right->position, &right->orientation);
	XrMatrix4x4f_CreateProjectionFov(&projection, GRAPHICS_OPENGL, fov, 0.01f, 100.0f);

	// Floor
	{
		_VR_GetVirtualScreenFloorModelMatrix(&model);

		qglUseProgram(floorShaderProgram);
		qglBindVertexArray(quadVAO);

		const int modelLoc = qglGetUniformLocation(vsShaderProgram, "model");
		const int viewLoc = qglGetUniformLocation(vsShaderProgram, "view");
		const int projLoc = qglGetUniformLocation(vsShaderProgram, "proj");
		const int cameraLoc = qglGetUniformLocation(vsShaderProgram, "camera");

		qglUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model.m);
		qglUniformMatrix4fv(viewLoc + 0, 1, GL_FALSE, (float*)view[0].m);
		qglUniformMatrix4fv(viewLoc + 1, 1, GL_FALSE, (float*)view[1].m);
		qglUniformMatrix4fv(projLoc, 1, GL_FALSE, (float*)projection.m);
		qglUniform3f(cameraLoc, left->position.x, left->position.y, left->position.z);

		glDrawElements(GL_TRIANGLES, quadIndexCount, GL_UNSIGNED_INT, 0);
	}

	// Virtual Screen
	{
		unsigned int VAO = (vr_virtualScreenShape->integer == CURVED) ? cylinderVAO : quadVAO;
		unsigned int indexCount = (vr_virtualScreenShape->integer == CURVED) ? cylinderIndexCount : quadIndexCount;

		_VR_GetVirtualScreenModelMatrix(&model, left);

		qglUseProgram(vsShaderProgram);
		qglBindVertexArray(VAO);

		const int modelLoc = qglGetUniformLocation(vsShaderProgram, "model");
		const int viewLoc = qglGetUniformLocation(vsShaderProgram, "view");
		const int projLoc = qglGetUniformLocation(vsShaderProgram, "proj");

		qglUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model.m);
		qglUniformMatrix4fv(viewLoc + 0, 1, GL_FALSE, (float*)view[0].m);
		qglUniformMatrix4fv(viewLoc + 1, 1, GL_FALSE, (float*)view[1].m);
		qglUniformMatrix4fv(projLoc, 1, GL_FALSE, (float*)projection.m);

		glBindTexture(GL_TEXTURE_2D, virtualScreenImage);
		glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
	}

	if (!blendingEnabled)
	{
		qglDisable(GL_BLEND);
	}
	if (depthMask == GL_TRUE)
	{
		qglDepthMask(GL_TRUE);
	}

	glBindTexture(GL_TEXTURE_2D, previousTexture);
	qglBindVertexArray(previousVAO);
	qglUseProgram(previousProgram);
}

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);

float VR_VirtualScreen_GetCurrentYaw(void)
{
	vec3_t rotation = {0, 0, 0};
	vec3_t yawPitchRoll = { 0, 0, 0 };
	QuatToYawPitchRoll(lastKnownVirtualScreenOrientation, rotation, yawPitchRoll);
	return yawPitchRoll[YAW];
}
