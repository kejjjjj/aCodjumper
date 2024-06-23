#include "main.hpp"
#include "r_codjumper.hpp"
#include "net/nvar_table.hpp"
#include <r/gui/r_main_gui.hpp>


CCodJumperWindow::CCodJumperWindow(const std::string& name)
	: CGuiElement(name) {

}

void CCodJumperWindow::Render()
{
	auto table = NVarTables::Get(NVAR_TABLE_NAME);

	for (auto& v : *table) {

		auto& [key, value] = v;

		if (value->IsImNVar()) {
			std::string _v_ = key + "_menu";
			ImGui::BeginChild(_v_.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
			value->RenderImNVar();
			ImGui::EndChild();
		}

	}


}

