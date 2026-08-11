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
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "VxScript.h"
#include "Board.h"
#include "Challenge.h"

namespace
{
	enum class VxCmdType
	{
		BreakPot,
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

	// vx: generated from ConstEnums.h (gSeedNames)
	constexpr std::pair<const char*, int> gSeedNames[] = {
		{"PEASHOOTER", 0}, {"SUNFLOWER", 1}, {"CHERRYBOMB", 2}, {"WALLNUT", 3}, {"POTATOMINE", 4},
		{"SNOWPEA", 5}, {"CHOMPER", 6}, {"REPEATER", 7}, {"PUFFSHROOM", 8}, {"SUNSHROOM", 9},
		{"FUMESHROOM", 10}, {"GRAVEBUSTER", 11}, {"HYPNOSHROOM", 12}, {"SCAREDYSHROOM", 13},
		{"ICESHROOM", 14}, {"DOOMSHROOM", 15}, {"LILYPAD", 16}, {"SQUASH", 17}, {"THREEPEATER", 18},
		{"TANGLEKELP", 19}, {"JALAPENO", 20}, {"SPIKEWEED", 21}, {"TORCHWOOD", 22}, {"TALLNUT", 23},
		{"SEASHROOM", 24}, {"PLANTERN", 25}, {"CACTUS", 26}, {"BLOVER", 27}, {"SPLITPEA", 28},
		{"STARFRUIT", 29}, {"PUMPKINSHELL", 30}, {"MAGNETSHROOM", 31}, {"CABBAGEPULT", 32},
		{"FLOWERPOT", 33}, {"KERNELPULT", 34}, {"INSTANT_COFFEE", 35}, {"GARLIC", 36}, {"UMBRELLA", 37},
		{"MARIGOLD", 38}, {"MELONPULT", 39}, {"GATLINGPEA", 40}, {"TWINSUNFLOWER", 41},
		{"GLOOMSHROOM", 42}, {"CATTAIL", 43}, {"WINTERMELON", 44}, {"GOLD_MAGNET", 45},
		{"SPIKEROCK", 46}, {"COBCANNON", 47}, {"IMITATER", 48}, {"EXPLODE_O_NUT", 49},
		{"GIANT_WALLNUT", 50}, {"SPROUT", 51}, {"LEFTPEATER", 52}, {"NUM_SEED_TYPES", 53},
		{"BEGHOULED_BUTTON_SHUFFLE", 54}, {"BEGHOULED_BUTTON_CRATER", 55}, {"SLOT_MACHINE_SUN", 56},
		{"SLOT_MACHINE_DIAMOND", 57}, {"ZOMBIQUARIUM_SNORKLE", 58}, {"ZOMBIQUARIUM_TROPHY", 59},
		{"ZOMBIE_NORMAL", 60}, {"ZOMBIE_TRAFFIC_CONE", 61}, {"ZOMBIE_POLEVAULTER", 62},
		{"ZOMBIE_PAIL", 63}, {"ZOMBIE_LADDER", 64}, {"ZOMBIE_DIGGER", 65}, {"ZOMBIE_BUNGEE", 66},
		{"ZOMBIE_FOOTBALL", 67}, {"ZOMBIE_BALLOON", 68}, {"ZOMBIE_SCREEN_DOOR", 69}, {"ZOMBONI", 70},
		{"ZOMBIE_POGO", 71}, {"ZOMBIE_DANCER", 72}, {"ZOMBIE_GARGANTUAR", 73}, {"ZOMBIE_IMP", 74},
		{"NUM_SEEDS_IN_CHOOSER", 49},
	};

	// vx: generated from ConstEnums.h (gZombieNames)
	constexpr std::pair<const char*, int> gZombieNames[] = {
		{"NORMAL", 0}, {"FLAG", 1}, {"TRAFFIC_CONE", 2}, {"POLEVAULTER", 3}, {"PAIL", 4},
		{"NEWSPAPER", 5}, {"DOOR", 6}, {"FOOTBALL", 7}, {"DANCER", 8}, {"BACKUP_DANCER", 9},
		{"DUCKY_TUBE", 10}, {"SNORKEL", 11}, {"ZAMBONI", 12}, {"BOBSLED", 13}, {"DOLPHIN_RIDER", 14},
		{"JACK_IN_THE_BOX", 15}, {"BALLOON", 16}, {"DIGGER", 17}, {"POGO", 18}, {"YETI", 19},
		{"BUNGEE", 20}, {"LADDER", 21}, {"CATAPULT", 22}, {"GARGANTUAR", 23}, {"IMP", 24}, {"BOSS", 25},
		{"PEA_HEAD", 26}, {"WALLNUT_HEAD", 27}, {"JALAPENO_HEAD", 28}, {"GATLING_HEAD", 29},
		{"SQUASH_HEAD", 30}, {"TALLNUT_HEAD", 31}, {"REDEYE_GARGANTUAR", 32}, {"NUM_ZOMBIE_TYPES", 33},
		{"CACHED_POLEVAULTER_WITH_POLE", 34},
	};

	bool VxLookupName(const char* theName, const std::pair<const char*, int>* theNames, size_t theNameCount, int& theValue)
	{
		const char* aName = theName;
		if (strncmp(aName, "SEED_", 5) == 0)
			aName += 5;
		else if (strncmp(aName, "ZOMBIE_", 7) == 0)
			aName += 7;
		for (size_t i = 0; i < theNameCount; i++)
		{
			if (strcmp(theNames[i].first, aName) == 0)
			{
				theValue = theNames[i].second;
				return true;
			}
		}
		return false;
	}

	int VxDictInt(PyObject* theDict, const char* theKey, int theDefault)
	{
		PyObject* aValue = PyDict_GetItemString(theDict, theKey); // borrowed
		if (aValue && PyLong_Check(aValue))
			return static_cast<int>(PyLong_AsLong(aValue));
		return theDefault;
	}

	bool VxDictName(PyObject* theDict, const char* theKey, const std::pair<const char*, int>* theNames, size_t theNameCount, int& theValue)
	{
		PyObject* aValue = PyDict_GetItemString(theDict, theKey); // borrowed
		if (aValue && PyLong_Check(aValue))
		{
			theValue = static_cast<int>(PyLong_AsLong(aValue));
			return true;
		}
		if (aValue && PyUnicode_Check(aValue))
		{
			const char* aName = PyUnicode_AsUTF8(aValue);
			if (aName)
				return VxLookupName(aName, theNames, theNameCount, theValue);
		}
		return false;
	}


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
	// vx: player-driven world-6 runs (Run = trial, Submit = official)
	std::mutex gRunMutex;
	bool gPendingRun = false;
	int gPendingRunLevel = 0;
	int gPendingRunMode = 0;
	bool gTrialRun = false;
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
	// vx: import vx_init_lvl and call <theFuncName>(theLevel); caller holds the GIL and owns the result
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
			gScriptDirs.push_back(aLiveRoot / "src" / "python");
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

	PyObject* VxCallLevelFunc(const char* theFuncName, int theLevel, int theSeed)
	{
		if (gScriptDirs.empty())
			VxResolveDirs();
		VxSetupPythonPath();
		PyObject* aModule = PyImport_ImportModule("vx_init_lvl");
		if (!aModule)
		{
			PyErr_Clear();
			return nullptr;
		}
		PyObject* aResult = nullptr;
		PyObject* aFunc = PyObject_GetAttrString(aModule, theFuncName);
		if (aFunc && PyCallable_Check(aFunc))
		{
			PyObject* aArgs = Py_BuildValue("(ii)", theLevel, theSeed);
			aResult = PyObject_CallObject(aFunc, aArgs);
			Py_XDECREF(aArgs);
		}
		Py_XDECREF(aFunc);
		Py_XDECREF(aModule);
		return aResult;
	}



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
    except Exception:
        pass
    try:
        print("=== %s ===" % path)
        traceback.print_exc()
    except Exception:
        pass
    if log is not None:
        log.write("=== %s ===\n" % path)
        traceback.print_exc(file=log)
        log.flush()

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
    except BaseException:
        write_error("RE")  # runtime error
    finally:
        if out is not None:
            out.close()

# vx: run only this level's script, e.g. script_adventure_6_1.py
mode = getattr(sys, "vx_game_mode", "")
area = getattr(sys, "vx_area", 0)
sub = getattr(sys, "vx_sub", 0)
path = os.path.join(d, "script_%s_%d_%d.py" % (mode, area, sub))
if os.path.exists(path):
    run_script(path)
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
				Py_BEGIN_ALLOW_THREADS
				gSleepCv.wait_for(aLock, std::chrono::duration<double>(aDelay), [] { return gSleepStop; });
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
		{"slp", VbSlp, METH_VARARGS, "Sleep for delay seconds; interruptible by StopScripts."},
		{"plt", VbPlt, METH_VARARGS, "Queue a plant from bank slot card_id at (row, col)."},
		{"rmv", VbRmv, METH_VARARGS, "Queue a shovel at (row, col)."},
		{"plc", VbPlc, METH_VARARGS, "Queue a plant from a dropped card (coin id) at (row, col)."},
		{"rmv_plant", VbRmvPlant, METH_VARARGS, "Queue a shovel of the plant with the given id."},
		{"get_zombies", VbGetZombies, METH_NOARGS, "Return a list of dicts describing the current zombies."},
		{"get_plants", VbGetPlants, METH_NOARGS, "Return a list of dicts describing the current plants."},
		{"get_cards", VbGetCards, METH_NOARGS, "Return a list of dicts describing the dropped seed cards."},
		{"get_vases", VbGetVases, METH_NOARGS, "Return a list of dicts describing the vases on the field."},
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

	void StopScripts()
	{
		if (gScriptThread.joinable())
		{
			VxRequestScriptStop();
			// vx: interrupt a hanging script (3.12 time.sleep is async-exc interruptible)
			unsigned long aThreadId = gScriptThreadId.load();
			if (aThreadId)
			{
				// vx: PyThreadState_SetAsyncExc is a C API call and requires the GIL
				PyGILState_STATE aGilState = PyGILState_Ensure();
				PyThreadState_SetAsyncExc(aThreadId, PyExc_KeyboardInterrupt);
				PyGILState_Release(aGilState);
			}
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
		if (!gPythonReady)
			return false;
		PyGILState_STATE aGILState = PyGILState_Ensure();

		bool aOk = false;
		PyObject* aResult = VxCallLevelFunc("generate", theLevel, theSeed);
		if (aResult == nullptr)
		{
			ScriptLog("[vb] vx_init_lvl.generate failed for level " + std::to_string(theLevel));
		}
		PyErr_Clear();

		if (aResult && PyList_Check(aResult))
		{
			Py_ssize_t aLength = PyList_Size(aResult);
			for (Py_ssize_t i = 0; i < aLength; i++)
			{
				PyObject* aItem = PyList_GetItem(aResult, i); // borrowed
				if (!aItem || !PyDict_Check(aItem))
					continue;
				VxPotDef aDef;
				aDef.mRow = VxDictInt(aItem, "row", -1);
				aDef.mCol = VxDictInt(aItem, "col", -1);
				aDef.mCount = VxDictInt(aItem, "count", 1);
				aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_SEED);
				aDef.mZombie = static_cast<int>(ZombieType::ZOMBIE_INVALID);
				aDef.mSeed = static_cast<int>(SeedType::SEED_NONE);
				const char* aTypeName = nullptr;
				PyObject* aTypeObj = PyDict_GetItemString(aItem, "type"); // borrowed
				if (aTypeObj && PyUnicode_Check(aTypeObj))
					aTypeName = PyUnicode_AsUTF8(aTypeObj);
				if (aTypeName && strcmp(aTypeName, "zombie") == 0)
					aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_ZOMBIE);
				else if (aTypeName && strcmp(aTypeName, "sun") == 0)
					aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_SUN);
				else if (aTypeName && strcmp(aTypeName, "empty") == 0)
					aDef.mType = static_cast<int>(ScaryPotType::SCARYPOT_NONE);

				if (aDef.mType == static_cast<int>(ScaryPotType::SCARYPOT_ZOMBIE))
				{
					int aZombie = -1;
					if (!VxDictName(aItem, "zombie", gZombieNames, sizeof(gZombieNames) / sizeof(gZombieNames[0]), aZombie))
					{
						ScriptLog("[vb] vx_pots: bad zombie in item " + std::to_string(i));
						continue;
					}
					aDef.mZombie = aZombie;
				}
				else if (aDef.mType == static_cast<int>(ScaryPotType::SCARYPOT_SEED))
				{
					int aSeed = -1;
					if (!VxDictName(aItem, "seed", gSeedNames, sizeof(gSeedNames) / sizeof(gSeedNames[0]), aSeed))
					{
						ScriptLog("[vb] vx_pots: bad seed in item " + std::to_string(i));
						continue;
					}
					aDef.mSeed = aSeed;
				}
				theOut.push_back(aDef);
			}
			aOk = true;
		}
		else if (aResult)
		{
			ScriptLog("[vb] vx_pots.generate must return a list");
		}
		Py_XDECREF(aResult);
		PyGILState_Release(aGILState);
		return aOk;
	}

	bool GetSceneLayout(int theLevel, std::vector<VxSceneDef>& theOut)
	{
		if (!gPythonReady)
			return false;
		PyGILState_STATE aGILState = PyGILState_Ensure();

		bool aOk = false;
		PyObject* aResult = VxCallLevelFunc("get_scene", theLevel, 0);
		// vx: unconfigured levels are normal (get_scene returns []), so failures stay silent here
		PyErr_Clear();

		if (aResult && PyList_Check(aResult))
		{
			Py_ssize_t aLength = PyList_Size(aResult);
			for (Py_ssize_t i = 0; i < aLength; i++)
			{
				PyObject* aItem = PyList_GetItem(aResult, i); // borrowed
				if (!aItem || !PyDict_Check(aItem))
					continue;
				VxSceneDef aDef;
				aDef.mRow = VxDictInt(aItem, "row", -1);
				aDef.mCol = VxDictInt(aItem, "col", -1);
				aDef.mIsPlant = true;
				const char* aTypeName = nullptr;
				PyObject* aTypeObj = PyDict_GetItemString(aItem, "type"); // borrowed
				if (aTypeObj && PyUnicode_Check(aTypeObj))
					aTypeName = PyUnicode_AsUTF8(aTypeObj);
				if (aTypeName && strcmp(aTypeName, "zombie") == 0)
					aDef.mIsPlant = false;
				if (aDef.mIsPlant)
				{
					int aSeed = -1;
					if (!VxDictName(aItem, "seed", gSeedNames, sizeof(gSeedNames) / sizeof(gSeedNames[0]), aSeed))
						continue;
					aDef.mSeed = aSeed;
				}
				else
				{
					int aZombie = -1;
					if (!VxDictName(aItem, "zombie", gZombieNames, sizeof(gZombieNames) / sizeof(gZombieNames[0]), aZombie))
						continue;
					aDef.mZombie = aZombie;
				}
				theOut.push_back(aDef);
			}
			aOk = true;
		}
		Py_XDECREF(aResult);
		PyGILState_Release(aGILState);
		return aOk;
	}

	bool GetSlotSetup(int theLevel, int& theSun, std::vector<int>& theSlots)
	{
		if (!gPythonReady)
			return false;
		PyGILState_STATE aGILState = PyGILState_Ensure();

		bool aOk = false;
		PyObject* aResult = VxCallLevelFunc("get_slot", theLevel, 0);
		PyErr_Clear();
		if (aResult && PyDict_Check(aResult))
		{
			theSun = VxDictInt(aResult, "sun", -1);
			PyObject* aSlotsObj = PyDict_GetItemString(aResult, "slots"); // borrowed
			if (aSlotsObj && PyList_Check(aSlotsObj))
			{
				Py_ssize_t aLength = PyList_Size(aSlotsObj);
				for (Py_ssize_t i = 0; i < aLength; i++)
				{
					PyObject* aItem = PyList_GetItem(aSlotsObj, i); // borrowed
					if (aItem && PyLong_Check(aItem))
						theSlots.push_back(static_cast<int>(PyLong_AsLong(aItem)));
				}
			}
			aOk = true;
		}
		Py_XDECREF(aResult);
		PyGILState_Release(aGILState);
		return aOk;
	}

	bool GetRandomSeeds(int theLevel, std::vector<int>& theOut)
	{
		if (!gPythonReady)
			return false;
		PyGILState_STATE aGILState = PyGILState_Ensure();

		bool aOk = false;
		PyObject* aResult = VxCallLevelFunc("get_random_seeds", theLevel, 0);
		PyErr_Clear();
		if (aResult && PyList_Check(aResult))
		{
			Py_ssize_t aLength = PyList_Size(aResult);
			for (Py_ssize_t i = 0; i < aLength; i++)
			{
				PyObject* aItem = PyList_GetItem(aResult, i); // borrowed
				if (aItem && PyLong_Check(aItem))
					theOut.push_back(static_cast<int>(PyLong_AsLong(aItem)));
			}
			aOk = true;
		}
		Py_XDECREF(aResult);
		PyGILState_Release(aGILState);
		return aOk;
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
						PyObject* aDict = Py_BuildValue("{s:i,s:i,s:d,s:i,s:i,s:i,s:i,s:i,s:i}",
							"row", aZombie->mRow,
							"col", theBoard->PixelToGridXKeepOnBoard(static_cast<int>(aZombie->mPosX), static_cast<int>(aZombie->mPosY)),
							"x", aZombie->mPosX,
							"hp", aZombie->mBodyHealth,
							"helm", aZombie->mHelmHealth,
							"hp_max", aZombie->mBodyMaxHealth,
							"helm_max", aZombie->mHelmMaxHealth,
							"slow", aZombie->mChilledCounter,
							"typ", static_cast<int>(aZombie->mZombieType));
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
						PyObject* aDict = Py_BuildValue("{s:i,s:i,s:i,s:i,s:O}",
							"row", aGridItem->mGridY,
							"col", aGridItem->mGridX,
							"vase_typ", static_cast<int>(aGridItem->mGridItemState),
							"content_typ", aContentTyp,
							"transparent", aGridItem->mTransparentCounter > 0 ? Py_True : Py_False);
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
		unsigned long aThreadId = gScriptThreadId.load();
		if (aThreadId)
		{
			// vx: PyThreadState_SetAsyncExc is a C API call and requires the GIL
			PyGILState_STATE aGilState = PyGILState_Ensure();
			PyThreadState_SetAsyncExc(aThreadId, PyExc_KeyboardInterrupt);
			PyGILState_Release(aGilState);
		}
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
	bool GetScaryPotLineup(int, int, std::vector<VxPotDef>&) { return false; }
	bool GetSceneLayout(int, std::vector<VxSceneDef>&) { return false; }
	bool GetSlotSetup(int, int&, std::vector<int>&) { return false; }
	bool GetRandomSeeds(int, std::vector<int>&) { return false; }
	bool GetScriptError(std::string&, int&) { return false; }
	void RequestScriptRun(int, int, bool) {}
	bool ConsumePendingScriptRun() { return false; }
	bool IsTrialRun() { return false; }
	void ClearRunFlags() {}
	void SetRunLocked(bool) {}
	bool IsRunLocked() { return false; }
	bool IsScriptRunning() { return false; }
	bool IsScriptDone() { return false; }
	void InterruptScript() {}
	std::wstring GetScriptsDir() { return std::wstring(); }
	std::wstring GetLevelScriptPath(int) { return std::wstring(); }
}
#endif // VX_SCRIPT
