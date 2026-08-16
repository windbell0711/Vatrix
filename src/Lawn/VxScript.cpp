// vx: Python scripting bridge - player scripts in ./scripts run on a worker thread
#ifdef VX_SCRIPT

#include <Python.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "VxScript.h"
#include "Board.h"
#include "Challenge.h"
#include "VxEditor.h"
#include "PvzpLib/VxLevelInit.h"

namespace
{
	enum class VxCmdType
	{
		BreakPot,
		BreakPotId,
		Plant,
		Shovel,
		PlantCard,
		ShovelId,
	};

	struct VxCmd
	{
		VxCmdType mType;
		int mRow;
		int mCol;
		int mArg;
	};



	std::mutex gQueueMutex;
	std::vector<VxCmd> gQueue;
	std::thread gScriptThread;
	std::filesystem::path gScriptsDir;
	std::vector<std::filesystem::path> gScriptDirs; // sys.path entries for game python code
	std::filesystem::path gDataDir;                 // Properties dir (adventure_info.csv etc.)
	int gScriptLevel = 0;
	int gScriptGameMode = 0;
	bool gPythonReady = false;
	// vx: worker thread OS id, used to interrupt a hanging script (PyThreadState_SetAsyncExc takes the id)
	std::atomic<unsigned long> gScriptThreadId{0};
	bool gScriptEnded = false;
	std::mutex gScriptEndedMutex;
	std::condition_variable gScriptEndedCv;
	// vx: cooperative sleep for vb.slp: the script thread waits on this cv (GIL released),
	// and StopScripts/interrupt wake it via the stop flag - deterministic, any delay length
	std::mutex gSleepMutex;
	std::condition_variable gSleepCv;
	bool gSleepStop = false;
	// vx: script-visible level game clock (seconds, fed by Board::Update; frozen while paused)
	double gVxGameTime = 0.0;
	// vx: async interrupt fires at most once per stop; repeated stop requests must not pile up KeyboardInterrupts
	bool gStopInterruptFired = false;
	// vx: player-driven world-6 runs (Run = trial, Submit = official)
	std::mutex gRunMutex;
	bool gPendingRun = false;
	int gPendingRunLevel = 0;
	int gPendingRunMode = 0;
	bool gTrialRun = false;
	// vx: seed carried into the next board (Run = fresh, Reset = same as the current board)
	int gTrialSeed = 0;
	// vx: strictly increasing per-session counter (random-ish base) keeps Run seeds unique
	int gTrialSeedCounter = static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFF);
	// vx: true while a locked Submit run is active: the editor autosave is suspended
	bool gRunLocked = false;

	// vx: blocking script query (vb.get_zombies / get_plants / get_cards):
	// script thread asks, main thread answers in ProcessBoardQueue
	enum class VxQueryType
	{
		Zombies,
		Plants,
		Cards,
		Vases,
	};

	std::mutex gQueryMutex;
	std::condition_variable gQueryCv;
	bool gQueryPending = false;
	VxQueryType gQueryType = VxQueryType::Zombies;
	PyObject* gQueryResult = nullptr;
	// vx: resolve script/data dirs; dev builds (running from <repo>/build) use the live source dirs,
	// so editing scripts/ or Properties/ takes effect without rebuilding
	void VxResolveDirs()
	{
		std::filesystem::path aCwd = std::filesystem::current_path();
		std::filesystem::path aLiveRoot = aCwd.parent_path();
		bool aDevMode = std::filesystem::is_directory(aCwd / "scripts") && std::filesystem::is_directory(aLiveRoot / "scripts");
		gScriptDirs.clear();
		if (aDevMode)
		{
			gScriptDirs.push_back(aLiveRoot / "scripts");
			gDataDir = aLiveRoot / "Properties";
		}
		else
		{
			gScriptDirs.push_back(aCwd / "scripts");
			gDataDir = aCwd / "Properties";
		}
		gScriptsDir = gScriptDirs.front();
	}

	void VxAddScriptDirsToPath()
	{
		for (const std::filesystem::path& aDir : gScriptDirs)
		{
			std::wstring aDirWide = aDir.wstring();
			PyObject* aDirObj = PyUnicode_FromWideChar(aDirWide.c_str(), static_cast<Py_ssize_t>(aDirWide.size()));
			if (!aDirObj)
				continue;
			PyObject* aPath = PySys_GetObject("path"); // borrowed
			if (aPath && PyList_Check(aPath) && PySequence_Contains(aPath, aDirObj) <= 0)
				PyList_Insert(aPath, 0, aDirObj);
			Py_DECREF(aDirObj);
		}
	}

	void VxSetupPythonPath()
	{
		VxAddScriptDirsToPath();
		// vx: userlibs/ next to the scripts dir (repo/userlibs in dev, <exe>/userlibs in release);
		// keep it after the game script dirs so game code wins, before the stdlib
		std::filesystem::path aUserlibs = gScriptsDir.parent_path() / "userlibs";
		std::error_code anEc;
		if (!std::filesystem::is_directory(aUserlibs, anEc))
			std::filesystem::create_directories(aUserlibs, anEc);
		std::wstring aUserlibsWide = aUserlibs.wstring();
		PyObject* aUserlibsObj = PyUnicode_FromWideChar(aUserlibsWide.c_str(), static_cast<Py_ssize_t>(aUserlibsWide.size()));
		if (aUserlibsObj)
		{
			PyObject* aPath = PySys_GetObject("path"); // borrowed
			if (aPath && PyList_Check(aPath) && PySequence_Contains(aPath, aUserlibsObj) <= 0)
				PyList_Insert(aPath, static_cast<Py_ssize_t>(gScriptDirs.size()), aUserlibsObj);
			Py_DECREF(aUserlibsObj);
		}
		std::wstring aScriptsWide = gScriptsDir.wstring();
		std::wstring aDataWide = gDataDir.wstring();
		PySys_SetObject("vx_scripts_dir", PyUnicode_FromWideChar(aScriptsWide.c_str(), static_cast<Py_ssize_t>(aScriptsWide.size())));
		PySys_SetObject("vx_data_dir", PyUnicode_FromWideChar(aDataWide.c_str(), static_cast<Py_ssize_t>(aDataWide.size())));
	}

	void ScriptLog(const std::string& theMessage); // vx: forward decl, defined below




	// vx: one-shot driver; per-script namespaces, prints streamed to vx_script_output.txt,
	// CE/RE written to vx_script_error.txt, tracebacks appended to vx_script.log
	const char* gScriptDriver = R"PY(
import sys, os, traceback, contextlib
d = sys.vx_scripts_dir
sys.path.insert(0, d)
log_path = os.path.join(d, "vx_script.log")
err_path = os.path.join(d, "vx_script_error.txt")
out_path = os.path.join(d, "vx_script_output.txt")
try:
    log = open(log_path, "a", encoding="utf-8")
except OSError:
    log = None
try:
    os.remove(err_path)
except OSError:
    pass
try:
    os.remove(out_path)
except OSError:
    pass

def write_error(kind):
    try:
        with open(err_path, "w", encoding="utf-8") as ef:
            ef.write(kind + "\n")
            traceback.print_exc(file=ef)
    except BaseException:
        pass
    try:
        print("=== %s ===" % path)
        traceback.print_exc()
    except BaseException:
        pass
    if log is not None:
        try:
            log.write("=== %s ===\n" % path)
            traceback.print_exc(file=log)
            log.flush()
        except BaseException:
            pass

def run_script(path):
    ns = {"__file__": path, "__name__": "__main__"}
    try:
        with open(path, "rb") as f:
            src = f.read()
        code = compile(src, path, "exec")
    except BaseException:
        write_error("CE")  # compile/parse error
        return
    try:
        out = open(out_path, "a", encoding="utf-8", buffering=1)
    except OSError:
        out = None
    try:
        with contextlib.redirect_stdout(out) if out is not None else contextlib.nullcontext():
            exec(code, ns)
    except KeyboardInterrupt:
        pass  # vx: player stop; not an error
    except BaseException:
        write_error("RE")  # runtime error
    finally:
        try:
            if out is not None:
                out.close()
        except BaseException:
            pass

# vx: run only this level's script, e.g. script_adventure_6_1.py
mode = getattr(sys, "vx_game_mode", "")
area = getattr(sys, "vx_area", 0)
sub = getattr(sys, "vx_sub", 0)
path = os.path.join(d, "script_%s_%d_%d.py" % (mode, area, sub))
try:
    if os.path.exists(path):
        run_script(path)
except BaseException:
    pass  # vx: a stray async interrupt must never escape the driver
try:
    if log is not None:
        log.close()
except BaseException:
    pass
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
		// vx: hard C++ delay: the user script only starts 0.5s after the level is created
		{
			std::unique_lock<std::mutex> aLock(gSleepMutex);
			if (gSleepCv.wait_for(aLock, std::chrono::milliseconds(500), [] { return gSleepStop; }))
			{
				// vx: stop requested before the script started; mark ended so StopScripts joins cleanly
				std::lock_guard<std::mutex> aEndedLock(gScriptEndedMutex);
				gScriptEnded = true;
				gScriptEndedCv.notify_all();
				return;
			}
		}
		PyGILState_STATE aGILState = PyGILState_Ensure();
		gScriptThreadId.store(PyThread_get_thread_ident());
		VxSetupPythonPath();
		PySys_SetObject("vx_game_mode", PyUnicode_FromString(gScriptGameMode == static_cast<int>(GameMode::GAMEMODE_ADVENTURE) ? "adventure" : ""));
		if (gScriptGameMode == static_cast<int>(GameMode::GAMEMODE_ADVENTURE))
		{
			int aArea = (gScriptLevel - 1) / 10 + 1;
			int aSub = (gScriptLevel - 1) % 10 + 1;
			PySys_SetObject("vx_area", PyLong_FromLong(aArea));
			PySys_SetObject("vx_sub", PyLong_FromLong(aSub));
		}
		else
		{
			PySys_SetObject("vx_area", PyLong_FromLong(0));
			PySys_SetObject("vx_sub", PyLong_FromLong(0));
		}
		PyRun_SimpleString(gScriptDriver);
		{
			std::lock_guard<std::mutex> aLock(gScriptEndedMutex);
			gScriptEnded = true;
		}
		gScriptEndedCv.notify_all();
		gScriptThreadId.store(0);
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
			gQueue.push_back({VxCmdType::BreakPot, aRow, aCol, 0});
		}
		Py_RETURN_NONE;
	}

	PyObject* VbBrkVase(PyObject*, PyObject* theArgs)
	{
		int aVaseID = 0;
		if (!PyArg_ParseTuple(theArgs, "i", &aVaseID))
			return nullptr;
		if (aVaseID <= 0)
		{
			PyErr_SetString(PyExc_ValueError, "vase id must be positive");
			return nullptr;
		}
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			gQueue.push_back({VxCmdType::BreakPotId, 0, 0, aVaseID});
		}
		Py_RETURN_NONE;
	}

	// vx: cooperative sleep - the script thread waits on a cv with the GIL released; a stop
	// request wakes it immediately instead of relying on PyThreadState_SetAsyncExc (which only
	// interrupts the eval loop, not arbitrary waits)
	PyObject* VbSlp(PyObject*, PyObject* theArgs)
	{
		double aDelay = 0.0;
		if (!PyArg_ParseTuple(theArgs, "d", &aDelay))
			return nullptr;
		if (aDelay < 0)
		{
			PyErr_SetString(PyExc_ValueError, "delay must be >= 0");
			return nullptr;
		}
		{
			std::unique_lock<std::mutex> aLock(gSleepMutex);
			if (aDelay > 0 && !gSleepStop)
			{
				// vx: wait on the level game clock: pause freezes the game time, so slp blocks until unpaused.
				// wait_for with a predicate is a single timeout window; loop so a long delay survives 500ms timeouts
				double aTargetTime = gVxGameTime + aDelay;
				while (!gSleepStop && gVxGameTime < aTargetTime)
				{
					Py_BEGIN_ALLOW_THREADS
					gSleepCv.wait_for(aLock, std::chrono::milliseconds(500));
					Py_END_ALLOW_THREADS
				}
			}
			if (gSleepStop)
			{
				PyErr_SetString(PyExc_KeyboardInterrupt, "script stopped");
				return nullptr;
			}
		}
		Py_RETURN_NONE;
	}

	// vx: sleep until the level game clock reaches the given absolute time (same wait as VbSlp)
	PyObject* VbSlpUntil(PyObject*, PyObject* theArgs)
	{
		double aTargetTime = 0.0;
		if (!PyArg_ParseTuple(theArgs, "d", &aTargetTime))
			return nullptr;
		{
			std::unique_lock<std::mutex> aLock(gSleepMutex);
			while (!gSleepStop && gVxGameTime < aTargetTime)
			{
				Py_BEGIN_ALLOW_THREADS
				gSleepCv.wait_for(aLock, std::chrono::milliseconds(500));
				Py_END_ALLOW_THREADS
			}
			if (gSleepStop)
			{
				PyErr_SetString(PyExc_KeyboardInterrupt, "script stopped");
				return nullptr;
			}
		}
		Py_RETURN_NONE;
	}

	PyObject* VbPlt(PyObject*, PyObject* theArgs)
	{
		int aRow = 0;
		int aCol = 0;
		int aSlot = 0;
		if (!PyArg_ParseTuple(theArgs, "iii", &aRow, &aCol, &aSlot))
			return nullptr;
		if (aRow < 0 || aCol < 0 || aSlot < 0)
		{
			PyErr_SetString(PyExc_ValueError, "row/col/card_id must be >= 0");
			return nullptr;
		}
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			gQueue.push_back({VxCmdType::Plant, aRow, aCol, aSlot});
		}
		Py_RETURN_NONE;
	}

	PyObject* VbRmv(PyObject*, PyObject* theArgs)
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
			gQueue.push_back({VxCmdType::Shovel, aRow, aCol, 0});
		}
		Py_RETURN_NONE;
	}

	PyObject* VxRunQuery(VxQueryType theType)
	{
		{
			std::lock_guard<std::mutex> aLock(gQueryMutex);
			gQueryType = theType;
			gQueryPending = true;
		}
		PyObject* aResult = nullptr;
		Py_BEGIN_ALLOW_THREADS
		{
			std::unique_lock<std::mutex> aLock(gQueryMutex);
			if (gQueryCv.wait_for(aLock, std::chrono::seconds(5), [] { return gQueryResult != nullptr; }))
			{
				aResult = gQueryResult;
			}
			gQueryResult = nullptr;
			gQueryPending = false;
		}
		Py_END_ALLOW_THREADS
		if (!aResult)
		{
			PyErr_SetString(PyExc_RuntimeError, "query timed out (no board update)");
			return nullptr;
		}
		return aResult;
	}

	PyObject* VbGetZombies(PyObject*, PyObject*)
	{
		return VxRunQuery(VxQueryType::Zombies);
	}

	PyObject* VbGetPlants(PyObject*, PyObject*)
	{
		return VxRunQuery(VxQueryType::Plants);
	}

	PyObject* VbGetCards(PyObject*, PyObject*)
	{
		return VxRunQuery(VxQueryType::Cards);
	}

	PyObject* VbGetVases(PyObject*, PyObject*)
	{
		return VxRunQuery(VxQueryType::Vases);
	}

	PyObject* VbGetTime(PyObject*, PyObject*)
	{
		std::lock_guard<std::mutex> aLock(gSleepMutex);
		return PyFloat_FromDouble(gVxGameTime);
	}

	PyObject* VbPlc(PyObject*, PyObject* theArgs)
	{
		int aCoinID = 0;
		int aRow = 0;
		int aCol = 0;
		if (!PyArg_ParseTuple(theArgs, "iii", &aCoinID, &aRow, &aCol))
			return nullptr;
		if (aCoinID <= 0 || aRow < 0 || aCol < 0)
		{
			PyErr_SetString(PyExc_ValueError, "coin id must be positive, row/col >= 0");
			return nullptr;
		}
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			gQueue.push_back({VxCmdType::PlantCard, aRow, aCol, aCoinID});
		}
		Py_RETURN_NONE;
	}

	PyObject* VbRmvPlant(PyObject*, PyObject* theArgs)
	{
		int aPlantID = 0;
		if (!PyArg_ParseTuple(theArgs, "i", &aPlantID))
			return nullptr;
		if (aPlantID <= 0)
		{
			PyErr_SetString(PyExc_ValueError, "plant id must be positive");
			return nullptr;
		}
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			gQueue.push_back({VxCmdType::ShovelId, 0, 0, aPlantID});
		}
		Py_RETURN_NONE;
	}

	PyMethodDef gVbMethods[] = {
		{"brk", VbBrk, METH_VARARGS, "Queue a vase break at (row, col)."},
		{"brk_vase", VbBrkVase, METH_VARARGS, "Queue a break of the vase with the given id."},
		{"slp", VbSlp, METH_VARARGS, "Sleep for delay seconds; interruptible by StopScripts."},
		{"slp_until", VbSlpUntil, METH_VARARGS, "Sleep until the level game time reaches the given value; interruptible by StopScripts."},
		{"plt", VbPlt, METH_VARARGS, "Queue a plant from bank slot card_id at (row, col)."},
		{"rmv", VbRmv, METH_VARARGS, "Queue a shovel at (row, col)."},
		{"plc", VbPlc, METH_VARARGS, "Queue a plant from a dropped card (coin id) at (row, col)."},
		{"rmv_plant", VbRmvPlant, METH_VARARGS, "Queue a shovel of the plant with the given id."},
		{"get_zombies", VbGetZombies, METH_NOARGS, "Return a list of dicts describing the current zombies."},
		{"get_plants", VbGetPlants, METH_NOARGS, "Return a list of dicts describing the current plants."},
		{"get_cards", VbGetCards, METH_NOARGS, "Return a list of dicts describing the dropped seed cards."},
		{"get_vases", VbGetVases, METH_NOARGS, "Return a list of dicts describing the vases on the field."},
		{"time", VbGetTime, METH_NOARGS, "Return the current level game time in seconds (frozen while paused)."},
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
			std::wstring aDynloadPath = aStdlibPath + L"\\lib-dynload";
			if (std::filesystem::is_directory(aDynloadPath))
			{
				PyWideStringList_Append(&aConfig.module_search_paths, aDynloadPath.c_str());
			}
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

	void StartScripts(int theLevel, int theGameMode)
	{
		if (!gPythonReady || gScriptThread.joinable())
			return; // python not up, or a worker is still running; Board dtor normally stops it
		{
			std::lock_guard<std::mutex> aLock(gSleepMutex);
			gSleepStop = false;
			gStopInterruptFired = false;
		}
		{
			std::lock_guard<std::mutex> aLock(gScriptEndedMutex);
			gScriptEnded = false;
		}
		gScriptLevel = theLevel;
		gScriptGameMode = theGameMode;
		gScriptThread = std::thread(ScriptThreadMain);
	}

	// vx: wake any vb.slp() wait so the script thread can raise KeyboardInterrupt by itself
	void VxRequestScriptStop()
	{
		{
			std::lock_guard<std::mutex> aLock(gSleepMutex);
			gSleepStop = true;
		}
		gSleepCv.notify_all();
	}

	// vx: main thread feeds the script-visible game clock every frame and wakes slp waiters
	void VxUpdateGameTime(double theGameTime)
	{
		{
			std::lock_guard<std::mutex> aLock(gSleepMutex);
			gVxGameTime = theGameTime;
		}
		gSleepCv.notify_all();
	}

	// vx: fire the async interrupt at most once per stop; repeated stop requests
	// (user stop + level restart) would otherwise land KeyboardInterrupts at random
	// bytecode boundaries (finally blocks, driver tail) and escape the script driver
	void VxInterruptThread()
	{
		unsigned long aThreadId = gScriptThreadId.load();
		if (!aThreadId)
			return;
		bool aFire = false;
		{
			std::lock_guard<std::mutex> aLock(gSleepMutex);
			if (!gStopInterruptFired)
			{
				gStopInterruptFired = true;
				aFire = true;
			}
		}
		if (!aFire)
			return;
		// vx: PyThreadState_SetAsyncExc is a C API call and requires the GIL
		PyGILState_STATE aGilState = PyGILState_Ensure();
		PyThreadState_SetAsyncExc(aThreadId, PyExc_KeyboardInterrupt);
		PyGILState_Release(aGilState);
	}

	void StopScripts()
	{
		if (gScriptThread.joinable())
		{
			VxRequestScriptStop();
			VxInterruptThread();
			std::unique_lock<std::mutex> aLock(gScriptEndedMutex);
			if (!gScriptEndedCv.wait_for(aLock, std::chrono::seconds(3), [] { return gScriptEnded; }))
			{
				// ponytail: script did not stop (C-extension busy loop); abandon the thread
				// instead of freezing the game - the process exits eventually anyway
				gScriptThread.detach();
				ScriptLog("[vb] script did not stop within 3s; thread abandoned, restart the game");
				gPythonReady = false;
			}
			else
			{
				gScriptThread.join();
			}
		}
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			gQueue.clear();
		}
	}

	bool GetScaryPotLineup(int theLevel, int theSeed, std::vector<VxPotDef>& theOut)
	{
		if (gDataDir.empty())
			VxResolveDirs(); // vx: old VxCallLevelFunc resolved dirs per call; keep the lazy guard
		theOut.clear();
		VxLevelInfo aInfo;
		if (!VxLoadLevelInfo(theLevel, gDataDir / "adventure_info.csv", aInfo))
			return false;
		if (!aInfo.mValid)
			return false;
		return VxGeneratePots(aInfo, theSeed, theOut);
	}

	bool GetSceneLayout(int theLevel, int theSeed, std::vector<VxSceneDef>& theOut)
	{
		if (gDataDir.empty())
			VxResolveDirs();
		VxLevelInfo aInfo;
		if (!VxLoadLevelInfo(theLevel, gDataDir / "adventure_info.csv", aInfo))
			return false;
		// vx: unconfigured levels are normal (the old get_scene returned [])
		VxBuildScene(aInfo, theSeed, theOut);
		return true;
	}

	bool GetSlotSetup(int theLevel, int& theSun, std::vector<int>& theSlots)
	{
		if (gDataDir.empty())
			VxResolveDirs();
		theSlots.clear();
		VxLevelInfo aInfo;
		if (!VxLoadLevelInfo(theLevel, gDataDir / "adventure_info.csv", aInfo))
			return false;
		if (!aInfo.mValid)
			return false;
		return VxParseSlot(aInfo, theSun, theSlots);
	}

	bool GetRandomSeeds(int theLevel, std::vector<int>& theOut)
	{
		if (gDataDir.empty())
			VxResolveDirs();
		theOut.clear();
		VxLevelInfo aInfo;
		if (!VxLoadLevelInfo(theLevel, gDataDir / "adventure_info.csv", aInfo))
			return false;
		if (!aInfo.mValid)
			return false;
		VxParseRandomSeeds(aInfo, theOut);
		return true;
	}

	bool GetLevelStatistics(int theLevel, int& theAvgMs, int& theMaxMs, int& theSun)
	{
		if (gDataDir.empty())
			VxResolveDirs();
		VxLevelInfo aInfo;
		if (!VxLoadLevelInfo(theLevel, gDataDir / "adventure_info.csv", aInfo))
			return false;
		if (!aInfo.mValid)
			return false;
		return VxParseStatistics(aInfo, theAvgMs, theMaxMs, theSun);
	}

	bool GetScriptError(std::string& theText, int& theKind)
	{
		if (gScriptsDir.empty())
			VxResolveDirs();
		std::ifstream aStream(gScriptsDir / "vx_script_error.txt");
		if (!aStream)
			return false;
		std::string aKindLine;
		if (!std::getline(aStream, aKindLine))
			return false;
		theKind = aKindLine == "CE" ? 1 : aKindLine == "RE" ? 2 : 0;
		theText.assign(std::istreambuf_iterator<char>(aStream), std::istreambuf_iterator<char>());
		aStream.close();
		// vx: consume so each error is shown/recorded exactly once
		std::error_code anEc;
		std::filesystem::remove(gScriptsDir / "vx_script_error.txt", anEc);
		return theKind != 0;
	}

	void ProcessBoardQueue(Board* theBoard)
	{
		// vx: answer a pending vb.get_zombies() query on the game thread
		{
			std::lock_guard<std::mutex> aLock(gQueryMutex);
			if (gQueryPending)
			{
				PyGILState_STATE aGILState = PyGILState_Ensure();
				PyObject* aList = PyList_New(0);
				switch (gQueryType)
				{
				case VxQueryType::Zombies:
					for (Zombie* aZombie : theBoard->mZombies)
					{
						if (aZombie->mDead)
							continue;
						// vx: zombie snapshot exposes mVelX as "v" and frame counter as "age"
						PyObject* aDict = Py_BuildValue("{s:i,s:i,s:d,s:d,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i}", // vx: last s:i fills the "id" key, s:d fills "v", s:i fills "age"
							"row", aZombie->mRow,
							"col", theBoard->PixelToGridXKeepOnBoard(static_cast<int>(aZombie->mPosX), static_cast<int>(aZombie->mPosY)),
							"x", aZombie->mPosX,
							"v", aZombie->mVelX,
							"age", aZombie->mAnimCounter,
							"hp", aZombie->mBodyHealth,
							"helm", aZombie->mHelmHealth,
							"hp_max", aZombie->mBodyMaxHealth,
							"helm_max", aZombie->mHelmMaxHealth,
							"slow", aZombie->mChilledCounter,
							"typ", static_cast<int>(aZombie->mZombieType),
							"id", static_cast<int>(theBoard->mZombies.DataArrayGetID(aZombie)));
						PyList_Append(aList, aDict);
						Py_DECREF(aDict);
					}
					break;
				case VxQueryType::Plants:
					for (Plant* aPlant : theBoard->mPlants)
					{
						if (aPlant->mDead)
							continue;
						PyObject* aDict = Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:O,s:i,s:i}",
							"row", aPlant->mRow,
							"col", aPlant->mPlantCol,
							"hp", aPlant->mPlantHealth,
							"hp_max", aPlant->mPlantMaxHealth,
							"age", aPlant->mAnimCounter,
							"asleep", aPlant->mIsAsleep ? Py_True : Py_False,
							"typ", static_cast<int>(aPlant->mSeedType),
							"id", static_cast<int>(theBoard->mPlants.DataArrayGetID(aPlant)));
						PyList_Append(aList, aDict);
						Py_DECREF(aDict);
					}
					break;
				case VxQueryType::Vases:
					for (GridItem* aGridItem : theBoard->mGridItems)
					{
						if (aGridItem->mDead)
							continue;
						if (aGridItem->mGridItemType != GridItemType::GRIDITEM_SCARY_POT)
							continue;
						// vx: content_typ convention: pt/zt values, -1 empty, -25/-50/-75 sun (no -100)
						int aContentTyp = -1;
						switch (aGridItem->mScaryPotType)
						{
						case ScaryPotType::SCARYPOT_SEED:
							aContentTyp = static_cast<int>(aGridItem->mSeedType);
							break;
						case ScaryPotType::SCARYPOT_ZOMBIE:
							aContentTyp = 100 + static_cast<int>(aGridItem->mZombieType);
							break;
						case ScaryPotType::SCARYPOT_SUN:
							aContentTyp = -25 * std::clamp(aGridItem->mSunCount, 1, 3);
							break;
						default:
							aContentTyp = -1;
							break;
						}
						PyObject* aDict = Py_BuildValue("{s:i,s:i,s:i,s:i,s:O,s:i}",
							"row", aGridItem->mGridY,
							"col", aGridItem->mGridX,
							"vase_typ", static_cast<int>(aGridItem->mGridItemState),
							"content_typ", aContentTyp,
							"transparent", aGridItem->mTransparentCounter > 0 ? Py_True : Py_False,
							"id", static_cast<int>(theBoard->mGridItems.DataArrayGetID(aGridItem)));
						PyList_Append(aList, aDict);
						Py_DECREF(aDict);
					}
					break;
				case VxQueryType::Cards:
					for (Coin* aCoin : theBoard->mCoins)
					{
						if (aCoin->mDead)
							continue;
						if (aCoin->mType != CoinType::COIN_USABLE_SEED_PACKET)
							continue;
						PyObject* aDict = Py_BuildValue("{s:i,s:i,s:i}",
							"age", aCoin->mCoinAge,
							"typ", static_cast<int>(aCoin->mUsableSeedType),
							"id", static_cast<int>(theBoard->mCoins.DataArrayGetID(aCoin)));
						PyList_Append(aList, aDict);
						Py_DECREF(aDict);
					}
					break;
				}
				PyGILState_Release(aGILState);
				gQueryResult = aList;
				gQueryPending = false;
			}
		}
		gQueryCv.notify_all();

		std::vector<VxCmd> aCommands;
		{
			std::lock_guard<std::mutex> aLock(gQueueMutex);
			aCommands.swap(gQueue);
		}
		for (const VxCmd& aCommand : aCommands)
		{
			switch (aCommand.mType)
			{
			case VxCmdType::BreakPot:
			{
				GridItem* aPot = theBoard->GetScaryPotAt(aCommand.mCol, aCommand.mRow);
				if (!aPot)
				{
					ScriptLog("[vb] no pot at (" + std::to_string(aCommand.mRow) + ", " + std::to_string(aCommand.mCol) + ")");
					break;
				}
				theBoard->mChallenge->ScaryPotterOpenPot(aPot);
				break;
			}
			case VxCmdType::BreakPotId:
				theBoard->VxBreakVaseID(aCommand.mArg);
				break;
			case VxCmdType::Plant:
				theBoard->VxPlantFromBank(aCommand.mArg, aCommand.mCol, aCommand.mRow);
				break;
			case VxCmdType::Shovel:
				theBoard->VxShovelAt(aCommand.mCol, aCommand.mRow);
				break;
			case VxCmdType::PlantCard:
				theBoard->VxPlantFromCard(aCommand.mArg, aCommand.mCol, aCommand.mRow);
				break;
			case VxCmdType::ShovelId:
				theBoard->VxShovelPlantID(aCommand.mArg);
				break;
			}
			}
		}
	}

namespace VX
{
	// vx: player-driven world-6 runs (Run = trial, Submit = official)
	void RequestScriptRun(int theLevel, int theGameMode, bool theSubmit)
	{
		std::lock_guard<std::mutex> aLock(gRunMutex);
		gPendingRun = true;
		gPendingRunLevel = theLevel;
		gPendingRunMode = theGameMode;
		gTrialRun = !theSubmit;
		if (!theSubmit)
		{
			gTrialSeed = gTrialSeedCounter += 7919; // vx: each Run click gets a fresh non-repeating seed
		}
		else
		{
			gTrialSeed = 0; // vx: Submit uses the fixed CSV test seeds
		}
	}

	bool ConsumePendingScriptRun()
	{
		std::lock_guard<std::mutex> aLock(gRunMutex);
		if (!gPendingRun)
			return false;
		gPendingRun = false;
		gPendingRunLevel = 0;
		gPendingRunMode = 0;
		// vx: a fresh run must not inherit the previous run's error file
		std::error_code anEc;
		std::filesystem::remove(gScriptsDir / "vx_script_error.txt", anEc);
		return true;
	}

	bool IsTrialRun()
	{
		return gTrialRun;
	}

	void ClearRunFlags()
	{
		std::lock_guard<std::mutex> aLock(gRunMutex);
		gPendingRun = false;
		gTrialRun = true; // vx: without a Submit run, a world-6 win must not advance the save
	}

	void SetTrialSeed(int theSeed)
	{
		std::lock_guard<std::mutex> aLock(gRunMutex);
		gTrialSeed = theSeed;
	}

	int TakeTrialSeed()
	{
		std::lock_guard<std::mutex> aLock(gRunMutex);
		int aSeed = gTrialSeed;
		gTrialSeed = 0;
		return aSeed;
	}

	void SetRunLocked(bool theLocked)
	{
		gRunLocked = theLocked;
	}

	bool IsRunLocked()
	{
		return gRunLocked;
	}

	bool IsScriptRunning()
	{
		return gScriptThread.joinable();
	}

	bool IsScriptDone()
	{
		std::lock_guard<std::mutex> aLock(gScriptEndedMutex);
		return gScriptEnded;
	}

	void InterruptScript()
	{
		VxRequestScriptStop();
		VxInterruptThread();
	}

	std::wstring GetScriptsDir()
	{
		if (gScriptsDir.empty())
			VxResolveDirs();
		return gScriptsDir.wstring();
	}

	std::wstring GetLevelScriptPath(int theLevel)
	{
		if (gScriptsDir.empty())
			VxResolveDirs();
		int aArea = (theLevel - 1) / 10 + 1;
		int aSub = (theLevel - 1) % 10 + 1;
		char aName[128];
		snprintf(aName, sizeof(aName), "script_adventure_%d_%d.py", aArea, aSub);
		return (gScriptsDir / aName).wstring();
	}

	void LogSubmitStats(int theLevel, int theAvgMs, int theMaxMs, int theSun)
	{
		// vx: settlement log: prepend "# Submit at {ts}: {AvgMs}, {MaxMs}, {Sun}" to the level script
		std::filesystem::path aPath = GetLevelScriptPath(theLevel);
		time_t aNow = time(nullptr);
		struct tm aTm = {};
		localtime_s(&aTm, &aNow);
		char aTimeBuf[64];
		strftime(aTimeBuf, sizeof(aTimeBuf), "%Y-%m-%d %H:%M:%S", &aTm);
		std::string aLine = "# Submit at " + std::string(aTimeBuf) + ": " + std::to_string(theAvgMs) + ", " + std::to_string(theMaxMs) + ", " + std::to_string(theSun) + "\n";
		std::ifstream aIn(aPath, std::ios::binary);
		std::string aContent((std::istreambuf_iterator<char>(aIn)), std::istreambuf_iterator<char>());
		aIn.close();
		std::ofstream aOut(aPath, std::ios::binary | std::ios::trunc);
		aOut << aLine << aContent;
		aOut.close();
	}
}

#else // !VX_SCRIPT
// vx: stubs so call sites compile unchanged when Python scripting is disabled
#include "VxScript.h"

namespace VX
{
	void Init() {}
	void Shutdown() {}
	void StartScripts(int, int) {}
	void StopScripts() {}
	void ProcessBoardQueue(Board*) {}
	void VxUpdateGameTime(double) {}
	bool GetScaryPotLineup(int, int, std::vector<VxPotDef>&) { return false; }
	bool GetSceneLayout(int, int, std::vector<VxSceneDef>&) { return false; }
	bool GetSlotSetup(int, int&, std::vector<int>&) { return false; }
	bool GetRandomSeeds(int, std::vector<int>&) { return false; }
	bool GetLevelStatistics(int, int&, int&, int&) { return false; }
	void LogSubmitStats(int, int, int, int) {}
	bool GetScriptError(std::string&, int&) { return false; }
	void RequestScriptRun(int, int, bool) {}
	bool ConsumePendingScriptRun() { return false; }
	bool IsTrialRun() { return false; }
	void ClearRunFlags() {}
	void SetTrialSeed(int) {}
	int TakeTrialSeed() { return 0; }
	void SetRunLocked(bool) {}
	bool IsRunLocked() { return false; }
	bool IsScriptRunning() { return false; }
	bool IsScriptDone() { return false; }
	void InterruptScript() {}
	std::wstring GetScriptsDir() { return std::wstring(); }
	std::wstring GetLevelScriptPath(int) { return std::wstring(); }
}
#endif // VX_SCRIPT
