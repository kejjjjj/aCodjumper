#include "main.hpp"
#include "net/nvar_table.hpp"
#include "r/gui/r_main_gui.hpp"
#include "r_codjumper.hpp"
#include "shared/sv_shared.hpp"

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

	GUI_RenderNVars();

}

