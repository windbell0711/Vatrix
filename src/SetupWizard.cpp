// vx: first-run setup wizard (VatrixSetup.exe, Windows only):
// license -> quiz -> main.pak/properties -> shortcuts -> done.
// UTF-8 source, wide-string UI, no .rc text resources (Chinese-safe with MinGW).

#include <windows.h>
#include <bcrypt.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <fstream>
#include <cwchar>
#include <cwctype>
#include <string>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// control ids
// ---------------------------------------------------------------------------
enum
{
	IDC_TITLE = 100,
	IDC_LICENSE,
	IDC_AGREE,
	IDC_Q1LBL, IDC_Q1A, IDC_Q1B, IDC_Q1C, IDC_Q1D, IDC_HINT1,
	IDC_Q2LBL, IDC_Q2A, IDC_Q2B, IDC_Q2C, IDC_Q2D, IDC_HINT2,
	IDC_PAKLBL, IDC_PAK_EDIT, IDC_PAK_BROWSE, IDC_PAK_STATUS, IDC_PROPS_STATUS,
	IDC_PAK_HINT, IDC_PAK_LINK,
	IDC_CHKLBL, IDC_CHK_DESK, IDC_CHK_MENU, IDC_SHORTCUT_NOTE,
	IDC_SUMMARY, IDC_CHK_LAUNCH, IDC_DONE_NOTE,
	IDC_BTN_BACK = 200, IDC_BTN_NEXT, IDC_BTN_CANCEL,
};

// ---------------------------------------------------------------------------
// quiz content (placeholder; edit the tables below to change questions/hints)
// ---------------------------------------------------------------------------
struct QuizQuestion
{
	const wchar_t* text;
	const wchar_t* options[4];
	int correct;
	const wchar_t* okHint;
	const wchar_t* wrongHint;
};

static const QuizQuestion gQuiz[2] = {
	{
		L"1. 火爆辣椒的攻击范围是？",
		{ L"3x3矩形", L"r1.5圆", L"一行", L"一列" },
		2,
		L"回答正确！本游戏需要玩家已经通关植僵一代，并对各个植物和僵尸的特性比较了解。",
		L"回答错误。请您确保玩过植僵一代，并对各个植物和僵尸的特性有所了解，否则可能不适合此款游戏。",
	},
	{
		L"2. 在 python 3.12 中，下面哪行代码会引发报错？",
		{ L"a = 1", L"b=[0][1]", L"c=abs(-1)", L"if 1>2: print(3)" },
		1,
		L"回答正确！本游戏中，部分关卡需要玩家手写python脚本，因此需要玩家具备python入门基础。",
		L"回答错误。请您确保具备编程入门基础，了解变量、函数、分支、循环等基本概念，否则可能不适合此款游戏。",
	},
};

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------
struct WizardState
{
	int page = 0;
	bool agree = false;
	int q1 = -1;
	int q2 = -1;
	fs::path pakPath;
	bool propsFound = false;
	bool pakCopied = false;
	bool propsCopied = false;
	bool shortcutDesk = true;
	bool shortcutMenu = false;
	bool deskOk = false;
	bool menuOk = false;
	bool gameExeExists = false;
	bool launch = true;
	bool fromGame = false;
	bool repair = false;
};

static HINSTANCE gInst = nullptr;
static HWND gWnd = nullptr;
static HFONT gFont = nullptr;
static WizardState gState;

static HWND gTitle;
static HWND gLicense, gAgree;
static HWND gQ1Lbl, gQ1[4], gHint1;
static HWND gQ2Lbl, gQ2[4], gHint2;
static HWND gPakLbl, gPakEdit, gPakBrowse, gPakStatus, gPropsStatus, gPakHint, gPakLink;
static HWND gChkLbl, gChkDesk, gChkMenu, gShortcutNote;
static HWND gSummary, gChkLaunch, gDoneNote;
static HWND gBtnBack, gBtnNext, gBtnCancel;

static const wchar_t* gPageTitles[] = {
	L"第 1 步 / 共 5 步 · 授权信息",
	L"第 2 步 / 共 5 步 · 基础知识问答",
	L"第 3 步 / 共 5 步 · 选择游戏资源数据",
	L"第 4 步 / 共 5 步 · 创建快捷方式",
	L"第 5 步 / 共 5 步 · 完成",
};

// vx: operation-instructions webpage opened by the link on the main.pak page (TODO: fill in)
static const wchar_t* gHelpUrl = L"https://pan.baidu.com/s/1ezUXYDc_ui88BLG8ShM6UQ?pwd=mv4e";

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static fs::path ExeDir()
{
	wchar_t aBuf[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, aBuf, MAX_PATH);
	return fs::path(aBuf).parent_path();
}

// vx: SHA-256 of a file as lowercase hex; empty on failure
static std::string ComputeFileSha256(const fs::path& aPath)
{
	std::ifstream aFile(aPath, std::ios::binary);
	if (!aFile)
	{
		return {};
	}
	BCRYPT_ALG_HANDLE aAlg = nullptr;
	if (BCryptOpenAlgorithmProvider(&aAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
	{
		return {};
	}
	BCRYPT_HASH_HANDLE aHash = nullptr;
	std::string aResult;
	if (BCryptCreateHash(aAlg, &aHash, nullptr, 0, nullptr, 0, 0) == 0)
	{
		char aBuf[1 << 16];
		bool aOk = true;
		while (aFile)
		{
			aFile.read(aBuf, sizeof(aBuf));
			const std::streamsize aGot = aFile.gcount();
			if (aGot > 0 && BCryptHashData(aHash, reinterpret_cast<PUCHAR>(aBuf),
					static_cast<ULONG>(aGot), 0) != 0)
			{
				aOk = false;
				break;
			}
		}
		UCHAR aDigest[32] = {};
		if (aOk && BCryptFinishHash(aHash, aDigest, sizeof(aDigest), 0) == 0)
		{
			static const char aHex[] = "0123456789abcdef";
			aResult.reserve(sizeof(aDigest) * 2);
			for (UCHAR aByte : aDigest)
			{
				aResult += aHex[aByte >> 4];
				aResult += aHex[aByte & 0xF];
			}
		}
		BCryptDestroyHash(aHash);
	}
	BCryptCloseAlgorithmProvider(aAlg, 0);
	return aResult;
}

// vx: expected SHA-256 for a path from resources.sha256 (next to the exe); empty if absent
static std::string ExpectedResourceHash(const std::string& aRelPath)
{
	std::ifstream aManifest(ExeDir() / L"resources.sha256");
	if (!aManifest)
	{
		return {};
	}
	std::string aLine;
	while (std::getline(aManifest, aLine))
	{
		if (!aLine.empty() && aLine.back() == '\r')
		{
			aLine.pop_back();
		}
		if (aLine.empty() || aLine[0] == '#')
		{
			continue;
		}
		if (aLine.size() < 66 || aLine.compare(64, 2, "  ") != 0)
		{
			continue;
		}
		std::string aEntryPath = aLine.substr(66);
		for (char& aCh : aEntryPath)
		{
			if (aCh == '/')
			{
				aCh = '\\';
			}
		}
		if (aEntryPath == aRelPath)
		{
			return aLine.substr(0, 64);
		}
	}
	return {};
}

static HWND MakeCtrl(const wchar_t* aClass, const wchar_t* aText, DWORD aStyle,
	int aX, int aY, int aW, int aH, int aId, DWORD aExStyle = 0)
{
	return CreateWindowExW(aExStyle, aClass, aText, WS_CHILD | WS_VISIBLE | aStyle,
		aX, aY, aW, aH, gWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(aId)), gInst, nullptr);
}

static HWND MakeLabel(const wchar_t* aText, int aX, int aY, int aW, int aH, int aId)
{
	return MakeCtrl(L"STATIC", aText, SS_LEFT | SS_EDITCONTROL, aX, aY, aW, aH, aId);
}

static HWND MakeEdit(const wchar_t* aText, int aX, int aY, int aW, int aH, int aId)
{
	return MakeCtrl(L"EDIT", aText,
		ES_LEFT | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
		aX, aY, aW, aH, aId, WS_EX_CLIENTEDGE);
}

static HWND MakeButton(const wchar_t* aText, int aX, int aY, int aW, int aH, int aId)
{
	return MakeCtrl(L"BUTTON", aText, BS_PUSHBUTTON | WS_TABSTOP, aX, aY, aW, aH, aId);
}

static HWND MakeCheck(const wchar_t* aText, int aX, int aY, int aW, int aH, int aId)
{
	return MakeCtrl(L"BUTTON", aText, BS_AUTOCHECKBOX | WS_TABSTOP, aX, aY, aW, aH, aId);
}

static HWND MakeRadio(const wchar_t* aText, int aX, int aY, int aW, int aH, int aId, bool aGroupStart = false)
{
	return MakeCtrl(L"BUTTON", aText, BS_AUTORADIOBUTTON | WS_TABSTOP | (aGroupStart ? WS_GROUP : 0),
		aX, aY, aW, aH, aId);
}

static void CheckRadio(HWND* aRadios, int aSel)
{
	for (int i = 0; i < 4; i++)
	{
		SendMessageW(aRadios[i], BM_SETCHECK, i == aSel ? BST_CHECKED : BST_UNCHECKED, 0);
	}
}

static bool IsChecked(HWND aCtrl)
{
	return SendMessageW(aCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static fs::path ShellFolderPath(int aCsidl)
{
	wchar_t aBuf[MAX_PATH] = {};
	if (FAILED(SHGetFolderPathW(nullptr, aCsidl, nullptr, SHGFP_TYPE_CURRENT, aBuf)))
	{
		return {};
	}
	return fs::path(aBuf);
}

static bool CreateShortcut(const fs::path& aLnkPath, const fs::path& aTarget)
{
	IShellLinkW* aLink = nullptr;
	if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
			IID_IShellLinkW, reinterpret_cast<void**>(&aLink))))
	{
		return false;
	}
	aLink->SetPath(aTarget.wstring().c_str());
	aLink->SetWorkingDirectory(aTarget.parent_path().wstring().c_str());
	aLink->SetIconLocation(aTarget.wstring().c_str(), 0);
	IPersistFile* aPersist = nullptr;
	bool aOk = false;
	if (SUCCEEDED(aLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&aPersist))))
	{
		aOk = SUCCEEDED(aPersist->Save(aLnkPath.wstring().c_str(), TRUE));
		aPersist->Release();
	}
	aLink->Release();
	return aOk;
}

// ---------------------------------------------------------------------------
// page logic
// ---------------------------------------------------------------------------
static void UpdateNav()
{
	bool aCanNext = false;
	switch (gState.page)
	{
	case 0: aCanNext = gState.agree; break;
	case 1: aCanNext = gState.q1 >= 0 && gState.q2 >= 0; break;
	case 2: aCanNext = !gState.pakPath.empty(); break;
	default: aCanNext = true; break;
	}
	EnableWindow(gBtnNext, aCanNext);
	EnableWindow(gBtnBack, gState.page > 0 && gState.page < 4);
	ShowWindow(gBtnBack, gState.page < 4 ? SW_SHOW : SW_HIDE);
	SetWindowTextW(gBtnNext, gState.page == 4 ? L"完成" : L"下一步");
	SetWindowTextW(gTitle, gPageTitles[gState.page]);
}

static void UpdateHints()
{
	SetWindowTextW(gHint1, gState.q1 < 0 ? L""
		: (gState.q1 == gQuiz[0].correct ? gQuiz[0].okHint : gQuiz[0].wrongHint));
	SetWindowTextW(gHint2, gState.q2 < 0 ? L""
		: (gState.q2 == gQuiz[1].correct ? gQuiz[1].okHint : gQuiz[1].wrongHint));
}

// clear the mutable content of a page so re-entering it (via Back) starts fresh
static void ClearPageState(int aPage)
{
	switch (aPage)
	{
	case 0:
		gState.agree = false;
		SendMessageW(gAgree, BM_SETCHECK, BST_UNCHECKED, 0);
		break;
	case 1:
		gState.q1 = -1;
		gState.q2 = -1;
		CheckRadio(gQ1, -1);
		CheckRadio(gQ2, -1);
		SetWindowTextW(gHint1, L"");
		SetWindowTextW(gHint2, L"");
		break;
	case 2:
		gState.pakPath.clear();
		gState.propsFound = false;
		gState.pakCopied = false;
		gState.propsCopied = false;
		SetWindowTextW(gPakEdit, L"");
		SetWindowTextW(gPakStatus, L"");
		SetWindowTextW(gPropsStatus, L"");
		break;
	case 3:
		gState.shortcutDesk = true;
		gState.shortcutMenu = false;
		gState.deskOk = false;
		gState.menuOk = false;
		SendMessageW(gChkDesk, BM_SETCHECK, BST_CHECKED, 0);
		SendMessageW(gChkMenu, BM_SETCHECK, BST_UNCHECKED, 0);
		break;
	}
}

static void ShowPage(int aPage)
{
	gState.page = aPage;
	ShowWindow(gLicense, aPage == 0);
	ShowWindow(gAgree, aPage == 0);
	ShowWindow(gQ1Lbl, aPage == 1);
	ShowWindow(gHint1, aPage == 1);
	ShowWindow(gQ2Lbl, aPage == 1);
	ShowWindow(gHint2, aPage == 1);
	for (int i = 0; i < 4; i++)
	{
		ShowWindow(gQ1[i], aPage == 1);
		ShowWindow(gQ2[i], aPage == 1);
	}
	ShowWindow(gPakLbl, aPage == 2);
	ShowWindow(gPakEdit, aPage == 2);
	ShowWindow(gPakBrowse, aPage == 2);
	ShowWindow(gPakStatus, aPage == 2);
	ShowWindow(gPropsStatus, aPage == 2);
	ShowWindow(gPakHint, aPage == 2);
	ShowWindow(gPakLink, aPage == 2);
	ShowWindow(gChkLbl, aPage == 3);
	ShowWindow(gChkDesk, aPage == 3);
	ShowWindow(gChkMenu, aPage == 3);
	ShowWindow(gShortcutNote, aPage == 3);
	ShowWindow(gSummary, aPage == 4);
	ShowWindow(gChkLaunch, aPage == 4);
	ShowWindow(gDoneNote, aPage == 4);
	UpdateNav();
}

static void BrowsePak()
{
	wchar_t aBuf[MAX_PATH] = {};
	OPENFILENAMEW aOfn = {};
	aOfn.lStructSize = sizeof(aOfn);
	aOfn.hwndOwner = gWnd;
	aOfn.lpstrFilter = L"PvZ 游戏数据 (*.pak)\0*.pak\0所有文件 (*.*)\0*.*\0";
	aOfn.lpstrFile = aBuf;
	aOfn.nMaxFile = MAX_PATH;
	aOfn.lpstrTitle = L"选择 main.pak";
	aOfn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	if (!GetOpenFileNameW(&aOfn))
	{
		return;
	}
	fs::path aPak(aBuf);
	if (_wcsicmp(aPak.filename().c_str(), L"main.pak") != 0)
	{
		MessageBoxW(gWnd, L"请选择名为 main.pak 的文件（来自你自己的《植物大战僵尸》安装目录）。",
			L"Vatrix 配置向导", MB_OK | MB_ICONWARNING);
		return;
	}
	// vx: verify the picked main.pak against resources.sha256 before accepting it
	const std::string aExpected = ExpectedResourceHash("main.pak");
	if (!aExpected.empty())
	{
		const std::string aActual = ComputeFileSha256(aPak);
		if (aActual != aExpected)
		{
			SetWindowTextW(gPakStatus, L"main.pak 校验不通过：所选文件不是正确版本，请重新选择。");
			MessageBoxW(gWnd, L"所选 main.pak 不是本游戏支持的版本，请重新选择正确版本的文件。",
				L"Vatrix 配置向导", MB_OK | MB_ICONWARNING);
			return;
		}
	}
	// vx: require a properties folder next to the picked main.pak
	if (!fs::is_directory(aPak.parent_path() / L"properties"))
	{
		SetWindowTextW(gPakStatus, L"同目录未找到 properties 文件夹，请重新选择。");
		MessageBoxW(gWnd, L"在 main.pak 所在目录没有找到 properties 文件夹。\n请选择来自完整游戏安装目录的 main.pak（需与 properties 文件夹在同一目录）。",
			L"Vatrix 配置向导", MB_OK | MB_ICONWARNING);
		return;
	}
	gState.pakPath = aPak;
	gState.propsFound = true;
	SetWindowTextW(gPakEdit, aPak.wstring().c_str());
	SetWindowTextW(gPakStatus, L"main.pak：已选择");
	SetWindowTextW(gPropsStatus, L"properties 文件夹：已在同目录找到");
	UpdateNav();
}

// returns false to stay on the page
static bool DoCopy()
{
	if (gState.pakPath.empty())
	{
		return false;
	}
	const fs::path aTarget = ExeDir();

	const fs::path aDstPak = aTarget / L"main.pak";
	try
	{
		if (!fs::exists(aDstPak) || !fs::equivalent(gState.pakPath, aDstPak))
		{
			fs::copy_file(gState.pakPath, aDstPak, fs::copy_options::overwrite_existing);
		}
		gState.pakCopied = true;
	}
	catch (const fs::filesystem_error& e)
	{
		const fs::path& aPath = e.path1().empty() ? e.path2() : e.path1();
		MessageBoxW(gWnd, (L"复制 main.pak 失败：\n" + aPath.wstring() +
			L"\n\n请确认当前目录可写（不要放在 Program Files 等系统目录），然后重试。").c_str(),
			L"Vatrix 配置向导", MB_OK | MB_ICONERROR);
		return false;
	}

	if (gState.propsFound)
	{
		const fs::path aSrcProps = gState.pakPath.parent_path() / L"properties";
		const fs::path aDstProps = aTarget / L"properties";
		try
		{
			if (fs::exists(aDstProps) && fs::equivalent(aSrcProps, aDstProps))
			{
				gState.propsCopied = true;
			}
			else
			{
				fs::create_directories(aDstProps);
				for (const auto& aEntry : fs::recursive_directory_iterator(aSrcProps))
				{
					if (aEntry.is_regular_file())
					{
						fs::copy_file(aEntry.path(),
							aDstProps / fs::relative(aEntry.path(), aSrcProps),
							fs::copy_options::overwrite_existing);
					}
				}
				gState.propsCopied = true;
			}
		}
		catch (const fs::filesystem_error& e)
		{
			const fs::path& aPath = e.path1().empty() ? e.path2() : e.path1();
			MessageBoxW(gWnd, (L"复制 properties 文件夹失败：\n" + aPath.wstring() +
				L"\n\n请确认当前目录可写，然后重试。").c_str(),
				L"Vatrix 配置向导", MB_OK | MB_ICONERROR);
			return false;
		}
	}
	return true;
}

static void DoShortcuts()
{
	const fs::path aGameExe = ExeDir() / L"Vatrix.exe";
	gState.gameExeExists = fs::is_regular_file(aGameExe);
	if (!gState.gameExeExists)
	{
		return;
	}
	if (gState.shortcutDesk)
	{
		gState.deskOk = CreateShortcut(ShellFolderPath(CSIDL_DESKTOPDIRECTORY) / L"Vatrix.lnk", aGameExe);
	}
	if (gState.shortcutMenu)
	{
		gState.menuOk = CreateShortcut(ShellFolderPath(CSIDL_PROGRAMS) / L"Vatrix.lnk", aGameExe);
	}
}

static void UpdateSummary()
{
	std::wstring aText = L"配置完成！\r\n\r\n";
	aText += gState.pakCopied ? L"· main.pak：已复制\r\n" : L"· main.pak：未复制\r\n";
	if (gState.propsCopied)
	{
		aText += L"· properties 文件夹：已复制\r\n";
	}
	else if (gState.pakCopied)
	{
		aText += L"· properties 文件夹：未找到（游戏可能无法正常运行）\r\n";
	}
	else
	{
		aText += L"· properties 文件夹：未复制\r\n";
	}
	aText += gState.deskOk ? L"· 桌面快捷方式：已创建\r\n" : L"· 桌面快捷方式：未创建\r\n";
	aText += gState.menuOk ? L"· 开始菜单快捷方式：已创建\r\n" : L"· 开始菜单快捷方式：未创建\r\n";
	if (!gState.gameExeExists)
	{
		aText += L"\r\n提示：未找到本目录中的 Vatrix.exe，无法启动游戏或创建快捷方式。\r\n";
	}
	SetWindowTextW(gSummary, aText.c_str());
}

static void Finish()
{
	if (!gState.fromGame && gState.launch && gState.gameExeExists)
	{
		const std::wstring aCmd = L"\"" + (ExeDir() / L"Vatrix.exe").wstring() + L"\"";
		STARTUPINFOW aSi = {};
		aSi.cb = sizeof(aSi);
		PROCESS_INFORMATION aPi = {};
		CreateProcessW(nullptr, const_cast<wchar_t*>(aCmd.c_str()), nullptr, nullptr,
			FALSE, 0, nullptr, ExeDir().c_str(), &aSi, &aPi);
	}
	DestroyWindow(gWnd);
}

// ---------------------------------------------------------------------------
// main window
// ---------------------------------------------------------------------------
static void ConfirmClose()
{
	if (MessageBoxW(gWnd, L"确定要退出配置向导吗？", L"Vatrix 配置向导",
			MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
	{
		DestroyWindow(gWnd);
	}
}

static LRESULT CALLBACK WndProc(HWND aWnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam)
{
	switch (aMsg)
	{
	case WM_CREATE:
	{
		// gWnd is still unset while CreateWindowExW sends WM_CREATE; use aWnd as the parent
		gWnd = aWnd;
		gTitle = MakeLabel(gPageTitles[0], 20, 12, 600, 28, IDC_TITLE);

		// page 0: license
		gLicense = MakeEdit(
			L"欢迎使用 Vatrix —— 植物大战僵尸非官方改版（基于 PvZ-Portable）\r\n\r\n"
			L"【授权信息】\r\n"
			L"· 本项目基于 PvZ-Portable 开发，遵循 LGPL-3.0-or-later 开源协议，可自由使用与分发。\r\n"
			L"· Vatrix 是植物大战僵尸的非官方改版，与 PopCap / EA 无任何关联。\r\n"
			L"· 本项目不包含任何 PopCap / EA 版权素材：游戏数据（main.pak、properties 等）需要玩家自备正版游戏。\r\n"
			L"· 请勿传播未授权的游戏数据文件。\r\n\r\n"
			L"【运行说明】\r\n"
			L"· 本向导将引导你完成授权确认、基础问答、游戏数据配置与快捷方式创建。\r\n"
			L"· 游戏自带 Python 3.12 脚本功能（VX_SCRIPT），不需要玩家自备python环境。",
			20, 52, 600, 360, IDC_LICENSE);
		gAgree = MakeCheck(L"我已阅读并同意上述内容", 20, 424, 560, 26, IDC_AGREE);

		// page 1: quiz
		gQ1Lbl = MakeLabel(gQuiz[0].text, 20, 52, 600, 24, IDC_Q1LBL);
		for (int i = 0; i < 4; i++)
		{
			gQ1[i] = MakeRadio(gQuiz[0].options[i], 36, 80 + i * 28, 300, 24, IDC_Q1A + i, i == 0);
		}
		gHint1 = MakeEdit(L"", 36, 196, 560, 48, IDC_HINT1);
		gQ2Lbl = MakeLabel(gQuiz[1].text, 20, 256, 600, 24, IDC_Q2LBL);
		for (int i = 0; i < 4; i++)
		{
			gQ2[i] = MakeRadio(gQuiz[1].options[i], 36, 284 + i * 28, 300, 24, IDC_Q2A + i, i == 0);
		}
		gHint2 = MakeEdit(L"", 36, 396, 560, 48, IDC_HINT2);

		// page 2: main.pak
		gPakLbl = MakeLabel(L"请选择 main.pak 文件：\r\n它位于你自己的植物大战僵尸安装目录中，通常与 properties 文件夹在同一目录。",
			20, 52, 600, 48, IDC_PAKLBL);
		gPakEdit = MakeCtrl(L"EDIT", L"", ES_LEFT | ES_READONLY | WS_TABSTOP,
			20, 112, 460, 26, IDC_PAK_EDIT, WS_EX_CLIENTEDGE);
		gPakBrowse   = MakeButton(L"浏览...", 490, 110, 130, 30, IDC_PAK_BROWSE);
		gPakStatus   = MakeLabel(L"", 20, 152, 600, 24, IDC_PAK_STATUS);
		gPropsStatus = MakeLabel(L"", 20, 182, 600, 24, IDC_PROPS_STATUS);
		gPakHint = MakeEdit(
			L"提示：如果您本地没有 植僵年度加强英文版 的原版游戏资源，请花费21元到Steam上购买GOTY原版，而不是点击下面的按钮免费下载游戏资源。",
			20, 220, 600, 56, IDC_PAK_HINT);
		gPakLink = MakeButton(L"游戏链接...", 20, 290, 140, 30, IDC_PAK_LINK);

		// page 3: shortcuts
		gChkLbl = MakeLabel(L"是否创建快捷方式？", 20, 52, 600, 24, IDC_CHKLBL);
		gChkDesk = MakeCheck(L"在桌面创建快捷方式", 36, 92, 560, 26, IDC_CHK_DESK);
		gChkMenu = MakeCheck(L"在开始菜单创建快捷方式", 36, 128, 560, 26, IDC_CHK_MENU);
		gShortcutNote = MakeLabel(L"快捷方式指向本目录的 Vatrix.exe（名称：Vatrix）。",
			20, 180, 600, 40, IDC_SHORTCUT_NOTE);

		// page 4: done
		gSummary = MakeEdit(L"", 20, 52, 600, 300, IDC_SUMMARY);
		gChkLaunch = MakeCheck(L"启动 Vatrix", 20, 366, 400, 26, IDC_CHK_LAUNCH);
		gDoneNote = MakeLabel(L"", 20, 400, 600, 40, IDC_DONE_NOTE);

		gBtnCancel = MakeButton(L"取消", 360, 468, 80, 30, IDC_BTN_CANCEL);
		gBtnBack = MakeButton(L"上一步", 450, 468, 80, 30, IDC_BTN_BACK);
		gBtnNext = MakeButton(L"下一步", 540, 468, 80, 30, IDC_BTN_NEXT);

		SendMessageW(gChkDesk, BM_SETCHECK, gState.shortcutDesk ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(gChkMenu, BM_SETCHECK, gState.shortcutMenu ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(gChkLaunch, BM_SETCHECK, gState.launch ? BST_CHECKED : BST_UNCHECKED, 0);
		if (gState.fromGame)
		{
			EnableWindow(gChkLaunch, FALSE);
			SetWindowTextW(gDoneNote, L"向导由游戏自动启动；关闭向导后游戏将自动继续。");
		}

		if (gState.repair)
		{
			// vx: repair mode: skip license/quiz, go straight to re-picking main.pak
			ShowPage(2);
			SetWindowTextW(gPakStatus, L"检测到资源文件校验失败：请重新选择正确版本的 main.pak。");
		}
		else
		{
			ShowPage(0);
		}
		return 0;
	}

	case WM_COMMAND:
	{
		const int aId = LOWORD(aWParam);
		const bool aClicked = HIWORD(aWParam) == BN_CLICKED;
		switch (aId)
		{
		case IDC_AGREE:
			gState.agree = IsChecked(gAgree);
			UpdateNav();
			break;
		case IDC_Q1A: case IDC_Q1B: case IDC_Q1C: case IDC_Q1D:
			gState.q1 = aId - IDC_Q1A;
			CheckRadio(gQ1, gState.q1);
			UpdateHints();
			UpdateNav();
			break;
		case IDC_Q2A: case IDC_Q2B: case IDC_Q2C: case IDC_Q2D:
			gState.q2 = aId - IDC_Q2A;
			CheckRadio(gQ2, gState.q2);
			UpdateHints();
			UpdateNav();
			break;
		case IDC_PAK_BROWSE:
			if (aClicked)
			{
				BrowsePak();
			}
			break;
		case IDC_PAK_LINK:
			if (aClicked)
			{
				if (gHelpUrl[0] == L'\0')
				{
					MessageBoxW(gWnd, L"操作说明网页地址尚未配置，敬请期待。",
						L"Vatrix 配置向导", MB_OK | MB_ICONINFORMATION);
				}
				else if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open",
						gHelpUrl, nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
				{
					MessageBoxW(gWnd, (std::wstring(L"无法打开网页：") + gHelpUrl).c_str(),
						L"Vatrix 配置向导", MB_OK | MB_ICONWARNING);
				}
			}
			break;
		case IDC_CHK_DESK:
			gState.shortcutDesk = IsChecked(gChkDesk);
			break;
		case IDC_CHK_MENU:
			gState.shortcutMenu = IsChecked(gChkMenu);
			break;
		case IDC_CHK_LAUNCH:
			gState.launch = IsChecked(gChkLaunch);
			break;
		case IDC_BTN_BACK:
			if (aClicked && gState.page > 0 && gState.page < 4)
			{
				ClearPageState(gState.page - 1);
				ShowPage(gState.page - 1);
			}
			break;
		case IDC_BTN_NEXT:
			if (!aClicked)
			{
				break;
			}
			if (gState.page == 4)
			{
				Finish();
			}
			else if (gState.page == 2 ? DoCopy() : true)
			{
				if (gState.page == 3)
				{
					DoShortcuts();
					UpdateSummary();
				}
				ShowPage(gState.page + 1);
			}
			break;
		case IDC_BTN_CANCEL:
			if (aClicked)
			{
				ConfirmClose();
			}
			break;
		}
		return 0;
	}

	case WM_KEYDOWN:
		if (aWParam == VK_RETURN && IsWindowEnabled(gBtnNext))
		{
			SendMessageW(gBtnNext, BM_CLICK, 0, 0);
			return 0;
		}
		if (aWParam == VK_ESCAPE)
		{
			SendMessageW(gWnd, WM_CLOSE, 0, 0);
			return 0;
		}
		break;

	case WM_CLOSE:
		ConfirmClose();
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(aWnd, aMsg, aWParam, aLParam);
}

int WINAPI wWinMain(HINSTANCE aInst, HINSTANCE, PWSTR aCmdLine, int aCmdShow)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	SetCurrentDirectoryW(ExeDir().c_str());
	gState.fromGame = wcsstr(aCmdLine, L"--from-game") != nullptr;
	gState.repair = wcsstr(aCmdLine, L"--repair") != nullptr;

	WNDCLASSW aWndClass = {};
	aWndClass.lpfnWndProc = WndProc;
	aWndClass.hInstance = aInst;
	// vx: background brush so hiding the previous page erases its text (no ghosting)
	aWndClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	aWndClass.hIcon = LoadIconW(aInst, MAKEINTRESOURCEW(101));
	aWndClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
	aWndClass.lpszClassName = L"VatrixSetupWizard";
	if (!RegisterClassW(&aWndClass))
	{
		return 1;
	}

	gInst = aInst;
	gFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

	RECT aRect = { 0, 0, 640, 520 };
	AdjustWindowRect(&aRect, WS_OVERLAPPEDWINDOW, FALSE);
	gWnd = CreateWindowExW(0, L"VatrixSetupWizard", L"Vatrix 配置向导", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, aRect.right - aRect.left, aRect.bottom - aRect.top,
		nullptr, nullptr, aInst, nullptr);
	if (!gWnd)
	{
		return 1;
	}

	// apply the custom font to every child control
	if (gFont)
	{
		HWND aChild = GetWindow(gWnd, GW_CHILD);
		while (aChild)
		{
			SendMessageW(aChild, WM_SETFONT, reinterpret_cast<WPARAM>(gFont), TRUE);
			aChild = GetWindow(aChild, GW_HWNDNEXT);
		}
	}

	ShowWindow(gWnd, aCmdShow);
	UpdateWindow(gWnd);

	MSG aMsg = {};
	while (GetMessageW(&aMsg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&aMsg);
		DispatchMessageW(&aMsg);
	}

	if (gFont)
	{
		DeleteObject(gFont);
	}
	return static_cast<int>(aMsg.wParam);
}
