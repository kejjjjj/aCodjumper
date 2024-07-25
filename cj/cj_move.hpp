#pragma once

#include "cg/cg_local.hpp"

struct playerState_s;
struct usercmd_s;

void CJ_FixedTime(usercmd_s* cmd, usercmd_s* oldcmd);
void CJ_Strafebot(usercmd_s* cmd, usercmd_s* oldcmd);
bool CJ_AutoPara(playerState_s* ps, usercmd_s* cmd);
void CJ_Force250(playerState_s* ps, usercmd_s* cmd);
bool CJ_InTransferZone(const playerState_s* ps, usercmd_s* cmd);
void CJ_Bhop(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);
void CJ_Prediction(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);

void CJ_AutoSlide(const pmove_t* pm, const pml_t* pml, usercmd_s* cmd, std::int32_t serverTime);
void CJ_EdgeJump(const playerState_s* old_ps, const playerState_s* ps, usercmd_s* cmd);

[[maybe_unused]] bool CJ_ForceStrafeInFPS(playerState_s* ps, usercmd_s* cmd, const int FPS);