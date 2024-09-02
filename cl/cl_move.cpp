#include "utils/hook.hpp"

#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cl/cl_utils.hpp"
#include "cl_move.hpp"

#include "cj/cj_move.hpp"
#include "cj/cj_fps.hpp"
#include "cj/cj_rpg.hpp"

#if(DEBUG_SUPPORT)
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"
#else
#include "shared/sv_shared.hpp"
#endif

void CL_CreateNewCommands([[maybe_unused]] int localClientNum)
{
	if (CL_ConnectionState() != CA_ACTIVE)
		return;

	const auto ps = &cgs->predictedPlayerState;
	auto cmd = &clients->cmds[clients->cmdNumber & 0x7F];
	auto oldcmd = &clients->cmds[(clients->cmdNumber - 1) & 0x7F];

	#if(!DEBUG_SUPPORT)

		const auto PlaybackActive = CMain::Shared::GetFunctionSafe("PlaybackActive");
		const auto ElebotActive = CMain::Shared::GetFunctionSafe("ElebotActive");

		//a playback is currently active, so don't overwrite the cmds
		if (PlaybackActive && PlaybackActive->As<bool>()->Call() || ElebotActive && ElebotActive->As<bool>()->Call())
			return;

	#else

		if (CStaticMovementRecorder::GetActivePlayback())
			return CStaticMovementRecorder::Instance->Update(ps, cmd, oldcmd);

	#endif

	if (ps->pm_type != PM_NORMAL)
		return;


	#if(DEBUG_SUPPORT)
		
		//shouldn't be hardcoded to this module, unless in debug mode
		CL_FixedTime(cmd, oldcmd);
	#endif

	if (CJ_Prediction(ps, cmd, oldcmd)) {
			
	} else if (CJ_Bhop(ps, cmd, oldcmd)) {

	}
	else {
		CJ_Strafebot(cmd, oldcmd);
		CJ_AutoFPS(cmd);
		CJ_AutoRPG(ps, cmd, oldcmd);
	}

	#if(DEBUG_SUPPORT)
		return CStaticMovementRecorder::Instance->Update(ps, cmd, oldcmd);
	#endif
}
