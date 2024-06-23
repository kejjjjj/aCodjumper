#include "cj_fps.hpp"
#include "cl/cl_move.hpp"
#include "com/com_channel.hpp"
#include <cg/cg_init.hpp>
#include <dvar/dvar.hpp>
#include <net/nvar_table.hpp>
#include <r/r_drawtools.hpp>
#include <cg/cg_angles.hpp>
#include <cg/cg_local.hpp>
#include <cg/cg_offsets.hpp>
#include <cl/cl_utils.hpp>


std::unordered_map<std::int32_t, std::vector<CFpsZone>> CFPS::fps_zones;

#ifndef BE_SMART
//let's be real you never want to go above 333
int CFPS::GetHighestUsefulFPS(std::int32_t g_speed)
{

	std::int32_t FPS = 1000;
	std::int32_t ms = 1;

	do {
		float accel = (static_cast<float>(g_speed) / static_cast<float>(FPS));

		if (std::roundf(accel) != 0.f)
			break;

		ms++;
		FPS = 1000 / ms;

	} while (true);
	
	return FPS;
}
#endif
CFpsZone CFPS::GetHighestFpsZone(std::int32_t g_speed)
{
#ifdef BE_SMART
	constexpr int FPS = 333;
#else
	const int FPS = GetHighestUsefulFPS(g_speed);
#endif

	CFpsZone zone;

	zone.start = static_cast<int>(std::roundf(g_speed / (1000.f / FPS)));
	zone.length = AngleWrap90(-zone.start) * 2 - 1;

	//add the WD strafe to it
	zone.start = static_cast<int>(AngleWrap90(zone.start + RAD2DEG(0.78539f)));
	zone.end = zone.start + zone.length;
	zone.fps = FPS;



	return zone;

}
std::int32_t CFPS::GetIdealFPS(playerState_s* ps, [[maybe_unused]]usercmd_s* cmd)
{
	const auto opt_zone = GetZoneAsCopy(ps->speed);

	if (!opt_zone)
		return 333;

	const auto zone = &opt_zone.value();
	constexpr float relative_acceleration_angle = 0.78539f;
	const float acceleration_angle = (atan2f(-(float)cmd->rightmove, (float)cmd->forwardmove)) - relative_acceleration_angle;
	const std::int32_t direction = (cmd->rightmove == 127) ? -1 : 1;


	for (const auto& z : *zone) {
		const auto yaw90 = static_cast<int>(AngleWrap90((ps->viewangles[YAW] + RAD2DEG(acceleration_angle)) * direction));
		if (yaw90 >= z.start && yaw90 <= z.end)
			return z.fps;

	}

	//punish the user for getting to a state where fps for this angle is not present
	return 500;
}
std::vector<CFpsZone> CFPS::GetZones(std::int32_t g_speed)
{
	const CFpsZone highestFps = GetHighestFpsZone(g_speed);
	std::vector<std::int32_t> fps;


#ifndef BE_SMART
	const std::int32_t ms = std::clamp((1000 / highestFps.fps), 1, 100);

	for (auto i = ms; i < ms + 4; i++)
		fps.push_back(1000 / i);
	
#else
	fps.push_back(333);
	fps.push_back(250);
	fps.push_back(200);
	fps.push_back(125);
#endif

	std::vector<CFpsZone> zones(fps.size());
	zones[0] = highestFps;

	for (const auto i : std::views::iota(1u, fps.size())) {
		zones[i].start = zones[i - 1].end;
		zones[i].end = int(float(zones[i].start) * (float(1000.f / fps[i - 1]) / float(1000.f / fps[i])));
		zones[i].length = (zones[i].start - zones[i].end);
		zones[i].fps = fps[i];
	}

	//fix inverted zones
	for (const auto i : std::views::iota(1u, fps.size())) {
		zones[i].start = zones[i - 1].end;
		zones[i].end = zones[i].start + zones[i].length;

		if (zones[i].start >= 90) {
			zones[i].start -= 90;
		}
		if (zones[i].end > 90) {
			zones[i].end -= 90;
		}

	}
	

	//for (const auto& z : zones) {
	//	std::cout << std::format("{}: [{}, {}] -> {}\n", z.fps, z.start, z.end, z.length);
	//}

	return zones;
}
std::optional<std::reference_wrapper<std::vector<CFpsZone>>> CFPS::GetZone(std::int32_t fps) {

	using Type = decltype(GetZone(0));

	const auto ps = &cgs->predictedPlayerState;
	const auto itr = fps_zones.find(fps);
	if (itr == fps_zones.end()) {
		fps_zones[fps] = GetZones(ps->speed);

		if (fps_zones.empty()) {
			return std::nullopt;
		}

		return Type(std::ref(fps_zones[fps]));
	}

	return Type(std::ref(itr->second));
}
std::optional<std::vector<CFpsZone>> CFPS::GetZoneAsCopy(std::int32_t fps)
{
	const auto ps = &cgs->predictedPlayerState;
	const bool long125 = NVar_FindMalleableVar<bool>("AutoFPS")->GetChild("Long 125")->As<ImNVar<bool>>()->Get();

	using Ret = std::optional<std::vector<CFpsZone>>;
	
	const auto itr = fps_zones.find(fps);
	if (itr == fps_zones.end()) {
		fps_zones[fps] = GetZones(ps->speed);
		return Ret(fps_zones[fps]);
	}

	CFpsZone* z125{}, * z333{};
	std::vector<CFpsZone> copy = fps_zones[fps];

	for (auto& z : copy) {

		if (z.fps == 125)
			z125 = &z;
		else if (z.fps == 333)
			z333 = &z;
	}

	if (!z125 || !z333)
		return Ret(copy);

	if (long125)
		z333->start = z125->end;
	else
		z125->end = z333->start;

	return Ret(copy);
}
void CJ_AutoFPS(usercmd_s* cmd)
{
	if (!NVar_FindMalleableVar<bool>("AutoFPS")->Get())
		return;

	dvar_s* com_maxfps = Dvar_FindMalleableVar("com_maxfps");
	const bool spacebar333 = NVar_FindMalleableVar<bool>("AutoFPS")->GetChild("+gostand 333fps")->As<ImNVar<bool>>()->Get();

	if (spacebar333 && (cmd->buttons & cmdEnums::jump) != 0)
		com_maxfps->current.integer = 333;
	else if (cgs->predictedPlayerState.groundEntityNum == 1023)
		com_maxfps->current.integer = CFPS::GetIdealFPS(&cgs->predictedPlayerState, cmd);
	else
		com_maxfps->current.integer = 125;
}

static void CG_AutoFPS_TextHud(playerState_s* ps, usercmd_s* cmd)
{
	char buff[12];
	sprintf_s(buff, "%i", CFPS::GetIdealFPS(ps, cmd));
	const float x = R_GetTextCentered(buff, "fonts/smallDevFont", 320.f, 0.7f);
	R_AddCmdDrawTextWithEffects(buff, "fonts/smallDevFont", x, 260, 0.7f, 0.7f, 0.f, vec4_t{ 1,0,0,1 }, 3, vec4_t{ 1,1,1,0 }, nullptr, nullptr, 0, 0, 0, 0);

}
static void CG_AutoFPS_Zones(playerState_s* ps, usercmd_s* cmd)
{
	auto ozones = CFPS::GetZoneAsCopy(ps->speed);

	if (!ozones)
		return;

	const float fov = Dvar_FindMalleableVar("cg_fov")->current.value * Dvar_FindMalleableVar("cg_fovScale")->current.value;

	auto& zones = ozones.value();

	constexpr float relative_acceleration_angle = 0.78539f;
	const float acceleration_angle = atan2f(-(float)cmd->rightmove, (float)cmd->forwardmove) - relative_acceleration_angle;
	const std::int32_t multiplier = (cmd->rightmove == 127) ? -1 : 1;
	const float yaw = AngleNormalize180(ps->viewangles[YAW] + RAD2DEG(acceleration_angle));

	//add the transfer zone marker
	zones.emplace_back(CFpsZone{ 250, 86, 87, 1 });

	size_t i = 0u;
	float color = {};
	const float color_increment = 1.f / zones.size();
	for ([[maybe_unused]] const auto& zone : zones) {

		float r{}, g{}, b{};

		r = (i % 1 == 0) ? color : 0;
		g = (i % 2 == 0) ? color : 0;
		b = (i % 3 == 0) ? color : 0;

		for (float a = -180.f; a != 180.f; a += 90.f) {
			CG_FillAngleYaw(float(zone.start * multiplier) + a, float(zone.end * multiplier) + a, yaw, 240, 10, fov, vec4_t{ r,g,b, 0.7f });

		}
		color += color_increment;
		i++;
	}

}
void CG_AutoFPSHud(playerState_s* ps, usercmd_s* cmd)
{
	const auto hud = NVar_FindMalleableVar<bool>("AutoFPS")->GetChild("Draw HUD")->As<ImNVar<bool>>();
	if (!hud->Get() || ps->pm_type != PM_NORMAL)
		return;

	if (hud->GetChild("FPS")->As<ImNVar<bool>>()->Get())
		CG_AutoFPS_TextHud(ps, cmd);
	if (hud->GetChild("Zones")->As<ImNVar<bool>>()->Get())
		CG_AutoFPS_Zones(ps, cmd);



}
