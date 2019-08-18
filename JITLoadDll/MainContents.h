#pragma once
#include <imgui.h>
#include "imgui_memory_editor.h"

#include <vector>
#include <string>
#include <array>
#include <regex>

namespace MainContents {
	namespace data {
		// can use == since static
		static const char* calling_conventions[] = { "stdcall", "cdecl", "fastcall", };
		static const char* types[] = { "char", "unsigned char", "int16_t", "uint16_t",
			"int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double"};
	}

	struct State {
		class ParamState {
		public:
			typedef uint64_t PackedType;
			static const uint8_t valSize = sizeof(PackedType);

			// imgui writes into value, so must alloc
			std::array<char, valSize> value;
			const char* type = "";

			PackedType getPacked() const {
				return *(PackedType*)value.data();
			}
		};

		MemoryEditor m_memEditor;
		const char* cur_convention = data::calling_conventions[0];
		std::vector<ParamState> params;
	};

	static State state;

	static ImGuiDataType typeToImType(const char* type) {
		if (strcmp(type, "char") == 0) {
			return ImGuiDataType_S8;
		} else if (strcmp(type, "unsigned char") == 0) {
			return ImGuiDataType_S8;
		} else if (strcmp(type, "int16_t") == 0) {
			return ImGuiDataType_S16;
		} else if (strcmp(type, "uint16_t") == 0) {
			return ImGuiDataType_U16;
		} else if (strcmp(type, "int32_t") == 0) {
			return ImGuiDataType_S32;
		} else if (strcmp(type, "uint32_t") == 0) {
			return ImGuiDataType_U32;
		} else if (strcmp(type, "int64_t") == 0) {
			return ImGuiDataType_S64;
		} else if (strcmp(type, "uint64_t") == 0) {
			return ImGuiDataType_U64;
		} else if (strcmp(type, "float") == 0) {
			return ImGuiDataType_Float;
		} else if (strcmp(type, "double") == 0) {
			return ImGuiDataType_Double;
		}
		return -1;
	}

	static void Draw() {
		ImGui::Text("Select Calling Convention: ");
		ImGui::SameLine();

		if (ImGui::BeginCombo("##convention", state.cur_convention)) // The second parameter is the label previewed before opening the combo.
		{
			for (int n = 0; n < IM_ARRAYSIZE(data::calling_conventions); n++)
			{
				bool is_selected = (state.cur_convention == data::calling_conventions[n]);
				if (ImGui::Selectable(data::calling_conventions[n], is_selected))
					state.cur_convention = data::calling_conventions[n];
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button("Add Parameter"))
		{
			state.params.push_back(State::ParamState());
		}

		for (int i = 0; i < state.params.size(); i++) {		
			// each element in this loop must have unique name
			std::string name = std::to_string(i);

			// value input, convert dropdown string to imgui input field type
			ImGuiDataType type = typeToImType(state.params.at(i).type);
			if (type != -1) {
				ImGui::InputScalar((name + "val").c_str(), type, state.params.at(i).value.data());
				ImGui::SameLine();
				ImGui::Button(std::string((name + "Mem").c_str()).c_str());
				ImGui::SameLine();
			}

			// set combo string type on param
			if (ImGui::BeginCombo((name + "type").c_str(), state.params.at(i).type)) // The second parameter is the label previewed before opening the combo.
			{
				for (int n = 0; n < IM_ARRAYSIZE(data::types); n++)
				{
					bool is_selected = (state.params.at(i).type == data::types[n]);
					if (ImGui::Selectable(data::types[n], is_selected))
						state.params.at(i).type = data::types[n];
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		/*static unsigned char ram[0x1000] = { 0 };
		m_memEditor.DrawWindow("Edit Memory", ram, 0x1000);*/
	}
}