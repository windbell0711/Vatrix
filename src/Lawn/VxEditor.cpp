// vx: in-game Python code editor (Dear ImGui + ImGuiColorTextEdit), docked right strip for world 6
#ifdef VX_SCRIPT

#include <SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "TextEditor.h"

#include "VxEditor.h"
#include "VxScript.h"
#include "SexyAppFramework/graphics/GLInterface.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
	// vx: editor state (lazily initialized on first open, GL context is current by then)
	bool gImGuiReady = false;
	bool gEditorOpen = false;
	SDL_Window* gGameWindow = nullptr;
	TextEditor gTextEditor;
	std::filesystem::path gScriptPath;
	std::filesystem::file_time_type gFileMtime{};
	bool gBufferDirty = false;
	bool gConflictPending = false;
	VX::VxEditorAction gPendingAction = VX::VxEditorAction::None;
	int gEditorLevel = 0;

	// vx: single point for the editor panel rect; v2 floating-window layout hooks in here
	constexpr float kVxEditorWidth = 250.0f;

	// vx: player-facing template written by the "New" button
	const char* kVxEditorTemplate = R"PY(# 6-X level script template
import vb

# Break a vase: vb.brk(row, col)   (1-based)
# Plant a card:  vb.plt(row, col, card_id)
# Wait:          vb.slp(seconds)
vb.brk(3, 7)
)PY";

	std::string VxReadFile(const std::filesystem::path& thePath)
	{
		std::ifstream aStream(thePath, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(aStream)), std::istreambuf_iterator<char>());
	}

	void VxLoadFile()
	{
		std::error_code anEc;
		auto aMtime = std::filesystem::last_write_time(gScriptPath, anEc);
		gTextEditor.SetText(VxReadFile(gScriptPath));
		if (!anEc)
			gFileMtime = aMtime;
		gBufferDirty = false;
		gConflictPending = false;
	}

	// vx: save the editor buffer; returns false when an external change conflict is pending
	bool VxSaveBuffer(bool theForce)
	{
		if (!gEditorOpen)
			return true;
		std::error_code anEc;
		auto aDiskMtime = std::filesystem::last_write_time(gScriptPath, anEc);
		if (!theForce && gBufferDirty && !anEc && aDiskMtime != gFileMtime)
		{
			gConflictPending = true;
			return false;
		}
		{
			std::ofstream aStream(gScriptPath, std::ios::binary | std::ios::trunc);
			aStream << gTextEditor.GetText();
		}
		gBufferDirty = false;
		gFileMtime = std::filesystem::last_write_time(gScriptPath, anEc);
		return true;
	}

	void VxSetEditorWindowSize(bool theEditorOpen)
	{
		if (!gGameWindow)
			return;
		Uint32 aFlags = SDL_GetWindowFlags(gGameWindow);
		if (aFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
			return;
		SDL_SetWindowSize(gGameWindow, theEditorOpen ? 1050 : 800, 600);
	}

	// vx: Python language definition for ImGuiColorTextEdit (upstream has no Python preset)
	void VxSetupPythonLanguage()
	{
		static bool sSetup = false;
		if (sSetup)
			return;
		sSetup = true;
		TextEditor::LanguageDefinition aDef;
		aDef.mName = "Python";
		aDef.mKeywords = {
			"and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del",
			"elif", "else", "except", "False", "finally", "for", "from", "global", "if", "import",
			"in", "is", "lambda", "None", "nonlocal", "not", "or", "pass", "raise", "return",
			"True", "try", "while", "with", "yield",
		};
		for (const char* aName : { "print", "len", "range", "int", "float", "str", "list", "dict",
				"bool", "open", "__file__", "__name__", "vb", "time", "brk", "slp", "plt", "rmv",
				"get_zombies", "get_plants", "get_cards", "get_vases" })
		{
			TextEditor::Identifier anId;
			anId.mDeclaration = "Vatrix builtin";
			aDef.mIdentifiers.emplace(aName, anId);
		}
		aDef.mTokenRegexStrings = {
			{R"([a-zA-Z_][a-zA-Z0-9_]*)", TextEditor::PaletteIndex::Identifier},
			{R"(0[xX][0-9a-fA-F]+)", TextEditor::PaletteIndex::Number},
			{R"(\d+\.\d*|\d*\.\d+)", TextEditor::PaletteIndex::Number},
			{R"(\d+)", TextEditor::PaletteIndex::Number},
			{R"("([^"\\]|\\.)*")", TextEditor::PaletteIndex::String},
			{R"('([^'\\]|\\.)*')", TextEditor::PaletteIndex::String},
			{R"(#.*$)", TextEditor::PaletteIndex::Comment},
		};
		aDef.mSingleLineComment = "#";
		aDef.mPreprocChar = '$'; // keep '#' a plain comment, not preprocessor
		aDef.mCaseSensitive = true;
		aDef.mAutoIndentation = true;
		gTextEditor.SetLanguageDefinition(aDef);
	}

	bool VxImGuiInit()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		// vx: merge a CJK font (player code/comments may be Chinese); fall back to ASCII-only
		ImFontConfig aCfg;
		aCfg.MergeMode = true;
		aCfg.OversampleH = 2;
		aCfg.OversampleV = 2;
		static const char* kFontCandidates[] = {
			"C:/Windows/Fonts/msyh.ttc",
			"C:/Windows/Fonts/msyhbd.ttc",
			"C:/Windows/Fonts/simhei.ttf",
			"C:/Windows/Fonts/simsun.ttc",
		};
		for (const char* aPath : kFontCandidates)
		{
			if (std::filesystem::exists(aPath))
			{
				// vx: explicit default size so the CJK merge below has a matching reference size
				ImFontConfig aDefaultCfg;
				aDefaultCfg.SizePixels = 16.0f;
				io.Fonts->AddFontDefault(&aDefaultCfg);
				io.FontDefault = io.Fonts->AddFontFromFileTTF(aPath, 16.0f, &aCfg, io.Fonts->GetGlyphRangesChineseFull());
				break;
			}
		}
		ImGui::StyleColorsDark();
		ImGuiStyle& aStyle = ImGui::GetStyle();
		aStyle.WindowRounding = 0.0f;
		aStyle.WindowBorderSize = 1.0f;

		if (!ImGui_ImplSDL2_InitForOpenGL(gGameWindow, SDL_GL_GetCurrentContext()))
			return false;
		// vx: the game uses an OpenGL ES 2.0 context; the backend is compiled with IMGUI_IMPL_OPENGL_ES2
		if (!ImGui_ImplOpenGL3_Init("#version 100"))
			return false;
		gImGuiReady = true;
		return true;
	}

	void VxBuildEditorUI()
	{
		ImGuiIO& io = ImGui::GetIO();
		// vx: v2 floating-window layout hooks in here; v1 docks a 250px strip on the right
		ImVec2 aPanelOrigin = ImVec2(io.DisplaySize.x - kVxEditorWidth, 0.0f);
		ImGui::SetNextWindowPos(aPanelOrigin);
		ImGui::SetNextWindowSize(ImVec2(kVxEditorWidth, io.DisplaySize.y));
		ImGuiWindowFlags aFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
		ImGui::Begin("VxEditor", nullptr, aFlags);

		if (ImGui::Button("Save"))
			VxSaveBuffer(false);
		ImGui::SameLine();
		if (ImGui::Button("Run"))
		{
			// vx: a pending external-change conflict keeps the panel open until resolved
			if (VxSaveBuffer(false))
				gPendingAction = VX::VxEditorAction::RunTrial;
		}
		ImGui::SameLine();
		if (ImGui::Button("Close"))
			gPendingAction = VX::VxEditorAction::Close;
		ImGui::Separator();

		gTextEditor.Render("VxPythonEditor");
		if (gTextEditor.IsTextChanged())
			gBufferDirty = true;
		ImGui::End();

		if (gConflictPending)
		{
			ImGui::OpenPopup("External change");
			gConflictPending = false;
		}
		if (ImGui::BeginPopupModal("External change", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("The script file changed on disk.");
			ImGui::Text("Overwrite it with the editor buffer, or reload the file?");
			if (ImGui::Button("Overwrite"))
			{
				VxSaveBuffer(true);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload"))
			{
				VxLoadFile();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	int VxEditorEventWatch(void*, SDL_Event* theEvent)
	{
		// vx: forward SDL events to ImGui (events stay in the queue for the game itself)
		if (gImGuiReady)
			ImGui_ImplSDL2_ProcessEvent(theEvent);
		return 0;
	}

	void VxRenderFrame()
	{
		if (!gImGuiReady)
			return;
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		if (gEditorOpen)
			VxBuildEditorUI();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}

namespace VX
{
	void VxEditorInit()
	{
		SDL_AddEventWatch(VxEditorEventWatch, nullptr);
		gVxEditorDockedFn = VxEditorDocked;
		gVxRenderImGuiFn = VxRenderFrame;
	}

	void VxEditorShutdown()
	{
		gVxEditorDockedFn = nullptr;
		gVxRenderImGuiFn = nullptr;
		SDL_DelEventWatch(VxEditorEventWatch, nullptr);
		if (gImGuiReady)
		{
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplSDL2_Shutdown();
			ImGui::DestroyContext();
			gImGuiReady = false;
		}
		gEditorOpen = false;
	}

	void VxEditorOpen(int theLevel, void* theWindow)
	{
		if (gEditorOpen)
			return;
		gEditorLevel = theLevel;
		gScriptPath = VX::GetLevelScriptPath(theLevel);
		if (gScriptPath.empty())
			return;
		gGameWindow = (SDL_Window*)theWindow;
		if (!gImGuiReady && !VxImGuiInit())
			return;
		VxSetupPythonLanguage();
		VxLoadFile();
		VxSetEditorWindowSize(true);
		gEditorOpen = true;
	}

	void VxEditorOpenScript(int theLevel, void* theWindow)
	{
		std::filesystem::path aPath = VX::GetLevelScriptPath(theLevel);
		// vx: Open instead of New: only seed a fresh template when the level script does not exist yet
		if (!aPath.empty() && !std::filesystem::exists(aPath))
		{
			std::ofstream aStream(aPath, std::ios::binary | std::ios::trunc);
			aStream << kVxEditorTemplate;
		}
		// vx: when the editor is already open (Open clicked again), reload the file
		if (gEditorOpen)
		{
			VxLoadFile();
			return;
		}
		VxEditorOpen(theLevel, theWindow);
	}

	void VxEditorClose()
	{
		if (!gEditorOpen)
			return;
		// vx: an unresolved external-change conflict keeps the panel open
		if (gBufferDirty && !VxSaveBuffer(false))
			return;
		VxSetEditorWindowSize(false);
		gEditorOpen = false;
	}

	bool VxEditorIsOpen()
	{
		return gEditorOpen;
	}

	bool VxEditorDocked()
	{
		return gEditorOpen && gImGuiReady;
	}

	bool VxEditorWantsMouse()
	{
		return gImGuiReady && gEditorOpen && ImGui::GetIO().WantCaptureMouse;
	}

	bool VxEditorWantsKeyboard()
	{
		return gImGuiReady && gEditorOpen && ImGui::GetIO().WantCaptureKeyboard;
	}

	bool VxEditorSave()
	{
		return VxSaveBuffer(false);
	}

	VxEditorAction VxEditorTakePendingAction()
	{
		VxEditorAction aAction = gPendingAction;
		gPendingAction = VxEditorAction::None;
		return aAction;
	}
}

#else // !VX_SCRIPT
// vx: stubs so call sites compile unchanged when Python scripting is disabled
#include "VxEditor.h"

namespace VX
{
	void VxEditorInit() {}
	void VxEditorShutdown() {}
	void VxEditorOpen(int, void*) {}
	void VxEditorOpenScript(int, void*) {}
	void VxEditorClose() {}
	bool VxEditorIsOpen() { return false; }
	bool VxEditorDocked() { return false; }
	bool VxEditorWantsMouse() { return false; }
	bool VxEditorWantsKeyboard() { return false; }
	bool VxEditorSave() { return true; }
	VxEditorAction VxEditorTakePendingAction() { return VxEditorAction::None; }
}
#endif // VX_SCRIPT
