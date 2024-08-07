#include "main.hpp"
#include "r_codjumper.hpp"
#include "net/nvar_table.hpp"
#include <r/gui/r_main_gui.hpp>
#include <iostream>
#include <shared/sv_shared.hpp>


CCodJumperWindow::CCodJumperWindow(const std::string& name)
	: CGuiElement(name) {

}

void CCodJumperWindow::Render()
{

#if(!DEBUG_SUPPORT)
	static auto func = CMain::Shared::GetFunctionSafe("GetContext");

	if (!func) {
		func = CMain::Shared::GetFunctionSafe("GetContext");
		return;
	}

	ImGui::SetCurrentContext(func->As<ImGuiContext*>()->Call());

#endif

	const auto table = NVarTables::Get(NVAR_TABLE_NAME);
	if (!table)
		return;

	for (const auto& nvar : table->GetSorted()) {

		if (nvar && nvar->IsImNVar()) {
			std::string _v_ = nvar->GetName() + "_menu";
			ImGui::BeginChild(_v_.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
			nvar->RenderImNVar();
			ImGui::EndChild();
		}

	}


}

