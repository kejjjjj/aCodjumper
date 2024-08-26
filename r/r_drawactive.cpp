#include "r_drawactive.hpp"
#include "utils/hook.hpp"
#include <r/gui/r_main_gui.hpp>
#include <r/r_utils.hpp>
#include <r/r_drawtools.hpp>
#include <cj/cj_fps.hpp>
#include <cl/cl_utils.hpp>
#include <cg/cg_local.hpp>
#include <cg/cg_offsets.hpp>

#if(DEBUG_SUPPORT)
#include "../aMovementRecorder/movement_recorder/mr_debug.hpp"
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"
#endif

void CG_DrawActive()
{

	if (R_NoRender())
#if(DEBUG_SUPPORT)
		return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#else
		return;
#endif


#if(DEBUG_SUPPORT)

	if (const auto pb = CStaticMovementRecorder::Instance->GetDebugPlayback())
		pb->CG_Render();

	const std::string text = std::format("buttons: {}", CL_GetUserCmd(clients->cmdNumber-1)->buttons);
	R_AddCmdDrawTextWithEffects(text, "fonts/normalFont", fvec2{ 310, 400 }, { 0.4f, 0.5f }, 0.f, 3, vec4_t{ 1,1,1,1 }, vec4_t{ 1,0,0,0 });
#endif

	CG_AutoFPSHud(&cgs->predictedPlayerState, CL_GetUserCmd(clients->cmdNumber-1));


#if(DEBUG_SUPPORT)
	return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#endif

}


void RB_DrawDebug([[maybe_unused]] GfxViewParms* viewParms)
{

	if (R_NoRender())
#if(DEBUG_SUPPORT)
		return hooktable::find<void, GfxViewParms*>(HOOK_PREFIX(__func__))->call(viewParms);
#else
		return;
#endif

#if(DEBUG_SUPPORT)
	hooktable::find<void, GfxViewParms*>(HOOK_PREFIX(__func__))->call(viewParms);

	if (const auto pb = CStaticMovementRecorder::Instance->GetDebugPlayback())
		pb->RB_Render(viewParms);

#endif

	



}