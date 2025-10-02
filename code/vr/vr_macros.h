#ifndef __VR_MACROS
#define __VR_MACROS

#include <stdio.h>
#include <stdlib.h>

#include "vr_debug.h"

#ifdef USE_DEBUG_STACKTRACE
void print_stacktrace(void);
#define PRINT_STACKTRACE print_stacktrace()
#else
#define PRINT_STACKTRACE
#endif

// Plain
#define CHECK(expr, msg)  \
	{                                                                                                                                                   \
		if (!(expr)) {                                                                                                                                    \
			fprintf(stderr, "[VR] Check failed:\n  Expression: %s\n  Message: %s\n", #expr, msg);                                                           \
			PRINT_STACKTRACE;                                                                                                                               \
			exit(1);                                                                                                                                        \
		}                                                                                                                                                 \
	}


// OpenXR
#define XR_CHECK(expr, msg)                                                                                                                           \
	{                                                                                                                                                   \
		XrResult result = (expr);                                                                                                                         \
		if (!XR_SUCCEEDED(result)) {                                                                                                                      \
			const char* result_str = GetXRErrorString(result);                                                                                              \
			fprintf(stderr, "[OpenXR] Check failed:\n  Expression: %s\n  Result: %d (%s)\n  Message: %s\n", #expr, (int)result, result_str, msg);           \
			PRINT_STACKTRACE;                                                                                                                               \
			exit(1);                                                                                                                                        \
		}                                                                                                                                                 \
	}

// GL
// TODO

#endif
