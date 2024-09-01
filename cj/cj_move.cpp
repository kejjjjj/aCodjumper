#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"
#include "cg/cg_angles.hpp"
#include "cg/cg_client.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cj_fps.hpp"
#include "cj_move.hpp"
#include "cl/cl_move.hpp"
#include "cl/cl_utils.hpp"
#include "com/com_channel.hpp"
#include "dvar/dvar.hpp"
#include "net/nvar_table.hpp"

#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"

#if(DEBUG_SUPPORT)
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
#else
#include "shared/sv_shared.hpp"
#endif

void CJ_PushPlayback([[maybe_unused]]const std::vector<playback_cmd>& cmds, [[maybe_unused]]bool debugRender)
{

#if(DEBUG_SUPPORT)
	CStaticMovementRecorder::PushPlayback(cmds,
		{
			.m_eJumpSlowdownEnable= slowdown_t::both,
			.m_bIgnorePitch = true,
			.m_bIgnoreWASD = true,
			.m_bSetComMaxfps = false,
			.m_bNoLag = true,
			.m_bRenderExpectationVsReality = debugRender
		}
	);
#else

	const auto func = CMain::Shared::GetFunctionSafe("AddPlaybackC");
	if (!func)
		return;

	func->As<void, const std::vector<playback_cmd>&, const CPlaybackSettings&>()->Call(
		cmds,
		{
			.m_eJumpSlowdownEnable = slowdown_t::both,
			.m_bIgnorePitch = true,
			.m_bIgnoreWASD = true,
			.m_bSetComMaxfps = false,
			.m_bNoLag = true,
			.m_bRenderExpectationVsReality = false
		}

	);
#endif

}
playback_cmd CJ_StateToPlayback(const playerState_s* ps, const usercmd_s& cmd, const usercmd_s& oldcmd)
{
	playback_cmd pcmd;
	pcmd.buttons = cmd.buttons;
	pcmd.forwardmove = cmd.forwardmove;
	pcmd.offhand = cmd.offHandIndex;
	pcmd.origin = ps->origin;
	pcmd.rightmove = cmd.rightmove;
	pcmd.velocity = ps->velocity;
	pcmd.weapon = cmd.weapon;

	pcmd.oldTime = oldcmd.serverTime;
	pcmd.serverTime = cmd.serverTime;

	pcmd.cmd_angles = cmd.angles;
	pcmd.delta_angles = ps->delta_angles;
	return pcmd;
}
void CJ_FixedTime(usercmd_s* cmd, usercmd_s* oldcmd)
{
	dvar_s* com_maxfps = Dvar_FindMalleableVar("com_maxfps");

	int SafeFPS = com_maxfps->current.integer == 0 ? 1000 : com_maxfps->current.integer;
	int Delta = cmd->serverTime - oldcmd->serverTime;

	if (Delta && (1000 / Delta) != SafeFPS) {
		cmd->serverTime = oldcmd->serverTime + (1000 / SafeFPS);
	}
}

float CJ_limit_turn_rate(float delta, float max_deg_per_second, float frametime) {
	float max_turn_this_frame = max_deg_per_second * frametime;
	if (std::abs(delta) <= max_turn_this_frame) {
		return delta;
	}
	return std::copysign(max_turn_this_frame, delta);
}

void CJ_Strafebot(usercmd_s* cmd, usercmd_s* oldcmd)
{
	if (!NVar_FindMalleableVar<bool>("Strafebot")->Get())
		return;

	std::optional<float> yaw;

	static int time_when_key_pressed = 0;
	static char most_recent_rightmove = 0;

	const auto ps = &cgs->predictedPlayerState;

	if (!CJ_AutoPara(ps, cmd))
		CJ_Force250(ps, cmd);


	const bool rightmove_was_pressed_this_frame = cmd->rightmove != NULL;

	const auto Strafebot = NVar_FindMalleableVar<bool>("Strafebot");

	const auto persistence = Strafebot->GetChild("Persistence ms")->As<ImNVar<int>>()->Get();
	const auto fullbeat_only = Strafebot->GetChild("Fullbeat only")->As<ImNVar<bool>>()->Get();
	const auto assist_yawspeed_cap = Strafebot->GetChild("Strafe assist")->As<ImNVar<float>>()->Get();
	const auto overstrafe_assist_yawspeed_cap = Strafebot->GetChild("Overstrafe assist")->As<ImNVar<float>>()->Get();

	//persistence
	if (rightmove_was_pressed_this_frame == false) {
		if (time_when_key_pressed + persistence > cmd->serverTime && most_recent_rightmove) {
			if (ps->groundEntityNum == 1023) {
				cmd->rightmove = most_recent_rightmove;
				cmd->forwardmove = 127;
			}
		}
	}

	if (fullbeat_only) {
		if (cmd->forwardmove != 127 || cmd->rightmove == 0)
			return;
	}

	if ((yaw = CG_GetOptYawDelta(ps, cmd, oldcmd)) == std::nullopt) {
		return;
	}


	if (rightmove_was_pressed_this_frame) {
		time_when_key_pressed = cmd->serverTime;
		most_recent_rightmove = cmd->rightmove;
	}

	auto delta = *yaw;


	// surely xkej will refactor this into separate smaller functions later because now this looks very messy but i am very lazy

	constexpr auto same_sign = [](char a, float b) { return (a >= 0 && b >= 0) || (a < 0 && b < 0); };

	const auto frametime = (cmd->serverTime - oldcmd->serverTime) / 1000.f;
	const auto user_yaw_delta_this_frame = AngleDelta(SHORT2ANGLE(cmd->angles[YAW]), SHORT2ANGLE(oldcmd->angles[YAW]));
	auto delta_from_user_yaw = delta - user_yaw_delta_this_frame;
	const bool is_overstrafing = same_sign(cmd->rightmove, delta_from_user_yaw);

	// todo: figure out either: a good value for this, a good way to set this dynamically, or just make it an option 
	constexpr auto MAX_OVERSTRAFE_ACTIVATION_ANGLE = 5.f;
	
	// todo: limit overstrafe assist to airmove only?
	if (is_overstrafing && overstrafe_assist_yawspeed_cap > 0.f && fabs(delta_from_user_yaw) <= MAX_OVERSTRAFE_ACTIVATION_ANGLE) {
		delta_from_user_yaw = CJ_limit_turn_rate(delta_from_user_yaw, overstrafe_assist_yawspeed_cap, frametime);
	} else if (assist_yawspeed_cap > 0.f) {
		delta_from_user_yaw = CJ_limit_turn_rate(delta_from_user_yaw, assist_yawspeed_cap, frametime);
	}

	if (overstrafe_assist_yawspeed_cap == 0.f && assist_yawspeed_cap == 0.f)
	{
		CL_SetPlayerYaw(cmd, ps->delta_angles, ps->viewangles[YAW] + delta);
	}
	else {
		clients->viewangles[YAW] += delta_from_user_yaw;
		cmd->angles[YAW] = ANGLE2SHORT(clients->viewangles[YAW]);
	}
}

bool CJ_AutoPara(playerState_s* ps, usercmd_s* cmd)
{
	const auto autoPara = NVar_FindMalleableVar<bool>("Strafebot")->GetChildAs<ImNVar<bool>>("Auto Para");

	if (!autoPara->Get())
		return false;

	const bool has_bounced = CG_HasBounced(ps);
	const bool valid_pre_bounce = autoPara->GetChildAs<ImNVar<bool>>("Before Bounce")->Get() && !has_bounced;
	const bool valid_post_bounce = autoPara->GetChildAs<ImNVar<bool>>("After Bounce")->Get() && has_bounced;

	if (!valid_pre_bounce && !valid_post_bounce)
		return false;

	return CJ_ForceStrafeInFPS(ps, cmd, 333);
}
void CJ_Force250(playerState_s* ps, usercmd_s* cmd)
{
	const auto force250 = NVar_FindMalleableVar<bool>("Strafebot")->GetChildAs<ImNVar<bool>>("Force 250fps");

	if (!force250->Get())
		return;

	const bool has_bounced = CG_HasBounced(ps);
	const bool valid_pre_bounce = force250->GetChildAs<ImNVar<bool>>("Before Bounce")->Get() && !has_bounced;
	const bool valid_post_bounce = force250->GetChildAs<ImNVar<bool>>("After Bounce")->Get() && has_bounced;

	if (!valid_pre_bounce && !valid_post_bounce)
		return;

	static char _rightmove = 0;


	if (cmd->forwardmove == NULL || ps->groundEntityNum != 1023) {
		_rightmove = 0;
		return;
	}

	auto opt_zones = CFPS::GetZone(ps->speed);
	if (!opt_zones)
		return;

	auto& zones = opt_zones.value().get();
	CFpsZone* zonetarget = {};

	for (auto& z : zones) {
		if (z.fps == 250) {
			zonetarget = &z;
			break;
		}
	}
	if (!zonetarget)
		return;


	if (!_rightmove) {
		_rightmove = cmd->rightmove;
	}
	else if (_rightmove) {
		cmd->rightmove = _rightmove;
	}

	const auto opt_delta_a = CG_GetOptYawDelta(ps, cmd, CL_GetUserCmd(clients->cmdNumber - 1));

	if (!opt_delta_a)
		return;

	const float tmp = ps->viewangles[YAW];
	ps->viewangles[YAW] += opt_delta_a.value();

	bool valid_zone = !CJ_InTransferZone(ps, cmd);
	auto FPS = CFPS::GetIdealFPS(ps, cmd);
	ps->viewangles[YAW] = tmp;

	//test if we can still continue in this direction
	if ((FPS == 250 || FPS == 333)  && valid_zone)
		return;

	//test other direction
	cmd->rightmove *= -1;
	const auto opt_delta_b = CG_GetOptYawDelta(ps, cmd, CL_GetUserCmd(clients->cmdNumber - 1));

	if (!opt_delta_b) {
		_rightmove = 0;
		return;
	}

	ps->viewangles[YAW] += opt_delta_b.value();
	valid_zone = !CJ_InTransferZone(ps, cmd);

	cmd->rightmove *= -1;

	FPS = CFPS::GetIdealFPS(ps, cmd);
	ps->viewangles[YAW] = tmp;

	if ((FPS == 250 || FPS == 333) && valid_zone) {
		_rightmove *= -1;
		return;
	}
	else {
		_rightmove = 0;
		return;
	}

	return;
}
bool CJ_ForceStrafeInFPS(playerState_s* ps, usercmd_s* cmd, const int target_fps)
{
	static char _rightmove = 0;


	if (cmd->forwardmove == NULL || ps->groundEntityNum != 1023) {
		_rightmove = 0;
		return false;
	}

	auto opt_zones = CFPS::GetZone(ps->speed);
	if (!opt_zones)
		return false;

	auto& zones = opt_zones.value().get();
	CFpsZone* zonetarget = {};

	for (auto& z : zones) {
		if (z.fps == target_fps) {
			zonetarget = &z;
			break;
		}
	}
	if (!zonetarget)
		return false;

	if (!_rightmove) {
		_rightmove = cmd->rightmove;
	}
	else if (_rightmove) {
		cmd->rightmove = _rightmove;
	}

	const auto opt_delta_a = CG_GetOptYawDelta(ps, cmd, CL_GetUserCmd(clients->cmdNumber - 1));
	cmd->rightmove *= -1;
	const auto opt_delta_b = CG_GetOptYawDelta(ps, cmd, CL_GetUserCmd(clients->cmdNumber - 1));
	cmd->rightmove *= -1;

	if (!opt_delta_a || !opt_delta_b)
		return false;

	if (std::fabsf(opt_delta_b.value() - opt_delta_a.value()) > zonetarget->length) {
		_rightmove = 0;
		return false;
	}

	const float tmp = ps->viewangles[YAW];
	ps->viewangles[YAW] += opt_delta_a.value();

	auto FPS = CFPS::GetIdealFPS(ps, cmd);
	ps->viewangles[YAW] = tmp;

	//test if we can still continue in this direction
	if (FPS == target_fps)
		return true;

	//if not, change direction
	cmd->rightmove *= -1;
	ps->viewangles[YAW] += opt_delta_b.value();
	FPS = CFPS::GetIdealFPS(ps, cmd);

	//restore
	ps->viewangles[YAW] = tmp;
	cmd->rightmove *= -1;

	if (FPS == target_fps) {
		_rightmove *= -1;
	}
	else {
		_rightmove = 0;
		return false;
	}


	cmd->rightmove = _rightmove;
	return true;
}
bool CJ_InTransferZone(const playerState_s* ps, usercmd_s* cmd)
{

	constexpr float tested_aa = RAD2DEG(-0.78539f);
	const float aa = RAD2DEG(atan2f(-(float)cmd->rightmove, (float)cmd->forwardmove)) - tested_aa;
	const int multiplier = (cmd->rightmove == 127) ? -1 : 1;
	int yaw90 = int(AngleWrap90((ps->viewangles[YAW] + aa) * multiplier));
	return yaw90 >= 86;

}

bool CJ_Bhop([[maybe_unused]]const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd)
{
	if (ps->groundEntityNum == 1023 || !NVar_FindMalleableVar<bool>("Bhop")->Get())
		return false;

	if (playersKb[KB_GOSTAND].active && (cmd->buttons & cmdEnums::jump) == 0)
		cmd->buttons |= cmdEnums::jump;

	if ((cmd->buttons & cmdEnums::jump) != 0) {
		if ((cmd->buttons & cmdEnums::jump) != 0 && (oldcmd->buttons & cmdEnums::jump) != 0) {
			cmd->buttons &= ~(cmdEnums::crouch | cmdEnums::crouch_hold);
			cmd->buttons &= ~cmdEnums::jump;
		}
	}

	return false;
}
bool CJ_Prediction(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd)
{

	if ((ps->pm_flags & PMF_MANTLE) != 0 || (ps->pm_flags & PMF_LADDER) != 0)
		return false;

	const auto autoSlide = NVar_FindMalleableVar<bool>("Auto Slide")->Get();
	const auto edgeJump = NVar_FindMalleableVar<bool>("Edge Jump")->Get();

	//don't calculate for no reason
	if (!autoSlide && !edgeJump)
		return false;

	const auto bGrounded = CG_IsOnGround(ps);

	if (!bGrounded && autoSlide && CJ_AutoSlide(ps, cmd, oldcmd))
		return true;
	
	if(bGrounded && edgeJump)
		CJ_EdgeJump(ps, cmd, cmd);

	return false;

}
bool CJ_AutoSlide(const playerState_s* _ps, usercmd_s* cmd, const usercmd_s* oldcmd)
{
	const auto frameTime = (cmd->serverTime - oldcmd->serverTime);

	playerState_s ps_local = *_ps;
	auto pm = PM_Create(&ps_local, cmd, cmd);
	const playerState_s* ps = pm.ps;

	CPmoveSimulation sim(&pm);
	sim.FPS = 1000 / (frameTime == 0 ? 3 : frameTime);
	sim.Simulate();

	if (!CG_IsOnGround(pm.ps))
		return false;

	ps_local = *_ps;
	pm = PM_Create(&ps_local, cmd, cmd);
	ps = pm.ps;
	const auto fps = NVar_FindMalleableVar<bool>("Auto Slide")->GetChild("FPS")->As<ImNVar<int>>()->Get();
	sim.FPS = std::clamp(fps, 1, 1000);
	sim.Simulate();

	if (CG_IsOnGround(pm.ps) && sim.GetPML()->impactSpeed != 0.f)
		return false;

	const auto firstCmd = CJ_StateToPlayback(pm.ps, pm.cmd, pm.oldcmd);
	pm.oldcmd.serverTime = pm.cmd.serverTime;
	pm.cmd.serverTime += frameTime;
	const auto secondCmd = CJ_StateToPlayback(pm.ps, pm.cmd, pm.oldcmd);
	CJ_PushPlayback({ firstCmd, secondCmd });
	return true;

}
void CJ_EdgeJump(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd)
{
	if ((cmd->buttons & cmdEnums::jump) != 0)
		return;

	playerState_s ps_local = *ps;
	auto pm = PM_Create(&ps_local, cmd, oldcmd);
	PM_Weapon_Idle(pm.ps);

	usercmd_s ccmd = *cmd;
	ccmd.buttons &= ~cmdEnums::jump;
	CPmoveSimulation sim(&pm);
	sim.FPS = Dvar_FindMalleableVar("com_maxfps")->current.integer;
	sim.Simulate(&ccmd, oldcmd);

	//current frame is on the ground and the next frame isn't
	if (ps->groundEntityNum == 1022 && pm.ps->groundEntityNum == 1023) {
		cmd->buttons &= ~(cmdEnums::crouch | cmdEnums::crouch_hold);
		cmd->buttons |= cmdEnums::jump;
	}

}
//
//static bool CJ_PlayerWillBeOnTheGroundAfterFrame(const playerState_s* _ps, const usercmd_s* cmd, const std::int32_t fps)
//{
//	if (CG_IsOnGround(_ps))
//		return true;
//
//	playerState_s ps_local = *_ps;
//	auto pm = PM_Create(&ps_local, cmd, cmd);
//	CPmoveSimulation sim(&pm);
//
//	sim.FPS = 333;
//	sim.Simulate();
//
//	memcpy(&pm.oldcmd, &pm.cmd, sizeof(usercmd_s));
//
//	sim.FPS = fps == 0 ? 1000 : fps;
//	sim.Simulate();
//
//	return pm.ps->velocity[Z] >= 0.f && !sim.GetPML()->walking;
//}
//

//static bool CJ_WillPlayerSlideOnTheSurface(const playerState_s* _ps, const usercmd_s* cmd, std::int32_t fps)
//{
//
//	playerState_s ps_local = *_ps;
//	auto pm = PM_Create(&ps_local, cmd, cmd);
//	const auto ps = pm.ps;
//
//	CPmoveSimulation sim(&pm);
//	sim.FPS = fps == 0 ? 1000 : fps;
//	sim.Simulate();
//	const auto pml = sim.GetPML();
//
//	if (ps->velocity[Z] >= 0.f && !pml->walking) {
//
//		const auto newVelocity = fvec2(pm.ps->velocity).mag();
//		const auto oldVelocity = fvec2(_ps->velocity).mag();
//
//		Com_Printf("velDelta: %.6f\n", (newVelocity / oldVelocity) * 100.f);
//		return true;
//	}
//
//	return false;
//}
