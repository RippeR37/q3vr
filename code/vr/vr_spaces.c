#include "vr_spaces.h"

#include "vr_macros.h"
#include "vr_types.h"

XrBool32 VR_IsStageSpaceSupported( XrSession session )
{
	uint32_t numOutputSpaces = 0;
	XR_CHECK(
		xrEnumerateReferenceSpaces(session, 0, &numOutputSpaces, NULL),
		"failed to count reference spaces");

	XrReferenceSpaceType* referenceSpaces =
		(XrReferenceSpaceType*)malloc(numOutputSpaces * sizeof(XrReferenceSpaceType));
	XR_CHECK(
		xrEnumerateReferenceSpaces(session, numOutputSpaces, &numOutputSpaces, referenceSpaces),
		"failed to enumerate reference spaces");

	XrBool32 result = XR_FALSE;

	for (uint32_t i = 0; i < numOutputSpaces; i++)
	{
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE)
		{
			result = XR_TRUE;
			break;
		}
	}

	free(referenceSpaces);
	return result;
}
