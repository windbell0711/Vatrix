// vx: Python scripting bridge - player scripts in ./scripts run on a worker thread
#ifdef VX_SCRIPT

#include <Python.h>

#include <cstring>
#include <filesystem>
#include <fstream>
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
		std::wstring aScriptsWide = gScriptsDir.wstring();
		std::wstring aDataWide = gDataDir.wstring();
		PySys_SetObject("vx_scripts_dir", PyUnicode_FromWideChar(aScriptsWide.c_str(), static_cast<Py_ssize_t>(aScriptsWide.size())));
		PySys_SetObject("vx_data_dir", PyUnicode_FromWideChar(aDataWide.c_str(), static_cast<Py_ssize_t>(aDataWide.size())));
	}

	PyObject* VxCallLevelFunc(const char* theFuncName, int theLevel)
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
			PyObject* aArgs = Py_BuildValue("(i)", theLevel);
			aResult = PyObject_CallObject(aFunc, aArgs);
			Py_XDECREF(aArgs);
		}
		Py_XDECREF(aFunc);
		Py_XDECREF(aModule);
		return aResult;
	}



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
    ns = {"__file__": path, "__name__": "__main__"}
    try:
        with open(path, "rb") as f:
            src = f.read()
        exec(compile(src, path, "exec"), ns)
    except BaseException:
        # vx: show the error on the console too (dev builds have CONSOLE=ON)
        try:
            print("=== %s ===" % path)
            traceback.print_exc()
        except Exception:
            pass
        if log is not None:
            log.write("=== %s ===\n" % path)
            traceback.print_exc(file=log)
            log.flush()

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

	PyMethodDef gVbMethods[] = {
		{"brk", VbBrk, METH_VARARGS, "Queue a vase break at (row, col)."},
		{"plt", VbPlt, METH_VARARGS, "Queue a plant from bank slot card_id at (row, col)."},
		{"rmv", VbRmv, METH_VARARGS, "Queue a shovel at (row, col)."},
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
		gScriptLevel = theLevel;
		gScriptGameMode = theGameMode;
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

	bool GetScaryPotLineup(int theLevel, std::vector<VxPotDef>& theOut)
	{
		if (!gPythonReady)
			return false;
		PyGILState_STATE aGILState = PyGILState_Ensure();

		bool aOk = false;
		PyObject* aResult = VxCallLevelFunc("generate", theLevel);
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
		PyObject* aResult = VxCallLevelFunc("get_scene", theLevel);
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
		PyObject* aResult = VxCallLevelFunc("get_slot", theLevel);
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

	void ProcessBoardQueue(Board* theBoard)
	{
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
			}
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
	void StartScripts(int, int) {}
	void StopScripts() {}
	void ProcessBoardQueue(Board*) {}
	bool GetScaryPotLineup(int, std::vector<VxPotDef>&) { return false; }
	bool GetSceneLayout(int, std::vector<VxSceneDef>&) { return false; }
	bool GetSlotSetup(int, int&, std::vector<int>&) { return false; }
}
#endif // VX_SCRIPT
