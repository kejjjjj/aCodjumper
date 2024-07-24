//#include "cg_hooks.hpp"
#include "cg_init.hpp"

#include "cl/cl_move.hpp"
#include "utils/hook.hpp"

#include "r/r_drawactive.hpp"
#include "wnd/wnd.hpp"

#include "cod4x/cod4x.hpp"

#include <shared/sv_shared.hpp>

#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include <thread>
#include <chrono>
using namespace std::chrono_literals;

#if(DEBUG_SUPPORT)
#include "r/gui/r_gui.hpp"
#include "cg/cg_memory.hpp"
#endif

static void CG_CreateHooks();
void CG_CreatePermaHooks()
{
#if(DEBUG_SUPPORT)
	hooktable::initialize();
#else
	hooktable::initialize(CMain::Shared::GetFunctionOrExit("GetHookTable")->As<std::vector<phookbase>*>()->Call());
#endif

	CG_CreateHooks();
}

void CG_CreateHooks()
{

#if(DEBUG_SUPPORT)
	CG_CreateEssentialHooks();

	hooktable::preserver<void, usercmd_s*>(HOOK_PREFIX("CL_FinishMove"), 0x463A60u, CL_FinishMove);
	hooktable::preserver<void>(HOOK_PREFIX("CG_DrawActive"), COD4X::get() ? COD4X::get() + 0x5464 : 0x42F7F0, CG_DrawActive);

#endif

}
void CG_ReleaseHooks()
{

}