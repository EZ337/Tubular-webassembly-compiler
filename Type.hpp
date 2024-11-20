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

  struct Info_Base {
    virtual ~Info_Base() { }

    virtual bool IsChar() const { return false; }
    virtual bool IsInt() const { return false; }
    virtual bool IsDouble() const { return false; }
    bool IsBase() const { return !(IsChar() || IsInt() || IsDouble()); }

    // Specialty types
    bool IsNumeric() const { return IsChar() || IsInt() || IsDouble(); }
    virtual bool IsString() const { return false; }

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
  bool IsBase() const { return info_ptr && Info().IsBase(); }
  bool IsNumeric() const { return info_ptr && Info().IsNumeric(); }

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


///////////////////////////////////////
//  Full function implementations


// Create a POD type from a string.
Type::Type(std::string type_name) {
  if (type_name == "char") info_ptr = std::make_unique<Info_Char>();
  else if (type_name == "int") info_ptr = std::make_unique<Info_Int>();
  else if (type_name == "double") info_ptr = std::make_unique<Info_Double>();
  else {
    std::cerr << "Internal ERROR: Unknown Type '" << type_name << "'." << std::endl;
    assert(false);
  }
}
