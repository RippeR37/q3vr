#include "vr_updates.h"

#include <stdio.h>
#include <ctype.h>

#include "../qcommon/json.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/cl_http.h"

const char* const GH_LATEST_RELEASE_API_URL = "https://api.github.com/repos/RippeR37/q3vr/releases/latest";

qboolean g_checkedAlready = qfalse;
qboolean g_checkInProgress = qfalse;
unsigned char g_responseBuffer[2 * 1024 * 1024];

void VR_OnCheckUpdatesResponse(int foo);
int ParseTagVersion(const char *str, int *major, int *minor, int *patch);

void VR_CheckUpdates(void)
{
#ifdef USE_HTTP
	if (g_checkInProgress || g_checkedAlready)
	{
		return;
	}

	const int bufferSize = sizeof(g_responseBuffer)/sizeof(g_responseBuffer[0]);
	memset(g_responseBuffer, 0, bufferSize);

	CL_HTTP_BeginInMemoryDownload(GH_LATEST_RELEASE_API_URL, VR_OnCheckUpdatesResponse, g_responseBuffer, bufferSize);

	g_checkInProgress = qtrue;
#endif
}

void VR_OnCheckUpdatesResponse(int bytesRead)
{
	if (bytesRead < 0)
	{
		fprintf(stderr, "Failed to check if there are any updates");
		g_checkInProgress = qfalse;
		g_checkedAlready = qtrue;
		return;
	}

  const char* tagValue = JSON_ObjectGetNamedValue((const char*)g_responseBuffer, (const char*)g_responseBuffer + bytesRead, "tag_name");
	if (!tagValue)
	{
		g_checkInProgress = qfalse;
		g_checkedAlready = qtrue;
		return;
	}

	char tagName[64] = { 0 };
	const unsigned int tagNameLength = JSON_ValueGetString(tagValue, (const char*)g_responseBuffer + bytesRead, tagName, 64);
	if (tagNameLength > 0)
	{
		int major = 0, minor = 0, patch = 0;
		ParseTagVersion(tagName, &major, &minor, &patch);

		fprintf(stderr, "Update version: %d.%d.%d\n", major, minor, patch);

		Cvar_SetValue("q3vr_update_version_major", major);
		Cvar_SetValue("q3vr_update_version_minor", minor);
		Cvar_SetValue("q3vr_update_version_patch", patch);

		const int needsUpdate = (major > Q3VR_VERSION_MAJOR) || (major == Q3VR_VERSION_MAJOR && (minor > Q3VR_VERSION_MINOR || (minor == Q3VR_VERSION_MINOR && patch > Q3VR_VERSION_PATCH)));
		Cvar_SetValue("q3vr_update_version_check", needsUpdate ? 1 : 0);
	}

	g_checkInProgress = qfalse;
	g_checkedAlready = qtrue;
}

int ParseTagVersion(const char *str, int *major, int *minor, int *patch)
{
    if (!str || !major || !minor || !patch)
		{
        return -1;
		}

    *major = *minor = *patch = 0;

    if (*str == 'v' || *str == 'V')
		{
        str++;
		}

    if (!isdigit((unsigned char)*str))
		{
        return -1;
		}

    while (isdigit((unsigned char)*str))
		{
        *major = (*major * 10) + (*str - '0');
        str++;
    }

    if (*str == '.')
		{
        str++;
        while (isdigit((unsigned char)*str))
				{
            *minor = (*minor * 10) + (*str - '0');
            str++;
        }
    }

    if (*str == '.')
		{
        str++;
        while (isdigit((unsigned char)*str))
				{
            *patch = (*patch * 10) + (*str - '0');
            str++;
        }
    }

    return 0;
}
