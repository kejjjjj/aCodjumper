#include "cg_init.hpp"
#include "cg_hooks.hpp"

#include "utils/engine.hpp"
#include <net/nvar_table.hpp>

#include "r/r_drawactive.hpp"
#include <cmd/cmd.hpp>

#include "shared/sv_shared.hpp"
#include <net/im_defaults.hpp>
#include <thread>
#include <cod4x/cod4x.hpp>

#include <r/gui/r_codjumper.hpp>

using namespace std::chrono_literals;

#define RETURN(type, data, value) \
*reinterpret_cast<type*>((DWORD)data - sizeof(FARPROC)) = value;\
return 0;

void NVar_CreateVars(NVarTable * table)
{
    ImNVar<bool>* Strafebot = table->AddImNvar<bool, ImCheckbox>("Strafebot", false, bool_to_string);
    {
        Strafebot->AddImChild<int, ImDragInt>("Persistence (ms)", 250, arithmetic_to_string<int>, 0, 2000);
        Strafebot->AddImChild<bool, ImCheckbox>("Fullbeat only", true, bool_to_string);

        ImNVar<bool>* AutoPara = Strafebot->AddImChild<bool, ImCheckbox>("Auto Para", true, bool_to_string);
        {
            AutoPara->AddImChild<bool, ImCheckbox>("Before Bounce", true, bool_to_string);
            AutoPara->AddImChild<bool, ImCheckbox>("After Bounce", true, bool_to_string);
        }

        ImNVar<bool>* Force250 = Strafebot->AddImChild<bool, ImCheckbox>("Force 250fps", false, bool_to_string);
        {
            Force250->AddImChild<bool, ImCheckbox>("Before Bounce", true, bool_to_string);
            Force250->AddImChild<bool, ImCheckbox>("After Bounce", true, bool_to_string);
        }

    }

    ImNVar<bool>* autofps = table->AddImNvar<bool, ImCheckbox>("AutoFPS", false, bool_to_string);
    {
        autofps->AddImChild<bool, ImCheckbox>("Long 125", true, bool_to_string);

        ImNVar<bool>* draw_hud = autofps->AddImChild<bool, ImCheckbox>("Draw HUD", false, bool_to_string);
        {
            draw_hud->AddImChild<bool, ImCheckbox>("FPS", false, bool_to_string);
            draw_hud->AddImChild<bool, ImCheckbox>("Zones", false, bool_to_string);
        }

        autofps->AddImChild<bool, ImCheckbox>("+gostand 333fps", false, bool_to_string);

    }

    table->AddImNvar<bool, ImCheckbox>("RPG Lookdown", false, bool_to_string);
    table->AddImNvar<bool, ImCheckbox>("Bhop", false, bool_to_string);

}

#if(DEBUG_SUPPORT)
#include "cmd/cmd.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include <r/gui/r_main_gui.hpp>



void CG_Init()
{
    COD4X::initialize();
    CG_CreatePermaHooks();
    


    NVarTables::tables[NVAR_TABLE_NAME] = std::make_unique<NVarTable>(NVAR_TABLE_NAME);
    auto table = NVarTables::Get();
    
    NVar_CreateVars(table);

    if (table->SaveFileExists())
        table->ReadNVarsFromFile();

    table->WriteNVarsToFile();

    Cmd_AddCommand("gui", CStaticMainGui::Toggle);
    CStaticMainGui::AddItem(std::make_unique<CCodJumperWindow>(NVAR_TABLE_NAME));


}

#else
#include <cl/cl_move.hpp>
void CG_Init()
{
    while (!CMain::Shared::AddFunction || !CMain::Shared::GetFunction) {
        std::this_thread::sleep_for(200ms);
    }
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
    //CMain::Shared::GetFunctionOrExit("Queue_R_EndScene")->As<void, endscene_t&&>()->Call(R_EndScene);


    CG_CreatePermaHooks();

}
#endif
void CG_Cleanup()
{
    CG_ReleaseHooks();
}

#if(!DEBUG_SUPPORT)

using PE_EXPORT = std::unordered_map<std::string, DWORD>;

#include <nlohmann/json.hpp>
#include <shared/sv_shared.hpp>
#include <utils/hook.hpp>
#include <utils/errors.hpp>

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

    ImGui::SetCurrentContext(shared::GetFunctionOrExit("GetContext")->As<ImGuiContext*>()->Call());
}

#endif