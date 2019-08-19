#pragma once
#include <imgui.h>
#include "imgui_memory_editor.h"

#include <vector>
#include <string>
#include <array>
#include <regex>

namespace FunctionEditor {
	namespace data {
		// can use == since static
		static const char* calling_conventions[] = { "stdcall", "cdecl", "fastcall", };
		static const char* types[] = { "char", "unsigned char", "int16_t", "uint16_t",
			"int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double"};
		static const char* DEFAULT_TYPE = "";
	}

	struct State {
		class ParamState {
		public:
			typedef uint64_t PackedType;
			static const uint8_t valSize = sizeof(PackedType);

			// imgui writes into value
			std::array<char, valSize> value;
			const char* type = data::DEFAULT_TYPE;
			bool showMem = false;

			// Reinterpret data as packed type
			PackedType getPacked() const {
				return *(PackedType*)value.data();
			}
		};

		bool isValidEndState() {
			if (returnType == data::DEFAULT_TYPE) {
				return false;
			}

			for (const ParamState& p : params) {
				if (p.type == data::DEFAULT_TYPE) {
					return false;
				}
			}

			// all other states are valid
			return true;
		}

		const char* returnType = data::DEFAULT_TYPE;
		MemoryEditor m_memEditor;
		const char* cur_convention = data::calling_conventions[0];
		std::vector<ParamState> params;

		bool finished = false;
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

		// set combo string type on return value type
		if (ImGui::BeginCombo("return type", state.returnType)) 
		{
			for (int n = 0; n < IM_ARRAYSIZE(data::types); n++)
			{
				bool is_selected = (state.returnType == data::types[n]);
				if (ImGui::Selectable(data::types[n], is_selected))
					state.returnType = data::types[n];
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
				if (ImGui::Button(std::string((name + "Mem").c_str()).c_str())) {
					state.params.at(i).showMem = true;
				}

				// check if should draw editor, then if it's not open (been closed), reset flag
				if (state.params.at(i).showMem && !state.m_memEditor.DrawWindow("Edit Memory", state.params.at(i).value.data(), State::ParamState::valSize)) {
					state.params.at(i).showMem = false;
				}
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

		// Window thread checks for this
		if (ImGui::Button("Close Function Editor"))
		{
			if (!state.isValidEndState()) {
				ImGui::OpenPopup("TypeDefError");
			} else {
				state.finished = true;
			}
		}

		if (ImGui::BeginPopup("TypeDefError"))
		{
			ImGui::TextColored(ImColor::HSV(356.09f, 68.05f, 66.27f), "%s", "Invalid State, Fill in all values");
			ImGui::EndPopup();
		}
	}
}