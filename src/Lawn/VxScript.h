	#ifndef __VXSCRIPT_H__
	#define __VXSCRIPT_H__

// vx: Python scripting bridge for player scripts (_vb module)

class Board;

namespace VX
{
	void Init();
	void Shutdown();
	void StartScripts();
	void StopScripts();
	void ProcessBoardQueue(Board* theBoard);
}

#endif // __VXSCRIPT_H__
