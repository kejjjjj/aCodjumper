#include "cj_move.hpp"
#include "cj_fps.hpp"
#include <cg/cg_angles.hpp>
#include <cg/cg_client.hpp>
#include <net/nvar_table.hpp>
#include <cl/cl_move.hpp>
#include <com/com_channel.hpp>
#include "dvar/dvar.hpp"
#include <bg/bg_pmove.hpp>
#include <cg/cg_local.hpp>
#include <cg/cg_offsets.hpp>
#include <cl/cl_utils.hpp>

void CJ_FixedTime(usercmd_s* cmd, usercmd_s* oldcmd)
{
	dvar_s* com_maxfps = Dvar_FindMalleableVar("com_maxfps");

	int SafeFPS = com_maxfps->current.integer == 0 ? 1000 : com_maxfps->current.integer;
	int Delta = cmd->serverTime - oldcmd->serverTime;

	if (Delta && (1000 / Delta) != SafeFPS) {
		cmd->serverTime = oldcmd->serverTime + (1000 / SafeFPS);
	}
}
void CJ_Strafebot(usercmd_s* cmd, usercmd_s* oldcmd)
{
	if (!NVar_FindMalleableVar<bool>("Strafebot")->Get())
		return;

	std::optional<float> yaw;

	static int time_when_key_pressed = 0;
	static char most_recent_rightmove = 0;

	const auto ps = &cgs->predictedPlayerState;

	if(!CJ_AutoPara(ps, cmd))
		CJ_Force250(ps, cmd);

	const bool rightmove_was_pressed_this_frame = cmd->rightmove != NULL;
	const auto persistence = NVar_FindMalleableVar<bool>("Strafebot")->GetChild("Persistence (ms)")->As<ImNVar<int>>()->Get();
	const auto fullbeat_only = NVar_FindMalleableVar<bool>("Strafebot")->GetChild("Fullbeat only")->As<ImNVar<bool>>()->Get();

	if (fullbeat_only) {
		if (cmd->forwardmove != 127 || cmd->rightmove == 0)
			return;
	}

	//persistence
	if (rightmove_was_pressed_this_frame == false) {
		if (time_when_key_pressed + persistence > cmd->serverTime && most_recent_rightmove) {
			if (ps->groundEntityNum == 1023) {
				cmd->rightmove = most_recent_rightmove;
				cmd->forwardmove = 127;
			}
		}
	}


	if ((yaw = CG_GetOptYawDelta(ps, cmd, oldcmd)) == std::nullopt) {
		return;
	}


	if (rightmove_was_pressed_this_frame) {
		time_when_key_pressed = cmd->serverTime;
		most_recent_rightmove = cmd->rightmove;
	}
	const float delta = yaw.value();

	//if (std::fabsf(delta) > 1.f)
	//	delta = CG_SmoothAngle(0.f, delta, 0.1f);

	CL_SetPlayerYaw(cmd, ps->delta_angles, ps->viewangles[YAW] + delta);
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
bool CJ_InTransferZone(playerState_s* ps, usercmd_s* cmd)
{

	constexpr float tested_aa = RAD2DEG(-0.78539f);
	const float aa = RAD2DEG(atan2f(-(float)cmd->rightmove, (float)cmd->forwardmove)) - tested_aa;
	const int multiplier = (cmd->rightmove == 127) ? -1 : 1;
	int yaw90 = int(AngleWrap90((ps->viewangles[YAW] + aa) * multiplier));
	return yaw90 >= 86;

}

void CJ_Bhop(playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd)
{
	if (ps->groundEntityNum == 1023)
		return;

	if (NVar_FindMalleableVar<bool>("Bhop")->Get() && (cmd->buttons & cmdEnums::jump) != 0) {
		if ((cmd->buttons & cmdEnums::jump) != 0 && (oldcmd->buttons & cmdEnums::jump) != 0)
			cmd->buttons -= cmdEnums::jump;
	}

}
