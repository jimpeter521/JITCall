#pragma once
#include <imgui.h>
#include "imgui_memory_editor.h"
#include "PEB.hpp"

#include <vector>
#include <string>
#include <array>
#include <regex>

namespace FunctionEditor {
	static std::vector<std::string> getExports(uint64_t moduleBase) {
		assert(moduleBase != NULL);
		if (moduleBase == NULL)
			return std::vector<std::string>();

		IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)moduleBase;
		IMAGE_NT_HEADERS* pNT = RVA2VA(IMAGE_NT_HEADERS*, moduleBase, pDos->e_lfanew);
		IMAGE_DATA_DIRECTORY* pDataDir = (IMAGE_DATA_DIRECTORY*)pNT->OptionalHeader.DataDirectory;

		if (pDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == NULL) {
			return std::vector<std::string>();
		}

		IMAGE_EXPORT_DIRECTORY* pExports = RVA2VA(IMAGE_EXPORT_DIRECTORY*, moduleBase, pDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

		uint32_t* pAddressOfFunctions = RVA2VA(uint32_t*, moduleBase, pExports->AddressOfFunctions);
		uint32_t* pAddressOfNames = RVA2VA(uint32_t*, moduleBase, pExports->AddressOfNames);
		uint16_t* pAddressOfNameOrdinals = RVA2VA(uint16_t*, moduleBase, pExports->AddressOfNameOrdinals);

		std::vector<std::string> exports;
		exports.reserve(pExports->NumberOfNames);
		for (uint32_t i = 0; i < pExports->NumberOfNames; i++)
		{
			char* exportName = RVA2VA(char*, moduleBase, pAddressOfNames[i]);
			exports.push_back(exportName);
		}
		return exports;
	}

	namespace data {
		// can use == since static
		static const char* calling_conventions[] = { "stdcall", "cdecl", "fastcall", };
		static const char* types[] = { "void", "char", "unsigned char", "int16_t", "uint16_t",
			"int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double", 
			"char*"
		};
		static const char* DEFAULT_TYPE = "";
		static const char* EMPTY_EXPORT_NAME = "";
	}

	struct State {
		State() {
			dllExports = std::vector<std::string>();
			memset(exportName.data(), 0, exportName.size());

			const char* defName = "Select an export...";
			memcpy(exportName.data(), defName, strlen(defName));
		}

		State(uint64_t dllBase) {
			dllExports = getExports(dllBase);
			memset(exportName.data(), 0, exportName.size());

			const char* defName = "Select an export...";
			memcpy(exportName.data(), defName, strlen(defName));
		}

		class ParamState {
		public:
			typedef uint64_t PackedType;
			static const uint8_t valSize = sizeof(PackedType);

			// imgui writes into value
			std::array<char, valSize> value;
			const char* type = data::DEFAULT_TYPE;
			bool showMem = false;

			// for params that need backing memory (all ptr types)
			std::shared_ptr<char> m_ptrParamMemory;
			uint32_t m_ptrParamMemorySize;

			// Reinterpret data as packed type
			PackedType getPacked() const {
				return *(PackedType*)value.data();
			}

			bool isPtrType() {
				return m_ptrParamMemory != nullptr;
			}
		};

		bool isValidEndState() {
			if (returnType == data::DEFAULT_TYPE || exportName.empty()) {
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
		
		// given a ptr to a param get backing memory as if it was a ptr type, alloc if not already done
		std::shared_ptr<char> getPtrSlot(uint32_t paramIdx, uint32_t size) {
			auto& param = params.at(paramIdx);
			if (!param.m_ptrParamMemory) {
				param.m_ptrParamMemorySize = size;
				param.m_ptrParamMemory.reset(new char[size]);
				memset(param.m_ptrParamMemory.get(), 0, size);
			}
			return param.m_ptrParamMemory;
		}

		static const uint8_t ExportMaxLen = 20;

		const char* returnType = data::DEFAULT_TYPE;
		MemoryEditor m_memEditor;
		const char* cur_convention = data::calling_conventions[0];
		std::vector<ParamState> params; 
		std::vector<std::string> dllExports;

		// imgui writes into value
		std::array<char, ExportMaxLen> exportName;
		bool finished = false;
		bool insertBreakpoint = false;
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
		ImGui::Text("Close window to call exports in order");
		if (ImGui::BeginCombo("##export", state.exportName.data())) {
			for (size_t i = 0; i < state.dllExports.size(); i++) {
				std::string cur_export = state.dllExports[i];

				bool is_selected = strcmp(state.exportName.data(), cur_export.c_str()) == 0;
				if (ImGui::Selectable(cur_export.c_str(), is_selected)) {
					memset(state.exportName.data(), 0, state.exportName.size());
					memcpy(state.exportName.data(), cur_export.c_str(), cur_export.length());
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (ImGui::BeginCombo("##convention", state.cur_convention)) // The second parameter is the label previewed before opening the combo.
		{
			for (size_t n = 0; n < (size_t)IM_ARRAYSIZE(data::calling_conventions); n++)
			{
				bool is_selected = (state.cur_convention == data::calling_conventions[n]);
				if (ImGui::Selectable(data::calling_conventions[n], is_selected))
					state.cur_convention = data::calling_conventions[n];
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::Text("Calling convention");
		
		// set combo string type on return value type
		if (ImGui::BeginCombo("Return type", state.returnType)) 
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

		ImGui::Checkbox("Break on execution", &state.insertBreakpoint);

		if (ImGui::Button("Add Parameter"))
		{
			state.params.emplace_back();
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove Parameter"))
		{
			state.params.pop_back();
		}
		ImGui::SameLine();
		if (ImGui::Button("Finish Editing Function"))
		{
			if (!state.isValidEndState()) {
				ImGui::OpenPopup("TypeDefError");
			}else {
				// Main Window thread checks for this
				state.finished = true;
			}
		}
		
		for (size_t i = 0; i < state.params.size(); i++) {		
			// each element in this loop must have unique name
			std::string name = std::to_string(i);
			auto& param = state.params.at(i);

			// value input, convert dropdown string to imgui input field type (for primitives)
			ImGuiDataType type = typeToImType(param.type);
			if (type != -1) {
				ImGui::InputScalar(("param " + name).c_str(), type, param.value.data());
				ImGui::SameLine();
				if (ImGui::Button("Raw Mem")) {
					param.showMem = true;
				}

				// check if should draw editor, then if it's not open (been closed), reset flag
				if (param.showMem && !state.m_memEditor.DrawWindow("Edit Memory", param.value.data(), State::ParamState::valSize)) {
					param.showMem = false;
				}
				ImGui::SameLine();
			} else { 
				// this is non-primitive input types, like char* and other ptrs
				if (strcmp(param.type, "char*") == 0) {
					// get backing memory
					uint16_t bufSize = 256;
					std::shared_ptr<char> ptrMem = state.getPtrSlot(i, bufSize);

					// set slot to address of backing memory
					uint64_t ptrVal = 0;
					ptrVal = (uint64_t)ptrMem.get();
					memcpy(param.value.data(), (char*)&ptrVal, sizeof(ptrVal));
					ImGui::InputTextMultiline(("param " + name).c_str(), ptrMem.get(), bufSize);
				}
			}
			  
			// set combo string type on param
			if (ImGui::BeginCombo(("Param " + name + " type").c_str(), param.type))
			{
				for (int n = 0; n < IM_ARRAYSIZE(data::types); n++)
				{
					// void not allowed as parameter
					if (strcmp(data::types[n], "void") == 0)
						continue;

					bool is_selected = (param.type == data::types[n]);
					if (ImGui::Selectable(data::types[n], is_selected))
						param.type = data::types[n];
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		if (ImGui::BeginPopup("TypeDefError"))
		{
			ImGui::TextColored(ImColor::HSV(356.09f, 68.05f, 66.27f), "%s", "Invalid State, Fill in all values");
			ImGui::EndPopup();
		}
	}
}