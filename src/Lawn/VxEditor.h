#ifndef __VXEDITOR_H__
#define __VXEDITOR_H__

// vx: in-game Python code editor (Dear ImGui), docked as a right-side strip on world-6 levels

namespace VX
{
	enum class VxEditorAction
	{
		None,
		RunTrial, // editor panel "运行": save, restart the level and run the script as a trial
		Close,
	};

	void VxEditorInit();
	void VxEditorShutdown();
	void VxEditorOpen(int theLevel, void* theWindow);
	void VxEditorOpenScript(int theLevel, void* theWindow);
	void VxEditorClose();
	bool VxEditorIsOpen();
	bool VxEditorDocked();
	bool VxEditorWantsMouse();
	bool VxEditorWantsKeyboard();
	bool VxEditorSave();
	VxEditorAction VxEditorTakePendingAction();
}

#endif // __VXEDITOR_H__
