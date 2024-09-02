#include "cj_rpg.hpp"
#include <cg/cg_local.hpp>
#include "cg/cg_offsets.hpp"
#include "bg/bg_weapon.hpp"
#include <cg/cg_angles.hpp>
#include <net/nvar_table.hpp>

#include "utils/typedefs.hpp"

void CJ_AutoRPG(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd)
{
	static float delta_per_frame = {};

	if (!NVar_FindMalleableVar<bool>("RPG Lookdown")->Get() || !ps->weaponDelay || ps->weaponstate != WEAPON_FIRING) {
		delta_per_frame = {};
		return;
	}

	if (ps->weapon != BG_FindWeaponIndexForName("rpg_mp") && ps->weapon != BG_FindWeaponIndexForName("rpg_sustain_mp"))
		return;

	const int deltaTime = cmd->serverTime - oldcmd->serverTime;

	//safeguard
	if (ps->weaponDelay <= (deltaTime * 2)) {
		cmd->angles[PITCH] = ANGLE2SHORT(AngleDelta(84.9999f, ps->delta_angles[PITCH]));
		return;
	}

	//const float interpolation = static_cast<float>(deltaTime) / ps->weaponDelay;


	//delta_per_frame = interpolation * AngularDistance(ps->viewangles[PITCH], 85);
	//cmd->angles[PITCH] = ANGLE2SHORT(AngleDelta(ps->viewangles[PITCH] + delta_per_frame, ps->delta_angles[PITCH]));
}
