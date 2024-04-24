#pragma once
#ifdef WIN32

#if !defined(STRICT)
#define STRICT
#endif

#if !defined(VC_EXTRALEAN)
#define VC_EXTRALEAN
#endif

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#if !defined(NOCOMM)
#define NOCOMM
#endif

#pragma warning(push)
#pragma warning(disable: 4255) // no function prototype: '()' to '(void)'
#pragma warning(disable: 4459) // declaration of '...' hides global declaration

#include <Windows.h> // must be first

#include <commdlg.h>
#include <dwmapi.h>
#include <initguid.h>
#include <KnownFolders.h>
#include <ShellScalingApi.h>
#include <shlobj_core.h>
#include <VersionHelpers.h>
#include <windowsx.h>
#pragma warning(pop)

#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(_malloca) // Microsoft runtime debug heap
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h> // _malloca()
#endif

#define export __declspec(dllexport)

#define b2e(call) (call ? 0 : GetLastError()) // BOOL -> errno_t

#endif // WIN32
