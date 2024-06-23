#pragma once

struct playerState_s;
struct usercmd_s;

void CJ_FixedTime(usercmd_s* cmd, usercmd_s* oldcmd);
void CJ_Strafebot(usercmd_s* cmd, usercmd_s* oldcmd);
bool CJ_AutoPara(playerState_s* ps, usercmd_s* cmd);
void CJ_Force250(playerState_s* ps, usercmd_s* cmd);
bool CJ_InTransferZone(playerState_s* ps, usercmd_s* cmd);
void CJ_Bhop(playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd);
[[maybe_unused]] bool CJ_ForceStrafeInFPS(playerState_s* ps, usercmd_s* cmd, const int FPS);
