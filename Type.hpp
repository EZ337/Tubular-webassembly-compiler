#pragma once

#include <assert.h>
#include <memory>
#include <string>
#include <vector>

#include "lexer.hpp"
#include "tools.hpp"

class Type {
private:
  struct Info_Char;
  struct Info_Int;
  struct Info_Double;
  struct Info_String;
  struct Info_Function;

  struct Info_Base {
    virtual ~Info_Base() { }

    virtual bool IsChar() const { return false; }
    virtual bool IsInt() const { return false; }
    virtual bool IsDouble() const { return false; }
    virtual bool IsString() const { return false; }
    virtual bool IsFunction() const {return false; }
    bool IsBase() const { return !(IsChar() || IsInt() || IsDouble()) || 
      IsString() || IsFunction(); }

    // Specialty types
    bool IsNumeric() const { return IsChar() || IsInt() || IsDouble(); }
    bool IsAlpha() const {return IsChar() || IsString(); }
    bool IsIndexable() const {return IsString(); }

    virtual std::string Name() const { return "void"; }
    virtual std::string ToWAT() const { return "UNKNOWN_TYPE"; }

    virtual bool IsSame(const Info_Base & in) const { assert(IsBase()); return in.IsBase(); }
    virtual bool ConvertToOK(const Info_Base & in) const { assert(IsBase()); return in.IsBase(); }
    virtual bool CastToOK(const Info_Base &) const { assert(IsBase()); return false; } 

    bool ConvertFromOK(const Info_Base & in) const { return in.ConvertToOK(*this); }
    bool CastFromOK(const Info_Base & in) const { return in.CastToOK(*this); }

    virtual int BitCount() const { return 0; }

    virtual std::unique_ptr<Info_Base> Clone() const {
      return std::make_unique<Info_Base>();
    }
  };

  std::unique_ptr<Info_Base> info_ptr = nullptr;

  // Helper functions
  const Info_Base & Info() const { return *info_ptr; }
  static const Info_Function & FunInfo(const Info_Base & in);
  const Info_Function & FunInfo() const { return FunInfo(*info_ptr); }

public:
  Type() { }  // Void type.

  // Create a POD type from a string.
  Type(std::string type_name);

  // Create a POD type from a token.
  Type(emplex::Token type_token) : Type(type_token.lexeme) { }

  // Create a Function type
  Type(const std::vector<Type> & param_types, const Type & return_type);

  // Move Constructor
  Type (Type &&) = default;

  // Copy Constructor
  Type(const Type & in) {
    if (in.info_ptr) {
      info_ptr = in.Info().Clone();
    }
  }

  bool IsChar() const { return info_ptr && Info().IsChar(); }
  bool IsInt() const { return info_ptr && Info().IsInt(); }
  bool IsDouble() const { return info_ptr && Info().IsDouble(); }
  bool IsString() const { return info_ptr && Info().IsString(); }
  bool IsFunction() const {return info_ptr && Info().IsFunction(); }
  bool IsBase() const { return info_ptr && Info().IsBase(); }
  bool IsNumeric() const { return info_ptr && Info().IsNumeric(); }
  bool IsAlpha() const { return info_ptr && Info().IsAlpha(); }
  bool IsIndexable() const {return info_ptr && Info().IsIndexable(); }

  bool IsSame(const Type & in) const { return Info().IsSame(*in.info_ptr); }
  bool operator==(const Type & in) const { return IsSame(in); }

  // Can one type be implicitly converted to another?
  bool ConvertToOK(const Type & in) const { return Info().ConvertToOK(*in.info_ptr); }

  // Can one type be implicitly converted to another?
  bool ConvertFromOK(const Type & in) const { return Info().ConvertFromOK(*in.info_ptr); }

  // Can one type be cast to another?
  bool CastToOK(const Type & in) const { return Info().CastToOK(*in.info_ptr); }

  // Can one type be cast to another?
  bool CastFromOK(const Type & in) const { return Info().CastFromOK(*in.info_ptr); }

  std::string Name() const { assert(info_ptr); return Info().Name(); }
  std::string ToWAT() const { assert(info_ptr); return Info().ToWAT(); }

  int BitCount() const { assert(info_ptr); return Info().BitCount(); }

  // Calls that can only be run function types for more type info.
  size_t NumParams() const;
  const Type & ParamType(size_t id) const;
  const Type & ReturnType() const;
};

struct Type::Info_Char : Type::Info_Base {
  bool IsChar() const override { return true; }
  std::string Name() const override { return "char"; }
  std::string ToWAT() const override { return "i32"; }
  int BitCount() const override { return 22; }

  std::unique_ptr<Info_Base> Clone() const override {
    return std::make_unique<Info_Char>();
  }

  bool IsSame(const Info_Base & in) const override { return in.IsChar(); }

  bool ConvertToOK(const Info_Base & in) const override {
    return (in.IsChar() || in.IsInt() || in.IsDouble());
  }
  bool CastToOK(const Info_Base & in) const override {
    return (in.IsChar() || in.IsInt() || in.IsDouble() || in.IsString());
  }
};

struct Type::Info_Int : Type::Info_Base {
  bool IsInt() const override { return true; }
  std::string Name() const override { return "int"; }
  std::string ToWAT() const override { return "i32"; }
  int BitCount() const override { return 32; }

  std::unique_ptr<Info_Base> Clone() const override {
    return std::make_unique<Info_Int>();
  }

  bool IsSame(const Info_Base & in) const override { return in.IsInt(); }

  bool ConvertToOK(const Info_Base & in) const override {
    return (in.IsInt() || in.IsDouble());
  }
  bool CastToOK(const Info_Base & in) const override {
    return (in.IsChar() || in.IsInt() || in.IsDouble());
  }
};

struct Type::Info_Double : Type::Info_Base {
  bool IsDouble() const override { return true; }
  std::string Name() const override { return "double"; }
  std::string ToWAT() const override { return "f64"; }
  int BitCount() const override { return 64; }

  std::unique_ptr<Info_Base> Clone() const override {
    return std::make_unique<Info_Double>();
  }

  bool IsSame(const Info_Base & in) const override { return in.IsDouble(); }

  bool ConvertToOK(const Info_Base & in) const override {
    return in.IsDouble(); // Cannot freely convert to any other type.
  }
  bool CastToOK(const Info_Base & in) const override {
    return (in.IsChar() || in.IsInt() || in.IsDouble());
  }
};

struct Type::Info_String : Type::Info_Base {
  bool IsString() const override {return true; }
  std::string Name() const override { return "string"; }
  std::string ToWAT() const override {return "i32"; } // TODO: Might need more
  int BitCount() const override {return 99; } // Set high to trump int

  std::unique_ptr<Info_Base> Clone() const override {
    return std::make_unique<Info_String>();
  }

  bool IsSame(const Info_Base & in) const override { return in.IsString(); }

  bool ConvertToOK(const Info_Base & in) const override {
    return false; // Cannot implicitly convert to any other type
  }
  bool CastToOK(const Info_Base & in) const override {
    return (in.IsChar()); // TODO: EC support cast to int
  }
};


struct Type::Info_Function : Type::Info_Base {
  std::vector<Type> param_types;
  Type return_type;

  Info_Function(const std::vector<Type> & param_types, Type return_type)
    : param_types(param_types), return_type(return_type) { }
  bool IsFunction() const override { return true; }
  std::string Name() const override { return return_type.Name() + "(...)"; }
  std::unique_ptr<Info_Base> Clone() const override {
    return std::make_unique<Info_Function>(param_types, return_type);
  }

  bool IsSame(const Info_Base & in) const override {
    if (!in.IsFunction()) return false;
    const Info_Function & in_fun = FunInfo(in);
    if (param_types.size() != in_fun.param_types.size()) return false;
    for (size_t i = 0; i < param_types.size(); ++i) {
      if (param_types[i] != in_fun.param_types[i]) return false;
    }
    return return_type == in_fun.return_type;
  }
  bool ConvertToOK(const Info_Base & in) const override {
    return IsSame(in);
  }
  bool CastToOK(const Info_Base &) const override {
    return false; // There are no casts to functions.
  }
};

///////////////////////////////////////
//  Full function implementations

const Type::Info_Function & Type::FunInfo(const Info_Base & in) {
  assert(in.IsFunction()); // Ensure that this is a function type.
  return dynamic_cast<const Info_Function &>(in);
}

// Create a POD type from a string.
Type::Type(std::string type_name) {
  if (type_name == "char") info_ptr = std::make_unique<Info_Char>();
  else if (type_name == "int") info_ptr = std::make_unique<Info_Int>();
  else if (type_name == "double") info_ptr = std::make_unique<Info_Double>();
  else if (type_name == "string") info_ptr = std::make_unique<Info_String>();
  else {
    std::cerr << "Internal ERROR: Unknown Type '" << type_name << "'." << std::endl;
    assert(false);
  }
}

// Create a Function type
Type::Type(const std::vector<Type> & param_types, const Type & return_type) {
  info_ptr = std::make_unique<Info_Function>(param_types, return_type);
}

size_t Type::NumParams() const {
  return FunInfo().param_types.size();
}

const Type & Type::ParamType(size_t id) const {
  assert(id < NumParams());
  return FunInfo().param_types[id];
};

const Type & Type::ReturnType() const {
  return FunInfo().return_type;
}

