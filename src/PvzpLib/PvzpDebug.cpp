/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include <time.h>
#include <cinttypes>
#include <stdarg.h>
#include <stdexcept>
#include <fstream>
#include <exception>
#if defined(__GNUC__)
#include <unwind.h>
#endif

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "PvzpDebug.h"
#include "PvzpCommon.h"
#include "misc/Debug.h"
#include "../SexyAppFramework/Common.h"
#include "../SexyAppFramework/SexyAppBase.h"

using namespace Sexy;

// vx: default path; replaced with appdata userdata/log.txt at PvzpAssertInitForApp
static char gLogFileName[512] = "vatrix_crash.txt";
static char gDebugDataFolder[512];

void PvzpErrorMessageBox(const char* theMessage, const char* theTitle)
{
#ifdef __SWITCH__
	ErrorApplicationConfig c;
	errorApplicationCreate(&c, theTitle, theMessage);
	errorApplicationShow(&c);
#else
	throw std::runtime_error("Error Box\n--" + std::string(theTitle) + "--\n" + theMessage);
#endif
}

void PvzpTraceMemory()
{
}

void* PvzpMalloc(int theSize)
{
	PVZP_ASSERT(theSize > 0);
	return malloc(theSize);
}

void PvzpFree(void* theBlock)
{
	if (theBlock != nullptr)
	{
		free(theBlock);
	}
}

void PvzpAssertFailed(const char* theCondition, const char* theFile, int theLine, const char* theMsg, ...)
{
	va_list argList;
	va_start(argList, theMsg);
	std::string aFormattedMsg = Sexy::VFormat(theMsg, argList);
	va_end(argList);

	std::string aBuffer;
	if (*theCondition != '\0')
		aBuffer = Sexy::StrFormat("\n%s(%d)\nassertion failed: '%s'\n%s", theFile, theLine, theCondition, aFormattedMsg.c_str());
	else
		aBuffer = Sexy::StrFormat("\n%s(%d)\nassertion failed: %s", theFile, theLine, aFormattedMsg.c_str());

	PvzpTrace("%s", aBuffer.c_str());
	PvzpErrorMessageBox(aBuffer.c_str(), "Assertion failed");
	exit(0);
}

void PvzpLogLn(const char* theFormat, ...)
{
	va_list argList;
	va_start(argList, theFormat);
	std::string aBuffer = Sexy::VFormat(theFormat, argList);
	va_end(argList);

	if (!aBuffer.empty())
		PvzpLogStringLn(aBuffer.c_str());
}

void PvzpLogStringLn(const char* theMsg)
{
	// vx: always write log.txt (release builds too)
	std::ofstream f(Sexy::PathFromU8(gLogFileName), std::ios::app | std::ios::binary);
	if (!f)
	{
		Sexy::LogError("Failed to open log file '%s'", gLogFileName);
		return;
	}

	f << theMsg << '\n';
	if (!f)
	{
		Sexy::LogError("Failed to write to log file");
	}
}

void PvzpTrace(const char* theFormat, ...)
{
	va_list argList;
	va_start(argList, theFormat);
	std::string aBuffer = Sexy::VFormat(theFormat, argList);
	va_end(argList);

	if (!aBuffer.empty())
		Sexy::PrintF("%s", aBuffer.c_str());
}

void PvzpHesitationTrace(...)
{
}

void PvzpTraceAndLogLn(const char* theFormat, ...)
{
	va_list argList;
	va_start(argList, theFormat);
	std::string aBuffer = Sexy::VFormat(theFormat, argList);
	va_end(argList);

	if (aBuffer.empty())
		return;

	Sexy::PrintF("%s", aBuffer.c_str());
	PvzpLogStringLn(aBuffer.c_str());
}

void PvzpTraceWithoutSpamming(const char* theFormat, ...)
{
	static uint64_t gLastTraceTime = 0LL;
	uint64_t aTime = time(nullptr);
	if (aTime < gLastTraceTime)
		return;

	gLastTraceTime = aTime;

	va_list argList;
	va_start(argList, theFormat);
	std::string aBuffer = Sexy::VFormat(theFormat, argList);
	va_end(argList);

	if (!aBuffer.empty())
		Sexy::PrintF("%s", aBuffer.c_str());
}

void PvzpAssertInitForApp()
{
	MkDir(GetAppDataPath("userdata"));
	std::string aRelativeUserPath = GetAppDataPath("userdata/");
	strcpy(gDebugDataFolder, aRelativeUserPath.c_str());
	strcpy(gLogFileName, gDebugDataFolder);
	strcpy(gLogFileName + strlen(gLogFileName), "log.txt");
	PVZP_ASSERT(strlen(gLogFileName) < 512);

	PvzpLogLn("Started %" PRIu64, static_cast<uint64_t>(time(nullptr)));
}

// vx: stack-trace helpers for crash logs
struct VxTraceState
{
	void**		mFrames;
	int			mCount;
	uintptr_t	mBase;
};

static _Unwind_Reason_Code VxTraceCallback(struct _Unwind_Context* aContext, void* anArg)
{
	VxTraceState* aState = static_cast<VxTraceState*>(anArg);
	if (aState->mCount >= 32)
		return _URC_END_OF_STACK;
	aState->mFrames[aState->mCount++] = (void*)_Unwind_GetIP(aContext);
	return _URC_NO_REASON;
}

#ifdef _WIN32
static bool VxAddressInExe(uintptr_t anAddress)
{
	HMODULE aModule = nullptr;
	if (GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(anAddress), &aModule))
		return aModule == GetModuleHandleA(nullptr);
	return false;
}
#endif

static std::string VxFormatStackTrace()
{
	std::string aResult = "STACK:\n";
#if defined(__GNUC__)
	void* aFrames[32];
	VxTraceState aState = { aFrames, 0, 0 };
	_Unwind_Backtrace(&VxTraceCallback, &aState);
#ifdef _WIN32
	aState.mBase = (uintptr_t)GetModuleHandleA(nullptr);
#endif
	for (int i = 0; i < aState.mCount; i++)
	{
		uintptr_t aAddress = (uintptr_t)aFrames[i];
		if (aState.mBase && aAddress >= aState.mBase && VxAddressInExe(aAddress))
			aResult += Sexy::StrFormat("  #%02d 0x%llX (rva, exe)\n",
				i, (unsigned long long)(aAddress - aState.mBase));
		else
			aResult += Sexy::StrFormat("  #%02d %p (other module)\n", i, (void*)aAddress);
	}
#endif
	return aResult;
}

// vx: crash handler: log unhandled exceptions and SEH crashes, then show a message box
static void VxTerminateHandler()
{
	std::string aMessage = "CRASH: unhandled C++ exception (unknown type)";
	try
	{
		std::rethrow_exception(std::current_exception());
	}
	catch (const std::exception& aException)
	{
		aMessage = std::string("CRASH: unhandled C++ exception: ") + aException.what();
	}
	catch (...)
	{
	}

	PvzpLogStringLn(aMessage.c_str());
	PvzpLogStringLn(VxFormatStackTrace().c_str());

#ifdef _WIN32
	std::string aBox = aMessage + "\n\nError details were written to:\n" + gLogFileName;
	MessageBoxA(nullptr, aBox.c_str(), "Vatrix crashed", MB_OK | MB_ICONERROR);
#endif
	std::abort();
}

#ifdef _WIN32
static LONG WINAPI VxSehExceptionFilter(EXCEPTION_POINTERS* aExceptionInfo)
{
	EXCEPTION_RECORD* aRecord = aExceptionInfo->ExceptionRecord;
	char aModuleName[MAX_PATH] = "?";
	HMODULE aModule = nullptr;
	if (GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			static_cast<LPCSTR>(aRecord->ExceptionAddress), &aModule))
	{
		GetModuleFileNameA(aModule, aModuleName, MAX_PATH);
	}

	PvzpLogStringLn(Sexy::StrFormat(
		"CRASH: SEH exception 0x%08X at %p (rva 0x%llX, module: %s)",
		static_cast<unsigned>(aRecord->ExceptionCode), aRecord->ExceptionAddress,
		(unsigned long long)((uintptr_t)aRecord->ExceptionAddress - (uintptr_t)GetModuleHandleA(nullptr)), aModuleName).c_str());
	PvzpLogStringLn(VxFormatStackTrace().c_str());

	std::string aBox = Sexy::StrFormat(
		"Vatrix crashed (0x%08X at %p, module: %s).\nError details were written to:\n%s",
		static_cast<unsigned>(aRecord->ExceptionCode), aRecord->ExceptionAddress, aModuleName, gLogFileName);
	MessageBoxA(nullptr, aBox.c_str(), "Vatrix crashed", MB_OK | MB_ICONERROR);
	TerminateProcess(GetCurrentProcess(), 1);
	return EXCEPTION_EXECUTE_HANDLER; // unreachable
}
#endif

void PvzpInstallCrashHandler()
{
	std::set_terminate(&VxTerminateHandler);
#ifdef _WIN32
	SetUnhandledExceptionFilter(&VxSehExceptionFilter);
#endif
}
