// vx: Python scripting bridge - player scripts in ./scripts run on a worker thread
#ifdef VX_SCRIPT

#include <Python.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "VxScript.h"
#include "Board.h"
#include "Challenge.h"

namespace
{
	struct BreakCmd
	{
		int mRow;
		int mCol;
	};

	std::mutex gQueueMutex;
	std::vector<BreakCmd> gQueue;
	std::thread gScriptThread;
	std::filesystem::path gScriptsDir;
	bool gPythonReady = false;

	// vx: one-shot driver; per-script namespaces, tracebacks appended to vx_script.log
	const char* gScriptDriver = R"PY(
import sys, os, traceback
d = sys.vx_scripts_dir
sys.path.insert(0, d)
log_path = os.path.join(d, "vx_script.log")
try:
    log = open(log_path, "a", encoding="utf-8")
except OSError:
    log = None

def run_script(path):
    ns = {}
    try:
        with open(path, "rb") as f:
            src = f.read()
        exec(compile(src, path, "exec"), ns)
    except BaseException:
        if log is not None:
            log.write("=== %s ===\n" % path)
            traceback.print_exc(file=log)
            log.flush()

# vx: bundled non-script files in scripts/ are not run as player scripts
SKIP = {"vb.py", "pvzp-v4-converter.py"}
try:
    files = sorted(f for f in os.listdir(d) if f.endswith(".py") and f not in SKIP)
except OSError:
    files = []
for f in files:
    run_script(os.path.join(d, f))
if log is not None:
    log.close()
)PY";

	void ScriptLog(const std::string& theMessage)
	{
		std::ofstream aLog(gScriptsDir / "vx_script.log", std::ios::app);
		if (aLog)
			aLog << theMessage << std::endl;
	}

	void ScriptThreadMain()
	{
		if (!gPythonReady)
			return;
		PyGILState_STATE aGILState = PyGILState_Ensure();
		std::wstring aDirWide = gScriptsDir.wstring();
		PyObject* aDirObj = PyUnicode_FromWideChar(aDirWide.c_str(), static_cast<Py_ssize_t>(aDirWide.size()));
		if (aDirObj)
		{
			PySys_SetObject("vx_scripts_dir", aDirObj);
			Py_DECREF(aDirObj);
			PyRun_SimpleString(gScriptDriver);
		}
		PyGILState_Release(aGILState);
	}

	PyObject* VbBrk(PyObject*, PyObject* theArgs)
	{
		int aRow = 0;
		int aCol = 0;
		if (!PyArg_ParseTuple(theArgs, "ii", &aRow, &aCol))
			return nullptr;
		if (aRow < 0 || aCol < 0)
		{
			PyErr_SetString(PyExc_ValueError, "row and col must be >= 0");
			return nullptr;
		}
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			gQueue.push_back({aRow, aCol});
		}
		Py_RETURN_NONE;
	}

	PyMethodDef gVbMethods[] = {
		{"brk", VbBrk, METH_VARARGS, "Queue a vase break at (row, col)."},
		{nullptr, nullptr, 0, nullptr},
	};

	PyModuleDef gVbModule = {
		PyModuleDef_HEAD_INIT,
		"_vb",
		"Vatrix scripting bridge (vase breaking).",
		-1,
		gVbMethods,
	};
}

extern "C" PyMODINIT_FUNC PyInit__vb(void)
{
	return PyModule_Create(&gVbModule);
}

namespace VX
{
	std::wstring VxExeDir()
	{
		wchar_t aBuffer[MAX_PATH];
		DWORD aLength = GetModuleFileNameW(nullptr, aBuffer, MAX_PATH);
		if (aLength == 0 || aLength >= MAX_PATH)
			return std::wstring();
		return std::filesystem::path(aBuffer).parent_path().wstring();
	}

	void Init()
	{
		if (gPythonReady)
			return;
		PyImport_AppendInittab("_vb", PyInit__vb);

		PyConfig aConfig;
		PyConfig_InitIsolatedConfig(&aConfig);
		// vx: module search path = <exe dir>/python312 (pure-python stdlib, synced at build)
		std::wstring aStdlibPath = VxExeDir() + L"\\python312";
		if (std::filesystem::is_directory(aStdlibPath))
		{
			PyWideStringList_Append(&aConfig.module_search_paths, aStdlibPath.c_str());
			aConfig.module_search_paths_set = 1;
		}
		PyStatus aStatus = Py_InitializeFromConfig(&aConfig);
		PyConfig_Clear(&aConfig);
		if (PyStatus_IsError(aStatus))
		{
			ScriptLog("[vb] python init failed");
			return;
		}
		// vx: the game thread keeps no GIL; the script worker acquires it via PyGILState
		PyEval_SaveThread();
		gPythonReady = true;
	}

	void Shutdown()
	{
		if (!gPythonReady)
			return;
		StopScripts();
		PyGILState_Ensure();
		Py_FinalizeEx();
		gPythonReady = false;
	}

	void StartScripts()
	{
		if (!gPythonReady || gScriptThread.joinable())
			return; // python not up, or a worker is still running; Board dtor normally stops it
		gScriptsDir = std::filesystem::current_path() / "scripts";
		gScriptThread = std::thread(ScriptThreadMain);
	}

	void StopScripts()
	{
		if (!gScriptThread.joinable())
			return;
		gScriptThread.join();
		// ponytail: join waits out any script sleep; add an interruptible wait if someone writes 9999s sleeps
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			gQueue.clear();
		}
	}

	void ProcessBoardQueue(Board* theBoard)
	{
		std::vector<BreakCmd> aCommands;
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			aCommands.swap(gQueue);
		}
		for (const BreakCmd& aCommand : aCommands)
		{
			GridItem* aPot = theBoard->GetScaryPotAt(aCommand.mCol, aCommand.mRow);
			if (!aPot)
			{
				ScriptLog("[vb] no pot at (" + std::to_string(aCommand.mRow) + ", " + std::to_string(aCommand.mCol) + ")");
				continue;
			}
			theBoard->mChallenge->ScaryPotterOpenPot(aPot);
		}
	}
}

#else // !VX_SCRIPT
// vx: stubs so call sites compile unchanged when Python scripting is disabled
#include "VxScript.h"

namespace VX
{
	void Init() {}
	void Shutdown() {}
	void StartScripts() {}
	void StopScripts() {}
	void ProcessBoardQueue(Board*) {}
}
#endif // VX_SCRIPT
