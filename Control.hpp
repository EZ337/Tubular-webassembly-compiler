#pragma once

#include <iostream>
#include <string>

#include "SymbolTable.hpp"

// A struct that contains all of the state information to control compilation.

struct Control {
  SymbolTable symbols{};
  int indent = 0;
  size_t wat_mem_pos = 0;   // Position for generating fixed data in WAT memory.

  std::vector<std::string> break_stack; // Stack of break labels for active scopes.
  std::vector<std::string> loop_stack;  // Stack of continue labels for active scopes.

  // Labels are made unique by adding a number to their end; track of what number we are up to!
  std::unordered_map<std::string, size_t> label_ids;

  struct WAT_Line {
    int indent;
    std::string code;
    std::string comment;
  };
  std::vector<WAT_Line> code;

public:  // Member functions.
  // Change the amount of indent used.
  Control &  Indent(int diff) {
    indent += diff;
    return *this;
  }

  template <typename... Ts>
  Control & Code(Ts &&... args) {
    std::stringstream ss;
    (ss << ... << std::forward<Ts>(args));
    code.emplace_back(indent, ss.str(), "");
    return *this;
  }

  // Add code for string data and return its memory position.
  size_t Data(std::string str) {
    Code("(data (i32.const ", wat_mem_pos ,") \"", str, "\\00\")");
    size_t out = wat_mem_pos;
    wat_mem_pos += str.size() + 1;
    return out;
  }

  // Drop the top value on the stack.
  // Either remove the last instruction (if no side effects) or add a "(drop)"
  Control & Drop() {
    if (code.back().code.starts_with("(local.get")) {
      code.pop_back();
    } else {
      code.emplace_back(indent, "(drop)", "Remove unneeded value from stack.");
    }
    return *this;
  }

  // Append a comment after the current line of code.
  template <typename... Ts>
  Control & Comment(Ts &&... args) {
    std::stringstream ss;
    (ss << ... << std::forward<Ts>(args));
    code.back().comment = ss.str();
    return *this;
  }

  // Special command for a whole-line comment that should indent with the code.
  template <typename... Ts>
  Control & CommentLine(Ts &&... args) {
    code.emplace_back(indent, "", "");
    return Comment(std::forward<Ts>(args)...);
  }

  void PrintCode(std::ostream & os=std::cout) const {
    // First, process code to identify the widest line with a comment.
    size_t max_width = 0;
    for (const auto & line : code) {
      if (line.comment.size() && line.code.size() > max_width) {
        max_width = line.code.size();
      }
    }

    // Print code, line by line.
    for (const auto & line : code) {
      os << std::string(line.indent, ' '); // Tabbing
      os << line.code;
      if (line.comment.size()) {
        if (line.code.size()) { // If there is code on this line, align comments.
          size_t gap = max_width - line.code.size() + 2;
          os << std::string(gap, ' ');
        }
        os << ";; " << line.comment;
      }
      os << std::endl;
    }
  }

  // Add a unique number to the end of any label base provided.
  // E.g., "loop" might become "loop13".
  std::string MakeLabel(std::string base) {
    size_t id = ++label_ids[base];
    return base + std::to_string(id);
  }

  // Get the most recent label made with a given name.
  std::string GetLastLabel(std::string base) const {
    auto it = label_ids.find(base);
    assert(it != label_ids.end());
    return base + std::to_string(it->second);
  }

  void PushBreakLabel(std::string label) { break_stack.push_back(label); }
  void PopBreakLabel() { break_stack.pop_back(); }
  bool HasBreakLabel() const { return break_stack.size(); }
  std::string GetBreakLabel() const {
    assert(HasBreakLabel());
    return break_stack.back();
  }

  void PushLoopLabel(std::string label) { loop_stack.push_back(label); }
  void PopLoopLabel() { loop_stack.pop_back(); }
  bool HasLoopLabel() const { return loop_stack.size(); }
  std::string GetLoopLabel() const {
    assert(HasLoopLabel());
    return loop_stack.back();
  }

  // ----------  Symbol Table Management --------------

  // Declare the set of variable ID's provided here.
  void WATDeclareSymbols(std::vector<size_t> var_ids) {
    // All local symbols must be declared at the beginning of the function.
    CommentLine("Variables");
    for (size_t i : var_ids) {
      Code("(local $var", i, " ", WATType(i), ")").Comment("Variable: ", symbols.GetName(i));
    }
    Code("");
  }

  std::string WATType(size_t var_id) const {
    return symbols.GetType(var_id).ToWAT();
  }
};
