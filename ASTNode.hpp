#pragma once

#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Control.hpp"
#include "lexer.hpp"
#include "tools.hpp"         // For FilePos
#include "SymbolTable.hpp"

class ASTNode {
protected:
  FilePos file_pos;   // What file position was this node parsed from in the original file?

public:
  using ptr_t = std::unique_ptr<ASTNode>;

  ASTNode() : file_pos(0,0) { }
  ASTNode(FilePos file_pos) : file_pos(file_pos) { }
  ASTNode(const ASTNode &) = default;
  ASTNode(ASTNode &&) = default;
  virtual ~ASTNode() { }
  ASTNode & operator=(const ASTNode &) = default;
  ASTNode & operator=(ASTNode &&) = default;

  // What position in the original file was this node defined at?
  FilePos GetFilePos() const { return file_pos; }

  virtual void AddChild(ptr_t &&) {
    // Cannot call AddChild on a non-parent class.
    assert(false);
  }

  virtual std::string GetTypeName() const = 0;
  virtual void Print(std::string prefix="") const {
    std::cout << prefix << GetTypeName() << std::endl;
  }

  virtual Type ReturnType(const SymbolTable & /* symbols */) const {
    return Type();  // By default, return an empty type.
  }

  virtual void TypeCheck(const SymbolTable & /* symbols */) { }

  // Generate any GLOBAL code that is needed to initialize this node.
  // (For example, place literal strings in memory.)
  virtual void InitializeWAT(Control & /* control */) { }

  // Generate WAT code and return (true/false) whether a value was left on the stack.
  virtual bool ToWAT(Control & /* control */) = 0;
};

class ASTNode_Parent : public ASTNode {
private:
  std::vector< ptr_t > children{};

public:
  template <typename... NODE_Ts>
  ASTNode_Parent(FilePos file_pos, NODE_Ts &&... nodes) : ASTNode(file_pos) {
    (AddChild(std::move(nodes)), ...);
  }

  void TypeCheck(const SymbolTable & symbols) override {
    TypeCheckChildren(symbols);
  }

  // Tools to work with child nodes...

  void TypeCheckChildren(const SymbolTable & symbols) {
    for (auto & child : children) { child->TypeCheck(symbols); }
  }

  void InitializeWAT(Control & control) override {
    for (auto & child : children) { child->InitializeWAT(control); }
  }

  size_t NumChildren() const { return children.size(); }
  bool HasChild(size_t id) const { return id < children.size() && children[id]; }

  ASTNode & GetChild(size_t id) { assert(HasChild(id)); return *children[id]; }
  const ASTNode & GetChild(size_t id) const { assert(HasChild(id)); return *children[id]; }
  ASTNode & LastChild() { assert(children.size()); return *children.back(); }
  const ASTNode & LastChild() const { assert(children.size()); return *children.back(); }

  void AddChild(ptr_t && child) override {
    children.push_back(std::move(child));
  }

  template <typename NODE_T, typename... ARG_Ts>
  void MakeChild(ARG_Ts &&... args) {
    AddChild( std::make_unique<NODE_T>(std::forward<ARG_Ts>(args)...) );
  }

  // Insert a new node between this one and a specified child.
  template <typename NODE_T>
  void AdaptChild(size_t id) {
    assert(id < children.size()); // Make sure child is there to adapt.
    children[id] = std::make_unique<NODE_T>(std::move(children[id]));
  }

  // Generate WAT code for a specified child.
  // Make sure there is an 'out_value' if needed; otherwise drop any out value.
  void ChildToWAT(size_t id, Control & control, bool out_needed) { 
    assert(HasChild(id));
    const bool has_out = children[id]->ToWAT(control);
    assert(!out_needed || has_out);  // If we need an out value, make sure one is provided.
    if (!out_needed && has_out) {    // If we don't need an out value and one is provided, drop it.
      control.Drop();
    }
  }

  void Print(std::string prefix="") const override {
    PrintChildren(prefix);
  }

  void PrintChildren(std::string prefix="") const {
    std::cout << prefix << GetTypeName() << std::endl;
    for (size_t i = 0; i < NumChildren(); ++i) {
      GetChild(i).Print(prefix + "  ");
    }
  }
};

class ASTNode_Block : public ASTNode_Parent {
public:
  template <typename... NODE_Ts>
  ASTNode_Block(FilePos file_pos, NODE_Ts &&... nodes) : ASTNode_Parent(file_pos, nodes...) { }

  std::string GetTypeName() const override { return "BLOCK"; }

  void AddChild(ptr_t && child) override {
    // TO ADD: Make sure another child is allowed?
    ASTNode_Parent::AddChild(std::move(child));
  }

  bool ToWAT(Control & control) override { 
    for (size_t i = 0; i < NumChildren(); ++i) {
      bool leftover_value = GetChild(i).ToWAT(control); // Run children.
      if (leftover_value) {
        // If child statement left an unneeded value on the stack, remove it.
        control.Drop();
      }
    }

    return false; // Value is left on the stack only if this is a return statement.
  }
};
