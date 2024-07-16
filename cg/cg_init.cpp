#include "cg/cg_cleanup.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cg_hooks.hpp"
#include "cg_init.hpp"
#include "cmd/cmd.hpp"
#include "cod4x/cod4x.hpp"
#include "net/im_defaults.hpp"
#include "net/nvar_table.hpp"
#include "r/gui/r_codjumper.hpp"
#include "r/r_drawactive.hpp"
#include "shared/sv_shared.hpp"
#include "sys/sys_thread.hpp"
#include "utils/engine.hpp"
#include "utils/hook.hpp"
#include <thread>

using namespace std::chrono_literals;

#define RETURN(type, data, value) \
*reinterpret_cast<type*>((DWORD)data - sizeof(FARPROC)) = value;\
return 0;

void NVar_CreateVars(NVarTable * table)
{
    ImNVar<bool>* Strafebot = table->AddImNvar<bool, ImCheckbox>("Strafebot", false, NVar_ArithmeticToString<bool>);
    {
        Strafebot->AddImChild<int, ImDragInt>("Persistence ms", 250, NVar_ArithmeticToString<int>, 0, 2000);
        Strafebot->AddImChild<bool, ImCheckbox>("Fullbeat only", true, NVar_ArithmeticToString<bool>);

        ImNVar<bool>* AutoPara = Strafebot->AddImChild<bool, ImCheckbox>("Auto Para", true, NVar_ArithmeticToString<bool>);
        {
            AutoPara->AddImChild<bool, ImCheckbox>("Before Bounce", true, NVar_ArithmeticToString<bool>);
            AutoPara->AddImChild<bool, ImCheckbox>("After Bounce", true, NVar_ArithmeticToString<bool>);
        }

        ImNVar<bool>* Force250 = Strafebot->AddImChild<bool, ImCheckbox>("Force 250fps", false, NVar_ArithmeticToString<bool>);
        {
            Force250->AddImChild<bool, ImCheckbox>("Before Bounce", true, NVar_ArithmeticToString<bool>);
            Force250->AddImChild<bool, ImCheckbox>("After Bounce", true, NVar_ArithmeticToString<bool>);
        }

    }

    ImNVar<bool>* autofps = table->AddImNvar<bool, ImCheckbox>("AutoFPS", false, NVar_ArithmeticToString<bool>);
    {
        autofps->AddImChild<bool, ImCheckbox>("Long 125", true, NVar_ArithmeticToString<bool>);

        ImNVar<bool>* draw_hud = autofps->AddImChild<bool, ImCheckbox>("Draw HUD", false, NVar_ArithmeticToString<bool>);
        {
            draw_hud->AddImChild<bool, ImCheckbox>("FPS", false, NVar_ArithmeticToString<bool>);
            draw_hud->AddImChild<bool, ImCheckbox>("Zones", false, NVar_ArithmeticToString<bool>);
        }

        autofps->AddImChild<bool, ImCheckbox>("gostand 333fps", false, NVar_ArithmeticToString<bool>);

    }

    table->AddImNvar<bool, ImCheckbox>("RPG Lookdown", false, NVar_ArithmeticToString<bool>);
    table->AddImNvar<bool, ImCheckbox>("Bhop", false, NVar_ArithmeticToString<bool>);

}

#if(DEBUG_SUPPORT)
#include "cg/cg_memory.hpp"
#include "cmd/cmd.hpp"
#include "r/gui/r_main_gui.hpp"

void CG_Init()
{

    while (!dx || !dx->device)
        std::this_thread::sleep_for(100ms);

    Sys_SuspendAllThreads();
    std::this_thread::sleep_for(300ms);

    COD4X::initialize();
    
    if (!CStaticMainGui::Owner->Initialized()) {
#pragma warning(suppress : 6011)
        CStaticMainGui::Owner->Init(dx->device, FindWindow(NULL, COD4X::get() ? "Call of Duty 4 X" : "Call of Duty 4"));
    }


    NVarTables::tables[NVAR_TABLE_NAME] = std::make_unique<NVarTable>(NVAR_TABLE_NAME);
    auto table = NVarTables::Get();
    
    NVar_CreateVars(table);

    if (table->SaveFileExists())
        table->ReadNVarsFromFile();

    table->WriteNVarsToFile();

    Cmd_AddCommand("gui", CStaticMainGui::Toggle);
    CStaticMainGui::AddItem(std::make_unique<CCodJumperWindow>(NVAR_TABLE_NAME));

    CG_CreatePermaHooks();
    CG_MemoryTweaks();
    Sys_ResumeAllThreads();
}

#else
#include "cl/cl_move.hpp"
void CG_Init()
{
    while (!CMain::Shared::AddFunction || !CMain::Shared::GetFunction) {
        std::this_thread::sleep_for(200ms);
    }

    while (!dx || !dx->device)
        std::this_thread::sleep_for(100ms);

    Sys_SuspendAllThreads();
    std::this_thread::sleep_for(300ms);

    COD4X::initialize();
    NVarTables::tables = CMain::Shared::GetFunctionOrExit("GetNVarTables")->As<nvar_tables_t*>()->Call();
    (*NVarTables::tables)[NVAR_TABLE_NAME] = std::make_unique<NVarTable>(NVAR_TABLE_NAME);
    const auto table = (*NVarTables::tables)[NVAR_TABLE_NAME].get();

    NVar_CreateVars(table);

    if (table->SaveFileExists())
        table->ReadNVarsFromFile();

    table->WriteNVarsToFile();

    CMain::Shared::GetFunctionOrExit("AddItem")->As<CGuiElement*, std::unique_ptr<CGuiElement>&&>()
        ->Call(std::make_unique<CCodJumperWindow>(NVAR_TABLE_NAME));

    //add the functions that need to be managed by the main module
    CMain::Shared::GetFunctionOrExit("Queue_CG_DrawActive")->As<void, drawactive_t>()->Call(CG_DrawActive);
    CMain::Shared::GetFunctionOrExit("Queue_CL_FinishMove")->As<void, finishmove_t>()->Call(CL_FinishMove);
    CMain::Shared::GetFunctionOrExit("Queue_CG_Cleanup")->As<void, cg_cleanup_t>()->Call(CG_Cleanup);
    //CMain::Shared::GetFunctionOrExit("Queue_R_EndScene")->As<void, endscene_t&&>()->Call(R_EndScene);


    CG_CreatePermaHooks();

    Sys_ResumeAllThreads();

}
#endif

void CG_Cleanup()
{
#if(DEBUG_SUPPORT)
    hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#endif

    CG_SafeExit();
}

#if(!DEBUG_SUPPORT)

using PE_EXPORT = std::unordered_map<std::string, DWORD>;

#include <nlohmann/json.hpp>
#include <shared/sv_shared.hpp>
#include <utils/hook.hpp>
#include <utils/errors.hpp>
#include <iostream>

using json = nlohmann::json;

static PE_EXPORT deserialize_data(const std::string& data)
{
    json j = json::parse(data);
    std::unordered_map<std::string, DWORD> map;
    for (auto it = j.begin(); it != j.end(); ++it) {
        map[it.key()] = it.value();
    }
    return map;

}

dll_export void L(void* data) {

    auto r = deserialize_data(reinterpret_cast<char*>(data));

    try {
        CMain::Shared::AddFunction = (decltype(CMain::Shared::AddFunction))r.at("public: static void __cdecl CSharedModuleData::AddFunction(class std::unique_ptr<class CSharedFunctionBase,struct std::default_delete<class CSharedFunctionBase> > &&)");
        CMain::Shared::GetFunction = (decltype(CMain::Shared::GetFunction))r.at("public: static class CSharedFunctionBase * __cdecl CSharedModuleData::GetFunction(class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const &)");
    }
    catch ([[maybe_unused]]std::out_of_range& ex) {
        return FatalError(std::format("couldn't get a critical function"));
    }
    using shared = CMain::Shared;

    //std::this_thread::sleep_for(500ms);

    ImGui::SetCurrentContext(shared::GetFunctionOrExit("GetContext")->As<ImGuiContext*>()->Call());
}

#endif