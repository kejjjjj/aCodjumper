#include "r_drawactive.hpp"
#include "utils/hook.hpp"
#include <r/gui/r_main_gui.hpp>
#include <r/r_utils.hpp>
#include <r/r_drawtools.hpp>
#include <cj/cj_fps.hpp>
#include <cl/cl_utils.hpp>
#include <cg/cg_local.hpp>
#include <cg/cg_offsets.hpp>

void CG_DrawActive()
{

	if (R_NoRender())
#if(DEBUG_SUPPORT)
		return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#else
		return;
#endif

	CG_AutoFPSHud(&cgs->predictedPlayerState, CL_GetUserCmd(clients->cmdNumber-1));

#if(DEBUG_SUPPORT)
	return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#endif

}