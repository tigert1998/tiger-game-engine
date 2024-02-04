#include "utils.h"

#include <glad/glad.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cuchar>
#include <fstream>
#include <vector>

std::string SnakeToPascal(const std::string &name) {
  std::vector<std::string> parts;
  int last = 0;
  for (int i = 0; i < name.size(); i++) {
    if (name[i] == '_') {
      parts.push_back(name.substr(last, i - last));
      last = i + 1;
    }
  }
  if (last < name.size()) {
    parts.push_back(name.substr(last, name.size() - last));
  }
  std::string ans = "";
  for (int i = 0; i < parts.size(); i++) {
    if (parts[i].size() == 0) continue;
    ans.push_back(toupper(parts[i][0]));
    for (int j = 1; j < parts[i].size(); j++) {
      ans.push_back(tolower(parts[i][j]));
    }
  }
  return ans;
}

std::string ReadFile(const std::filesystem::path &file_path, bool binary) {
  if (binary) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string buffer;
    buffer.resize(size);
    file.read(buffer.data(), size);
    return buffer;
  } else {
    std::ifstream ifs(file_path);
    ifs.seekg(0, std::ios::end);
    size_t size = ifs.tellg();
    std::string content(size, ' ');
    ifs.seekg(0);
    ifs.read(content.data(), size);
    return content;
  }
}

std::string ToLower(const std::string &str) {
  std::string ans;
  for (int i = 0; i < str.size(); i++) ans.push_back(std::tolower(str[i]));
  return ans;
}

std::u8string ToU8string(const aiString &str) {
  return std::u8string(str.C_Str(), str.C_Str() + str.length);
}

std::string StringToHex(const std::string &input) {
  static const char hex_digits[] = "0123456789ABCDEF";

  std::string output;
  output.reserve(input.length() * 4);
  for (unsigned char c : input) {
    output += "\\x";
    output.push_back(hex_digits[c >> 4]);
    output.push_back(hex_digits[c & 15]);
  }
  return output;
}

void ImGuiListBox(const std::string &label, int *current_item,
                  const std::vector<std::string> &items) {
  ImGui::ListBox(
      label.c_str(), current_item,
      [](void *vec, int idx, const char **out_text) {
        std::vector<std::string> *vector =
            reinterpret_cast<std::vector<std::string> *>(vec);
        if (idx < 0 || idx >= vector->size()) return false;
        *out_text = vector->at(idx).c_str();
        return true;
      },
      (void *)(&items), items.size());
}