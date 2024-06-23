#pragma once

#include <unordered_map>
#include <optional>

#define BE_SMART

struct CFpsZone
{
	std::int32_t fps = {};
	std::int32_t start = {};
	std::int32_t end = {};
	std::int32_t length = {};
};

struct playerState_s;
struct usercmd_s;

class CFPS
{
public:
	static std::int32_t GetIdealFPS(playerState_s* ps, usercmd_s* cmd);
	static std::optional<std::reference_wrapper<std::vector<CFpsZone>>> GetZone(std::int32_t fps);
	static std::optional<std::vector<CFpsZone>> GetZoneAsCopy(std::int32_t fps);

private:
	static std::vector<CFpsZone> GetZones(std::int32_t g_speed);
	static CFpsZone GetHighestFpsZone(std::int32_t g_speed);
	
#ifdef BE_SMART
	constexpr static inline int GetHighestUsefulFPS() { return 333; }
#else
	static int GetHighestUsefulFPS(std::int32_t g_speed);
#endif
	// key = g_speed, value = a vector of all fps zones
	static std::unordered_map<std::int32_t, std::vector<CFpsZone>> fps_zones;

};

void CJ_AutoFPS(usercmd_s* cmd);
void CG_AutoFPSHud(playerState_s* ps, usercmd_s* cmd);
