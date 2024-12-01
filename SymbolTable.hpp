#pragma once

#include <assert.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "lexer.hpp"
#include "tools.hpp"
#include "Type.hpp"

class SymbolTable {
private:
  struct VarInfo {
    std::string name;     // Identifier for this variable.
    FilePos def_pos;      // Location in the file where variable was defined.
    Type type;            // Type of variable.
  };

  // Track all of the individual variables.
  std::vector< VarInfo > var_array{};

  // Track variable names in a scope to ids (positions) in var_array
  using scope_t = std::unordered_map<std::string, size_t>;

  // Keep a stack of active scopes as we process the file (start at global)
  std::vector< scope_t > scope_stack{1};

  // Track variables that were created inside of a function body.
  std::vector<size_t> function_vars; 

public:
  static constexpr size_t NO_ID = static_cast<size_t>(-1);

  // ----------- SCOPE MANAGEMENT ------------

  void PushScope() { scope_stack.emplace_back(); }
  void PopScope() {
    assert(scope_stack.size() > 1); // First level is global -- do not delete!
    scope_stack.pop_back();
  }

  // ----------- CONTENTS CHECKS ------------

  size_t NumVars() const { return var_array.size(); }

  // Test if a given variable ID exists in the symbol table.
  bool Has(size_t id) const { return id < var_array.size(); }

  // Test if a given identifier exists anywhere in the symbol table.
  bool Has(std::string name) {
    for (const scope_t & scope : scope_stack) {
      if (scope.count(name)) return true;
    }
    return false;
  }

  // ----------- VARIABLE ACCESS ------------

  VarInfo & At(size_t id) {
    assert(id < var_array.size());
    return var_array[id];
  }

  const VarInfo & At(size_t id) const {
    assert(id < var_array.size());
    return var_array[id];
  }

  // Scan through symbol table, in order, to find the correct variable.
  size_t GetVarID(std::string name) const {
    for (auto scope_it = scope_stack.rbegin(); scope_it < scope_stack.rend(); ++scope_it) {
      // Look for the variable in this scope; if we find it, print its ID.
      auto var_it = scope_it->find(name);
      if (var_it != scope_it->end()) return var_it->second;
    }
    assert(false); // Cannot find ID!
    return NO_ID;  // Not found in any scope!
  }

  std::string GetName(size_t id) const { return At(id).name; }


  // ----------- ADDING VARIABLES and FUNCTIONS  ------------

  // Add a variable with the provided identifier.
  size_t AddVar(emplex::Token type_token, emplex::Token id_token) {
    assert(id_token.id == emplex::Lexer::ID_ID);

    const std::string name = id_token.lexeme;

    scope_t & table = scope_stack.back();
    if (table.count(name)) {
      const auto & id = table[name];
      Error(id_token, "Redeclaration of variable '", name,
            "' (original declaration on line ", At(id).def_pos.line, ").");
    }
    const size_t id = var_array.size();
    Type type(type_token);
    var_array.emplace_back(name, id_token, type);
    table[name] = id;

    function_vars.push_back(id); // Store this variable's ID for this function.

    return id;
  }

  size_t AddFunction(
    emplex::Token id_token,
    const std::vector<Type> & param_types,
    Type return_type
  ) {
    assert(id_token.id == emplex::Lexer::ID_ID);

    const std::string name = id_token.lexeme;

    // Functions are always defined in the global scope.
    scope_t & table = scope_stack[0]; 
    if (table.count(name)) {
      const auto & id = table[name];
      Error(id_token, "Redeclaration of function '", name,
            "' (original declaration on line ", At(id).def_pos.line, ").");
    }
    const size_t id = var_array.size();
    var_array.emplace_back(name, id_token, Type(param_types, return_type));
    table[name] = id;

    return id;
  }

  /// @brief Creates an inbuilt function with pos 0,0
  /// @param func_name Name of the function to create
  /// @param param_types vector of the parameter types
  /// @param return_type return type of the function
  /// @return the id to the function
  size_t AddInbuiltFunction(const std::string& func_name, 
    const std::vector<Type>& param_types, Type return_type)
  {
    emplex::Token inbuilt_token{emplex::Lexer::ID_FUNCTION, func_name, 0, 0};

    // Functions are always defined in the global scope.
    scope_t & table = scope_stack[0];
    if (table.count(func_name)) {
      const auto & id = table[func_name];
      Error("Inbuilt function ", func_name, " already exists");
    }

    const size_t id = var_array.size();
    var_array.emplace_back(func_name, inbuilt_token, Type(param_types, return_type));
    table[func_name] = id;

    return id;
  }

  // ----------- TYPE MANAGEMENT ------------

  const Type & GetType(size_t id) const { return At(id).type; }

  // ----------- TRACKING OF VARIABLES IN A FUNCTION BODY -------------
  // Reset vars associated with a function (typically at the start of the next function)
  void ClearFunctionVars() { function_vars.resize(0); }

  const std::vector<size_t> & GetFunctionVars() const { return function_vars; }

  // ----------- DEBUGGING ------------

  void Print() const {
    std::cout << var_array.size() << " variables found:" << std::endl;
    for (const VarInfo & v : var_array) {
      std::cout << " '" << v.name << "' (type: " << v.type.Name()
                << "; defined: " << v.def_pos.ToString() << ")"
                << std::endl;
    }
  }
};
