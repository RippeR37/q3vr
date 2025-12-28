/*
===========================================================================
Copyright (C) 2025 Tim Angus (tim@ngus.net)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_HTTP

#include "client.h"

#include <windows.h>
#include <wininet.h>

static HINTERNET hInternet = NULL;
static HINTERNET hUrl = NULL;

static HINTERNET hInMemoryUrl = NULL;
static unsigned char* hInMemoryBuffer = NULL;
static size_t hInMemoryBufferSize = 0;
static size_t hInMemoryBufferCursor = 0;
static CL_HTTP_InMemoryDownloadCallback hInMemoryCallback = NULL;

static Q_PRINTF_FUNC(2, 3) void DropIf(qboolean condition, const char *fmt, ...)
{
    char buffer[1024];

    if (!condition)
        return;

    va_list argptr;
    va_start(argptr, fmt);
    Q_vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);

    Com_Error(ERR_DROP, "Download Error: %s URL: %s", buffer, clc.downloadURL);
}

/*
=================
CL_HTTP_Init
=================
*/
qboolean CL_HTTP_Init()
{
    OSVERSIONINFO osvi = {sizeof(OSVERSIONINFO)};
    const char *windowsVersion = GetVersionEx(&osvi) ? va("Windows %lu.%lu (build %lu)",
                                                          osvi.dwMajorVersion,
                                                          osvi.dwMinorVersion,
                                                          osvi.dwBuildNumber)
                                                     : "Windows";

    hInternet = InternetOpenA(
        va("%s %s", Q3_VERSION, windowsVersion),
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

    return hInternet != NULL;
}

/*
=================
CL_HTTP_Available
=================
*/
qboolean CL_HTTP_Available()
{
    return hInternet != NULL;
}

/*
=================
CL_HTTP_Shutdown
=================
*/
void CL_HTTP_Shutdown(void)
{
    if (hInternet)
    {
        InternetCloseHandle(hInternet);
        hInternet = NULL;
    }
}

/*
=================
CL_HTTP_BeginDownload
=================
*/
void CL_HTTP_BeginDownload(const char *remoteURL)
{
    DWORD httpCode = 0;
    DWORD contentLength = 0;
    DWORD len = sizeof(httpCode);
    DWORD zero = 0;
    BOOL success;

    hUrl = InternetOpenUrlA(hInternet, remoteURL,
                            va("Referer: ioQ3://%s\r\n", NET_AdrToString(clc.serverAddress)), (DWORD)-1,
                            INTERNET_FLAG_HYPERLINK |
                                INTERNET_FLAG_NO_CACHE_WRITE |
                                INTERNET_FLAG_NO_COOKIES |
                                INTERNET_FLAG_NO_UI |
                                INTERNET_FLAG_RESYNCHRONIZE |
                                INTERNET_FLAG_RELOAD,
                            0);

    DropIf(hUrl == NULL, "InternetOpenUrlA failed %lu", GetLastError());

    success = HttpQueryInfo(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &httpCode, &len, &zero);
    DropIf(!success, "Get HTTP_QUERY_STATUS_CODE failed %lu", GetLastError());

    DropIf(httpCode >= 400, "HTTP code %lu", httpCode);
    DropIf(httpCode != 200, "Unhandled HTTP code %lu", httpCode);

    success = HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &len, &zero);
    DropIf(!success, "Get HTTP_QUERY_CONTENT_LENGTH failed %lu", GetLastError());

    clc.downloadSize = (int)contentLength;
    Cvar_SetValue("cl_downloadSize", clc.downloadSize);
}


qboolean CL_HTTP_PerformInMemoryDownload(void);

/*
=================
CL_HTTP_PerformDownload
=================
*/
qboolean CL_HTTP_PerformDownload(void)
{
    static BYTE readBuffer[256 * 1024];
    DWORD bytesRead = 0;
    BOOL success;

    DropIf(hUrl == NULL, "hUrl is NULL");

    success = InternetReadFile(hUrl, readBuffer, sizeof(readBuffer), &bytesRead);
    DropIf(!success, "InternetReadFile failed %lu", GetLastError());

    if (bytesRead > 0)
    {
        clc.downloadCount += bytesRead;
        Cvar_SetValue("cl_downloadCount", clc.downloadCount);

        DWORD bytesWritten = (DWORD)FS_Write(readBuffer, bytesRead, clc.download);
        DropIf(bytesWritten != bytesRead, "bytesWritten != bytesRead");

        return qfalse;
    }

    InternetCloseHandle(hUrl);
    return qtrue;
}

void CL_HTTP_BeginInMemoryDownload( const char *remoteURL, CL_HTTP_InMemoryDownloadCallback callback, unsigned char* buffer, size_t bufferSize)
{
    DWORD httpCode = 0;
    DWORD contentLength = 0;
    DWORD len = sizeof(httpCode);
    DWORD zero = 0;
    BOOL success;

    hInMemoryUrl = InternetOpenUrlA(
        hInternet, 
        remoteURL,
        NULL, 
        (DWORD)-1,
        INTERNET_FLAG_HYPERLINK |
            INTERNET_FLAG_NO_CACHE_WRITE |
            INTERNET_FLAG_NO_COOKIES |
            INTERNET_FLAG_NO_UI |
            INTERNET_FLAG_RESYNCHRONIZE |
            INTERNET_FLAG_RELOAD,
        0);

    if (hInMemoryUrl == NULL)
    {
    	fprintf(stderr, "InternetOpenUrlA failed %lu", GetLastError());
    	return;
    }

    success = HttpQueryInfo(hInMemoryUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &httpCode, &len, &zero);
    if (!success)
    {
    	fprintf(stderr, "Get HTTP_QUERY_STATUS_CODE failed %lu", GetLastError());
    	return;
    }
    if (httpCode >= 400)
    {
    	fprintf(stderr, "HTTP code %lu", httpCode);
    	return;
    }
    if (httpCode != 200)
    {
    	fprintf(stderr, "Unhandled HTTP code %lu", httpCode);
    	return;
    }

    success = HttpQueryInfo(hInMemoryUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &len, &zero);
    if (!success)
    {
    	fprintf(stderr, "Get HTTP_QUERY_CONTENT_LENGTH failed %lu", GetLastError());
    	return;
    }

    hInMemoryBuffer = buffer;
    hInMemoryBufferSize = bufferSize;
    hInMemoryBufferCursor = 0;
    hInMemoryCallback = callback;
}

qboolean CL_HTTP_PerformInMemoryDownload(void)
{
    static BYTE readBuffer[256 * 1024];
    DWORD bytesRead = 0;
    BOOL success;

    if (hInMemoryUrl == NULL)
    {
        return qtrue;
    }
   
    success = InternetReadFile(hInMemoryUrl, readBuffer, sizeof(readBuffer), &bytesRead);
    if (!success)
    {
    	  hInMemoryCallback(-1);
        goto clear;
    }

    if (bytesRead > 0)
    {
        memcpy_s(hInMemoryBuffer + hInMemoryBufferCursor, hInMemoryBufferSize, readBuffer, bytesRead);
        hInMemoryBufferCursor += bytesRead;
        hInMemoryBufferSize -= bytesRead;
        return qfalse;
    }

    InternetCloseHandle(hInMemoryUrl);
    hInMemoryCallback(hInMemoryBufferCursor);

clear:
    hInMemoryUrl = NULL;
    hInMemoryCallback = NULL;
    hInMemoryBuffer = NULL;
    hInMemoryBufferSize = 0;
    hInMemoryBufferCursor = 0;

    return qtrue;
}

#endif /* USE_HTTP */
