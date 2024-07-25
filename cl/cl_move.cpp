#include "cl_move.hpp"
#include <utils/hook.hpp>

#include <cg/cg_local.hpp>
#include <cg/cg_offsets.hpp>
#include <cl/cl_utils.hpp>

#include <cj/cj_move.hpp>
#include <cj/cj_fps.hpp>
#include <cj/cj_rpg.hpp>

#include <shared/sv_shared.hpp>

void CL_FinishMove(usercmd_s* cmd)
{

#if(DEBUG_SUPPORT)
	hooktable::find<void, usercmd_s*>(HOOK_PREFIX(__func__))->call(cmd);
#endif
	auto ps = &cgs->predictedPlayerState;
	auto oldcmd = CL_GetUserCmd(clients->cmdNumber - 1);

#if(!DEBUG_SUPPORT)

	const auto PlaybackActive = CMain::Shared::GetFunctionSafe("PlaybackActive");

	//a playback is currently active, so don't overwrite the cmds
	if (PlaybackActive && PlaybackActive->As<bool>()->Call())
		return;
#endif

	if (ps->pm_type == PM_NORMAL) {
#if(DEBUG_SUPPORT)
		//shouldn't be hardcoded to this module, unless in debug mode
		CL_FixedTime(cmd, oldcmd);
#endif

		CJ_Strafebot(cmd, oldcmd);
		CJ_AutoFPS(cmd);
		CJ_AutoRPG(ps, cmd, oldcmd);
		CJ_Bhop(ps, cmd, oldcmd);
		CJ_Prediction(ps, cmd, oldcmd);
	}

	return;
}
