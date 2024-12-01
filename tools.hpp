#pragma once

#include <iostream>
#include <string>
#include <sstream>

#include "lexer.hpp"

// Helper to convert everything passed to it into a single, concatenated string.
template <typename... Ts>
std::string ToString(Ts... message) {
  std::stringstream ss;
  (ss << ... << std::forward<Ts>(message));
  return ss.str();
}

// A position in a file specified by line and column.
struct FilePos {
  size_t line;
  size_t col;

  FilePos(size_t line, size_t col) : line(line), col(col) { }
  FilePos(const emplex::Token & token) : FilePos(token.line_id, token.col_id) { }
  FilePos(const FilePos &) = default;
  FilePos & operator=(const FilePos &) = default;

  std::string ToString() const { return ::ToString(line, ':', col); }
  auto operator<=>(const FilePos &) const = default;
};

// Helper function that take a line number and any number of additional args
// that it uses to write an error message and terminate the program.
template <typename... Ts>
void Error(FilePos file_pos, Ts... message) {
  std::cerr << "ERROR (at " << file_pos.line << ":" << file_pos.col <<  "): ";
  (std::cerr << ... << std::forward<Ts>(message)) << std::endl;
  exit(1);
}

template <typename... Ts>
void Error(Ts... message) {
  std::cerr << "ERROR: ";
  (std::cerr << ... << std::forward<Ts>(message)) << std::endl;
  exit(1);
}

template <auto... VALS, typename T>
bool IsOneOf(T value) {
  return ((value == VALS) || ...);
}

// Replace all instances in a string of one substring with another.
void ReplaceAll(std::string & str, std::string from, std::string to) {
  size_t pos = 0;
  while ((pos = str.find(from, pos)) != std::string::npos) {
    str.replace(pos, from.size(), to);
    pos += to.size(); // Move past the last replacement
  }
}
