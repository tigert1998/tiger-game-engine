#include "utils.h"

#include <glad/glad.h>
#include <glog/logging.h>

#include <algorithm>
#include <cctype>
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