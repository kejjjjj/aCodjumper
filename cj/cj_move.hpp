#pragma once

//#include "cg/cg_local.hpp"

#include <vector>

struct playerState_s;
struct usercmd_s;
struct playback_cmd;


void CJ_PushPlayback(const std::vector<playback_cmd>& cmds, bool debugRender=false, bool noLag=true, bool com_maxfps=false);
[[nodiscard]] playback_cmd CJ_StateToPlayback(const playerState_s* ps, const usercmd_s& cmd, const usercmd_s& oldcmd);


void CJ_FixedTime(usercmd_s* cmd, usercmd_s* oldcmd);
void CJ_Strafebot(usercmd_s* cmd, usercmd_s* oldcmd);
bool CJ_AutoPara(playerState_s* ps, usercmd_s* cmd);
void CJ_Force250(playerState_s* ps, usercmd_s* cmd);

[[nodiscard]] bool CJ_InTransferZone(const playerState_s* ps, usercmd_s* cmd);
[[nodiscard]] bool CJ_Bhop(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd);

[[nodiscard]] bool CJ_Prediction(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);

[[nodiscard]] bool CJ_AutoSlide(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);
void CJ_EdgeJump(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);

[[nodiscard]] bool CJ_EasyBounces(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);

[[maybe_unused]] bool CJ_BounceFPS(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd);


[[maybe_unused]] bool CJ_ForceStrafeInFPS(playerState_s* ps, usercmd_s* cmd, const int FPS);

