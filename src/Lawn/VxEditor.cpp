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
	Uint32 gLastSaveTick = 0;
	// vx: persistent output panel below the editor: streamed script prints + last error
	std::string gVxOutputText;
	std::string gVxErrorText;
	bool gVxErrorCompile = false;
	bool gVxPanelOpen = true;
	std::uintmax_t gVxOutputSize = 0;
	bool gVxOutputScrollToBottom = false;

	// vx: three window/editor width levels; the game view stays a fixed 800px strip
	constexpr int kVxWidthLevels = 3;
	constexpr int kVxWindowWidths[kVxWidthLevels] = { 1000, 1100, 1200 };
	int gVxWidthLevel = 0;

	std::filesystem::path VxOutputPath()
	{
		return std::filesystem::path(VX::GetScriptsDir()) / "vx_script_output.txt";
	}

	std::string VxReadFile(const std::filesystem::path& thePath)
	{
		std::ifstream aStream(thePath, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(aStream)), std::istreambuf_iterator<char>());
	}

	void VxLoadFile()
	{
		std::error_code anEc;
		auto aMtime = std::filesystem::last_write_time(gScriptPath, anEc);
		// vx: SetText turns the file's trailing newline into a phantom empty line that GetText
		// re-serializes as an extra blank line on save; drop it so open+save round-trips losslessly
		std::string aText = VxReadFile(gScriptPath);
		if (!aText.empty() && aText.back() == '\n')
			aText.pop_back();
		gTextEditor.SetText(aText);
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
		gLastSaveTick = SDL_GetTicks();
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
		SDL_SetWindowSize(gGameWindow, theEditorOpen ? kVxWindowWidths[gVxWidthLevel] : 800, 600);
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
				"bool", "open", "__file__", "__name__", "time" })
		{
			TextEditor::Identifier anId;
			anId.mDeclaration = "Python builtin";
			aDef.mIdentifiers.emplace(aName, anId);
		}
		// vx: Vatrix script commands get a dedicated yellow highlight (PreprocIdentifier slot)
		for (const char* aName : { "vb", "brk", "slp", "plt", "rmv", "get_zombies", "get_plants",
				"get_cards", "get_vases" })
		{
			TextEditor::Identifier anId;
			anId.mDeclaration = "Vatrix command";
			aDef.mPreprocIdentifiers.emplace(aName, anId);
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
		// vx: brighter-than-stock palette for readability on dark backgrounds
		TextEditor::Palette aPalette = TextEditor::GetDarkPalette();
		aPalette[(int)TextEditor::PaletteIndex::Default] = 0xffd0d0d0;
		aPalette[(int)TextEditor::PaletteIndex::Keyword] = 0xffe8b06a;
		aPalette[(int)TextEditor::PaletteIndex::String] = 0xff9090ff;
		aPalette[(int)TextEditor::PaletteIndex::CharLiteral] = 0xff90c0ff;
		aPalette[(int)TextEditor::PaletteIndex::Identifier] = 0xffe0e0e0;
		aPalette[(int)TextEditor::PaletteIndex::KnownIdentifier] = 0xffc8e878;
		aPalette[(int)TextEditor::PaletteIndex::Preprocessor] = 0xff80c0c0;
		aPalette[(int)TextEditor::PaletteIndex::PreprocIdentifier] = 0xffe8e848;
		aPalette[(int)TextEditor::PaletteIndex::Comment] = 0xff66aa66;
		aPalette[(int)TextEditor::PaletteIndex::MultiLineComment] = 0xff88bb88;
		aPalette[(int)TextEditor::PaletteIndex::LineNumber] = 0xffb0b060;
		gTextEditor.SetPalette(aPalette);
		// vx: no whitespace markers, spaces and tabs stay blank
		gTextEditor.SetShowWhitespaces(false);
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
		// vx: autosave dirty buffers every 2s (the manual Save button is gone);
		// a locked Submit run keeps the submitted code frozen on disk
		if (gBufferDirty && !VX::IsRunLocked() && SDL_GetTicks() - gLastSaveTick > 2000)
			VxSaveBuffer(false);

		// vx: the game view is docked to a fixed 800px strip; the panel fills the rest
		float aEditorWidth = io.DisplaySize.x - 800.0f;
		if (aEditorWidth < 100.0f)
			aEditorWidth = 100.0f;
		const float kVxOutputHeight = 220.0f;
		float aOutputHeight = gVxPanelOpen ? kVxOutputHeight : 0.0f;
		ImVec2 aPanelOrigin = ImVec2(io.DisplaySize.x - aEditorWidth, 0.0f);
		ImGui::SetNextWindowPos(aPanelOrigin);
		ImGui::SetNextWindowSize(ImVec2(aEditorWidth, io.DisplaySize.y - aOutputHeight));
		ImGuiWindowFlags aFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
		ImGui::Begin("VxEditor", nullptr, aFlags);

		// vx: Run lives on the bottom-left script bar; Save is replaced by autosave
		if (ImGui::Button("Close"))
			gPendingAction = VX::VxEditorAction::Close;
		ImGui::SameLine();
		if (ImGui::Button(gVxWidthLevel == 0 ? "Width 1/3" : gVxWidthLevel == 1 ? "Width 2/3" : "Width 3/3"))
		{
			gVxWidthLevel = (gVxWidthLevel + 1) % kVxWidthLevels;
			VxSetEditorWindowSize(true);
		}
		ImGui::SameLine();
		// vx: the persistent output panel toggles open/closed from here
		if (ImGui::Button(gVxPanelOpen ? "Collapse Output" : "Expand Output"))
			gVxPanelOpen = !gVxPanelOpen;
		ImGui::Separator();

		gTextEditor.Render("VxPythonEditor");
		if (gTextEditor.IsTextChanged())
			gBufferDirty = true;
		ImGui::End();

		// vx: stream the player script's print output into the panel (line-buffered file)
		{
			std::error_code anEc;
			std::uintmax_t aSize = std::filesystem::file_size(VxOutputPath(), anEc);
			if (anEc) // no output yet or a fresh run cleared it
			{
				if (!gVxOutputText.empty())
					gVxOutputText.clear();
				gVxOutputSize = 0;
			}
			else if (aSize != gVxOutputSize)
			{
				gVxOutputSize = aSize;
				std::ifstream aStream(VxOutputPath(), std::ios::binary);
				gVxOutputText.assign(std::istreambuf_iterator<char>(aStream), std::istreambuf_iterator<char>());
				gVxOutputScrollToBottom = true;
			}
		}

		// vx: persistent output panel below the editor: prints + last error, auto-scrolls
		if (gVxPanelOpen)
		{
			ImGui::SetNextWindowPos(ImVec2(aPanelOrigin.x, io.DisplaySize.y - kVxOutputHeight));
			ImGui::SetNextWindowSize(ImVec2(aEditorWidth, kVxOutputHeight));
			ImGui::Begin("VxScriptOutput", nullptr, aFlags);
			if (gVxErrorText.empty())
				ImGui::TextDisabled("Output");
			else
				ImGui::TextColored(gVxErrorCompile ? ImVec4(1.0f, 0.65f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
					gVxErrorCompile ? "Compile Error (CE)" : "Runtime Error (RE)");
			ImGui::Separator();
			ImGui::BeginChild("VxOutputScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
			if (!gVxOutputText.empty())
				ImGui::TextUnformatted(gVxOutputText.c_str());
			if (!gVxErrorText.empty())
			{
				ImGui::TextWrapped("%s", gVxErrorText.c_str());
				gVxOutputScrollToBottom = true;
			}
			if (gVxOutputScrollToBottom)
			{
				ImGui::SetScrollHereY(1.0f);
				gVxOutputScrollToBottom = false;
			}
			ImGui::EndChild();
			ImGui::End();
		}

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
		// vx: seed a missing level script from templates/<same name> first, else an empty file
		if (!aPath.empty() && !std::filesystem::exists(aPath))
		{
			std::filesystem::path aTemplate = aPath.parent_path() / "templates" / aPath.filename();
			std::error_code anEc;
			if (!std::filesystem::exists(aTemplate, anEc))
				std::ofstream(aPath, std::ios::binary | std::ios::trunc); // no template -> empty file
			else
				std::filesystem::copy_file(aTemplate, aPath, std::filesystem::copy_options::overwrite_existing, anEc);
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

	void VxEditorShowError(const std::string& theText, bool theCompileError)
	{
		gVxErrorText = theText;
		gVxErrorCompile = theCompileError;
		gVxOutputScrollToBottom = true;
	}

	void VxEditorClearError()
	{
		gVxErrorText.clear();
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
	void VxEditorShowError(const std::string&, bool) {}
	void VxEditorClearError() {}
}
#endif // VX_SCRIPT
