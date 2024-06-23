#pragma once

#include "r/gui/r_gui.hpp"

class CCodJumperWindow : public CGuiElement
{
public:
	CCodJumperWindow(const std::string& id);
	~CCodJumperWindow() = default;

	void* GetRender() override {
		union {
			void (CCodJumperWindow::* memberFunction)();
			void* functionPointer;
		} converter{};
		converter.memberFunction = &CCodJumperWindow::Render;
		return converter.functionPointer;
	}

	void Render() override;
	//const std::string& GetIdentifier() const override { return id; }

private:


	size_t m_uSelectedPlayback = {};
};
