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

  // What position in the original file was this whole code segment defined at?
  virtual FilePos GetFirstPos() const { return file_pos; }

  virtual void AddChild(ptr_t &&) {
    // Cannot call AddChild on a non-parent class.
    assert(false);
  }

  virtual std::string GetTypeName() const = 0;
  virtual void Print(std::string prefix="") const {
    std::cout << prefix << GetTypeName() << std::endl;
  }

  // Does this node represent a guaranteed return? Options are:
  // - A return node.
  // - A block node ending in return (earlier guaranteed returns give an error)
  // - An if-else block where both conditions trigger a return.
  virtual bool IsReturn() const { return false; }

  // Does this node have the possibility of returning? Additional options are:
  // - An if-else block where one branch returns.
  // - A while loop with a return inside.
  virtual bool MayReturn() const { return false; }

  virtual Type ReturnType(const SymbolTable & /* symbols */) const {
    return Type();  // By default, return an empty type.
  }

  virtual void TypeCheck(const SymbolTable & /* symbols */) { }

  // Generate any GLOBAL code that is needed to initialize this node.
  // (For example, place literal strings in memory.)
  virtual void InitializeWAT(Control & /* control */) { }

  // Generate WAT code and return (true/false) whether a value was left on the stack.
  virtual bool ToWAT(Control & /* control */) = 0;

  virtual bool CanAssign() const { return false; }
  virtual void ToAssignWAT(Control & /* control */) {
    assert(false); // By default, nodes are not assignable!
  }
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

  FilePos GetFirstPos() const override {
    FilePos first_pos = file_pos;
    for (const auto & child : children) {
      if (child->GetFirstPos() < first_pos) first_pos = child->GetFirstPos();
    }
    return first_pos;
  }

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
private:
  bool is_return = false;
  bool may_return = false;

public:
  template <typename... NODE_Ts>
  ASTNode_Block(FilePos file_pos, NODE_Ts &&... nodes) : ASTNode_Parent(file_pos, nodes...) { }

  std::string GetTypeName() const override { return "BLOCK"; }

  void AddChild(ptr_t && child) override {
    // If this block already has a return, this code can't be reached.
    if (is_return) Error(child->GetFirstPos(), "Unreachable code.");
    // If the child we are adding is a return statement, this block ends in a return.
    if (child->IsReturn()) is_return = true;
    if (child->MayReturn()) may_return = true;
    ASTNode_Parent::AddChild(std::move(child));
  }

  bool IsReturn() const override { return is_return; }
  bool MayReturn() const override { return may_return; }
  Type ReturnType(const SymbolTable & symbols) const override {
    return LastChild().ReturnType(symbols);
  }

  bool ToWAT(Control & control) override { 
    bool is_final_node = control.FinalNode();
    control.FinalNode(false);
    for (size_t i = 0; i < NumChildren(); ++i) {
      // Only the final node in the block should be marked as such.
      if (i == NumChildren()-1) control.FinalNode(is_final_node);

      bool leftover_value = GetChild(i).ToWAT(control); // Run children.
      if (leftover_value) {
        // If child statement left an unneeded value on the stack, remove it.
        control.Drop();
      }
    }

    return false; // Value is left on the stack only if this is a return statement.
  }
};

class ASTNode_Function : public ASTNode_Parent {
protected:
  size_t fun_id;
  std::vector<size_t> param_ids;    // The set of variables used as function parameters.
  std::vector<size_t> var_ids;      // The set of variables used inside the function. 
public:
  ASTNode_Function(
    const emplex::Token & name_token,
    size_t fun_id,
    std::vector<size_t> param_ids,
    ptr_t && body
  ) : ASTNode_Parent(name_token, body)
    , fun_id(fun_id)
    , param_ids(param_ids) { }

  std::string GetTypeName() const override { return std::string("FUNCTION: ") + std::to_string(fun_id); }

  void AddVar(size_t var_id) { var_ids.push_back(var_id); }
  void SetVars(const std::vector<size_t> & in) { var_ids = in; }

  Type ReturnType(const SymbolTable & symbols) const override {
    return symbols.At(fun_id).type.ReturnType();
  }

  bool ToWAT(Control & control) override {
    assert(NumChildren() == 1);

    std::string param_declare;
    for (size_t id : param_ids) {
      std::string type = control.WATType(id);
      param_declare += ToString(" (param $var", id, " ", type, ")");
    }

    auto fun_name = control.symbols.At(fun_id).name;
    auto fun_type = control.symbols.At(fun_id).type;

    std::string wat_return = fun_type.ReturnType().ToWAT();
    control.Code("(func $", fun_name, param_declare, " (result ", wat_return, ")");
    control.Indent(2);
    control.WATDeclareSymbols(var_ids);
    control.FinalNode(true);     // Since there is only one node in this function, in must be the final one.
    ChildToWAT(0, control, false);
    control.Indent(-2);
    control.Code(")").Comment("END '", fun_name, "' function definition.")
           .Code("")  // Skip a line.
           .Code("(export \"", fun_name, "\" (func $", fun_name, "))")
           .Code("");  // Skip a line.

    return false;
  }
};


class ASTNode_If : public ASTNode_Parent {
public:
  ASTNode_If(FilePos file_pos, ptr_t && test, ptr_t && action)
    : ASTNode_Parent(file_pos, test, action) { }
  ASTNode_If(FilePos file_pos, ptr_t && test, ptr_t && action, ptr_t && alt_action)
    : ASTNode_Parent(file_pos, test, action, alt_action) { }

  std::string GetTypeName() const override { return "IF"; }

  bool IsReturn() const override {
    // If both then and else statements are returns, 'if' is guaranteed to return.
    return (NumChildren() == 3) && GetChild(1).IsReturn() && GetChild(2).IsReturn();
  }

  bool MayReturn() const override {
    // If either then or else statements may return, 'if' could return.
    if (GetChild(1).MayReturn()) return true;
    return (NumChildren() == 3) && GetChild(2).MayReturn();
  }

  Type ReturnType(const SymbolTable & symbols) const override {
    return GetChild(1).ReturnType(symbols);
  }

  void TypeCheck(const SymbolTable & symbols) override {
    if (NumChildren() < 2 || NumChildren() > 3) {
      Error(file_pos, "Internal error: Expected 2 or 3 children in if node, found ", NumChildren());
    }
    if (!GetChild(0).ReturnType(symbols).IsInt()) {
      Error(file_pos, "Condition for if-statement must evaluate to type int, not ",
            GetChild(0).ReturnType(symbols).Name());
    }
    TypeCheckChildren(symbols);
  }

  bool ToWAT(Control & control) override {
    control.CommentLine("Test condition for if.");
    ChildToWAT(0, control, true);
    std::string result_str;
    if (control.FinalNode()) {
      std::string type = ReturnType(control.symbols).ToWAT();
      result_str = ToString("(result ", type, ")");
    }
    control.Code("(if ", result_str, "").Comment("Execute code based on result of condition.")
           .Indent(2)
           .Code("(then").Comment("'then' block")
           .Indent(2);
    ChildToWAT(1, control, false);
    control.Indent(-2);
    control.Code(")").Comment("End 'then'");
    if (NumChildren() == 3) {
      control.Code("(else").Comment("'else' block");
      control.Indent(2);
      ChildToWAT(2, control, false);
      control.Indent(-2);
      control.Code(")").Comment("End 'else'");
    }
    control.Indent(-2);
    control.Code(")").Comment("End 'if'");
    return false;
  }
};

class ASTNode_While : public ASTNode_Parent {
public:
  ASTNode_While(FilePos file_pos, ptr_t && test, ptr_t && action)
    : ASTNode_Parent(file_pos, test, action) { }

  std::string GetTypeName() const override { return "WHILE"; }

  bool IsReturn() const override {
    return false;   // Loop may never run, so return is never guaranteed.
  }
  bool MayReturn() const override {
    return GetChild(1).MayReturn();
  }

  Type ReturnType(const SymbolTable & symbols) const override {
    return GetChild(1).ReturnType(symbols);
  }

  void TypeCheck(const SymbolTable & symbols) override {
    if (NumChildren() != 2) {
      Error(file_pos, "Internal error: Expected 2 in while node, found ", NumChildren());
    }
    TypeCheckChildren(symbols);
    if (!GetChild(0).ReturnType(symbols).IsInt()) {
      Error(GetChild(0).GetFilePos(), "Condition for while-statement must evaluate to type int, not ",
            GetChild(0).ReturnType(symbols).Name());
    }
  }

  bool ToWAT(Control & control) override {
    assert(NumChildren() == 2);
    // A while loop may go around again, so we cannot treat any node inside of it as final.
    // (In practice, though programs functions should end with a while anyway)
    control.FinalNode(false);
    std::string while_exit = control.MakeLabel("$exit");
    std::string while_loop = control.MakeLabel("$loop");

    // Store labels in case of break or continue.
    control.PushBreakLabel(while_exit);
    control.PushLoopLabel(while_loop);
    
    control.Code("(block ", while_exit, "").Comment("Outer block for breaking while loop.")
           .Code("  (loop ", while_loop, "").Comment("Inner loop for continuing while.");
    control.Indent(4);
    control.CommentLine("WHILE Test condition...");

    ChildToWAT(0, control, true);

    control.Code("(i32.eqz)").Comment("Invert the result of the test condition.")
           .Code("(br_if ", while_exit, ")").Comment("If condition is false (0), exit the loop")
           .CommentLine("WHILE Loop body...");

    ChildToWAT(1, control, false);

    control.CommentLine("WHILE start next loop.")
           .Code("(br ", while_loop, ")").Comment("Jump back to the start of the loop");
    control.Indent(-4);
    control.Code("  )").Comment("End loop")
           .Code(")").Comment("End block");

    // Remove labels for break and continue;
    control.PopBreakLabel();
    control.PopLoopLabel();

    return false;
  }
};

class ASTNode_Return : public ASTNode_Parent {
public:
  ASTNode_Return(FilePos file_pos, ptr_t && expr)
    : ASTNode_Parent(file_pos, expr) { }

  std::string GetTypeName() const override { return "RETURN"; }

  bool IsReturn() const override { return true; }
  bool MayReturn() const override { return true; }

  Type ReturnType(const SymbolTable & symbols) const override {
    return GetChild(0).ReturnType(symbols);
  }

  void TypeCheck(const SymbolTable & symbols) override {
    if (NumChildren() != 1) {
      Error(file_pos, "Internal error: Expected one arg in return node, found ", NumChildren());
    }
    TypeCheckChildren(symbols);
    // @CAO - SHOULD CHECK RETURN TYPE.
  }

  bool ToWAT(Control & control) override {
    // Simply leave the return value on the stack.
    ChildToWAT(0, control, true);
    // If this is not a final node, we should set up a break.
    if (!control.FinalNode()) {
      control.Code("(return)").Comment("Halt and return value.");
    }
    return false;
  }
};

class ASTNode_Break : public ASTNode {
public:
  ASTNode_Break(FilePos file_pos) : ASTNode(file_pos) { }
  std::string GetTypeName() const override { return "BREAK"; }

  bool ToWAT(Control & control) override {
    if (!control.HasLoopLabel()) Error(file_pos, "No loop for `break` to exit.");
    std::string loop_exit = control.GetBreakLabel();
    control.Code("(br ", loop_exit, ")").Comment("'break' command.");
    return false;
  }
};

class ASTNode_Continue : public ASTNode {
public:
  ASTNode_Continue(FilePos file_pos) : ASTNode(file_pos) { }
  std::string GetTypeName() const override { return "CONTINUE"; }

  bool ToWAT(Control & control) override {
    if (!control.HasLoopLabel()) Error(file_pos, "No loop for `continue` to operate on.");
    std::string loop_label = control.GetLoopLabel();
    control.Code("(br ", loop_label, ")").Comment("'continue' command.");
    return false;
  }
};

class ASTNode_ToDouble : public ASTNode_Parent {
public:
  ASTNode_ToDouble(ptr_t && child) : ASTNode_Parent(child->GetFilePos(), child) { }
  std::string GetTypeName() const override { return "ToDouble"; }
  Type ReturnType(const SymbolTable &) const override { return Type{"double"}; }

  void TypeCheck(const SymbolTable & symbols) override {
    if (NumChildren() != 1) {
      Error(file_pos, "Internal error: Expected child in ToDouble node, found ", NumChildren());
    }
    TypeCheckChildren(symbols);
    const Type & child_type = GetChild(0).ReturnType(symbols);
    if (!child_type.CastToOK(Type("double"))) {
      Error(file_pos, "Cannot convert type ", child_type.Name(), " to double.");
    }
  }

  bool ToWAT(Control & control) override {
    assert(NumChildren() == 1);
    ChildToWAT(0, control, true);
    if (!GetChild(0).ReturnType(control.symbols).IsDouble()) {
      control.Code("(f64.convert_i32_s)").Comment("Convert to double.");
    }
    return true;
  }
};

class ASTNode_ToInt : public ASTNode_Parent {
public:
  ASTNode_ToInt(ptr_t && child) : ASTNode_Parent(child->GetFilePos(), child) { }
  std::string GetTypeName() const override { return "ToInt"; }
  Type ReturnType(const SymbolTable &) const override { return Type("int"); }

  void TypeCheck(const SymbolTable & symbols) override {
    if (NumChildren() != 1) {
      Error(file_pos, "Internal error: Expected child in ToInt node, found ", NumChildren());
    }
    TypeCheckChildren(symbols);
    const Type & child_type = GetChild(0).ReturnType(symbols);
    if (!child_type.CastToOK(Type("int"))) {
      Error(file_pos, "Cannot convert type ", child_type.Name(), " to int.");
    }
  }

  bool ToWAT(Control & control) override {
    assert(NumChildren() == 1);
    ChildToWAT(0, control, true);
    if (GetChild(0).ReturnType(control.symbols).IsDouble()) {
      control.Code("(i32.trunc_f64_s)").Comment("Convert to int.");
    }
    return true;
  }
};

class ASTNode_ToString : public ASTNode_Parent {
public:
  ASTNode_ToString(ptr_t && child) : ASTNode_Parent(child->GetFilePos(), child) { }
  std::string GetTypeName() const override { return "ToString"; }
  Type ReturnType(const SymbolTable &) const override { return Type("string"); }

  void TypeCheck(const SymbolTable & symbols) override {
    if (NumChildren() != 1) {
      Error(file_pos, "Internal error: Expected child in ToString node, found ", NumChildren());
    }
    TypeCheckChildren(symbols);
    const Type & child_type = GetChild(0).ReturnType(symbols);
    if (!child_type.CastToOK(Type("string"))) {
      Error(file_pos, "Cannot convert type ", child_type.Name(), " to string.");
    }
  }

  bool ToWAT(Control & control) override {
    assert(NumChildren() == 1);
    ChildToWAT(0, control, true);
    if (GetChild(0).ReturnType(control.symbols).IsChar()) {
      control.Code("(call $_char_to_string)").Comment("Convert to string.");
    }
    return true;
  }
};


class ASTNode_Math1 : public ASTNode_Parent {
protected:
  std::string op;
public:
  ASTNode_Math1(FilePos file_pos, std::string op, ptr_t && child)
    : ASTNode_Parent(file_pos, child), op(op) { }
  ASTNode_Math1(const emplex::Token & token, ptr_t && child)
    : ASTNode_Math1(token, token.lexeme, std::move(child)) { }

  std::string GetTypeName() const override { return std::string("MATH1: ") + op; }

  Type ReturnType(const SymbolTable & symbols) const override {
    if (op == "!") return Type("int");
    if (op == "sqrt") return Type("double");

    // Negation does not change the return type.
    return GetChild(0).ReturnType(symbols);
  }

  void TypeCheck(const SymbolTable & symbols) override {
    if (NumChildren() != 1) {
      Error(file_pos, "Internal error: Expected one child in Math1 node (", op, "), found ", NumChildren());
    }
    TypeCheckChildren(symbols);

    const Type & child_type = GetChild(0).ReturnType(symbols);
    if (op == "-") {
      if (child_type.IsChar() || !child_type.IsNumeric()) Error(file_pos, "Unary operator NEGATE (-) cannot be used on type '", child_type.Name(),"'.");
    }
    else if (op == "!") {
      if (!child_type.IsInt()) Error(file_pos, "Unary operator NOT (!) can only be used on 'int' types.");
    }
    else if (op == "sqrt") {
      if (!child_type.IsNumeric()) Error(file_pos, "Square root (sqrt) must have a numeric argument.");
      if (!child_type.IsDouble()) AdaptChild<ASTNode_ToDouble>(0);
    }
    else if (!child_type.IsNumeric()) {
      Error(file_pos, "In unary operator '", op, "', cannot convert type ",
            child_type.Name(), " to a numerical value.");
    }
  }

  bool ToWAT(Control & control) override {
    assert(NumChildren() == 1);

    if (op == "!") {
      ChildToWAT(0, control, true);
      control.Code("i32.eqz").Comment("Boolean NOT.");
    }
    else if (op == "-") {
      std::string type = ReturnType(control.symbols).ToWAT();
      control.Code("(", type, ".const 0)").Comment("Setup unary negation");
      ChildToWAT(0, control, true);
      control.Code("(", type, ".sub)").Comment("Unary negation.");
    }
    else if (op == "sqrt") {
      ChildToWAT(0, control, true);
      control.Code("(f64.sqrt)").Comment("Square Root");
    }

    return true;
  }
};

class ASTNode_Math2 : public ASTNode_Parent {
protected:
  std::string op;
public:
  ASTNode_Math2(FilePos file_pos, std::string op, ptr_t && child1, ptr_t && child2)
    : ASTNode_Parent(file_pos, child1, child2), op(op) { }
  ASTNode_Math2(const emplex::Token & token, ptr_t && child1, ptr_t && child2)
    : ASTNode_Parent(token, std::move(child1), std::move(child2)), op(token.lexeme) { }

  std::string GetTypeName() const override { return std::string("MATH2: " + op); }

  Type ReturnType(const SymbolTable & symbols) const override {
    // Assignments use the type of the variable being assigned.
    if (op == "=") return GetChild(0).ReturnType(symbols);

    // Comparisons and Boolean operations always return type int.
    if (op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=" ||
        op == "&&" || op == "||"|| op == "%") return Type("int");

    // Binary math scales to the higher precision of inputs.
    if (op == "*" || op == "/" || op == "+" || op == "-")  {
      if (GetChild(0).ReturnType(symbols).BitCount() > GetChild(1).ReturnType(symbols).BitCount()) {
        return GetChild(0).ReturnType(symbols);
      } else {
        return GetChild(1).ReturnType(symbols);
      }
    }

    return Type();
  }


  void TypeCheck(const SymbolTable & symbols) override {
    constexpr bool DEBUG = false;

    if (NumChildren() != 2) {
      Error(file_pos, "Internal error: Expected two children in ToDouble node, found ", NumChildren());
    }

    // Resolve types of children before checking this node.
    TypeCheckChildren(symbols);

    const Type & type0 = GetChild(0).ReturnType(symbols);
    const Type & type1 = GetChild(1).ReturnType(symbols);

    if constexpr (DEBUG) {
      std::cerr << "TESTING OP '" << op << "' with types " << type0.Name() << " and " << type1.Name() << "." << std::endl;
    }

    // Conduct tests based on the type of operator:
    enum Status { INVALID, OK,
                  PROMOTE0_INT, PROMOTE0_DOUBLE,
                  PROMOTE1_INT, PROMOTE1_DOUBLE,
                  PROMOTE0_STRING, PROMOTE1_STRING };
    Status status = INVALID;
    bool align_numeric = false;
    if (op == "*") {
      if ((type0.IsInt()    && type1.IsInt()) ||
          (type0.IsDouble() && type1.IsDouble()) ||
          (type0.IsAlpha() && type1.IsInt())) {
        status = OK;
      }
      else if (type0.IsInt() && type1.IsDouble()) status = PROMOTE0_DOUBLE;
      else if (type0.IsDouble() && type1.IsInt()) status = PROMOTE1_DOUBLE;
    }
    else if (op == "/") {
      if ((type0.IsInt() && type1.IsInt()) ||
          (type0.IsDouble() && type1.IsDouble()) ) {
        status = OK;
      }
      else if (type0.IsInt() && type1.IsDouble()) status = PROMOTE0_DOUBLE;
      else if (type0.IsDouble() && type1.IsInt()) status = PROMOTE1_DOUBLE;
    }
    else if (op == "%" || op == "&&" || op == "||") {
      if (type0.IsInt() && type1.IsInt()) status = OK;
    }
    else if (op == "+" || op == "-" || 
             op == "<" || op == "<=" || op == ">" || op == ">=" ||
             op == "==" || op == "!=") {
      align_numeric = true;
    }
    else if (op == "=") {
      // If both sides already match we're good.
      if (type0 == type1 && !type0.IsFunction())      status = OK;

      // Otherwise see about promote RHS only.
      else if (type0.IsDouble() && type1.IsNumeric()) status = PROMOTE1_DOUBLE;
      else if (type0.IsInt() && type1.IsChar())       status = PROMOTE1_INT;
      else if (type0.IsString() && type1.IsAlpha())     status = PROMOTE1_STRING;
    }
    else {
      // Internal error
      Error(file_pos, "Unknown binary operator in AST: ", op);
    }

    // If we deferred aligning numeric types above, handle it now.
    if (align_numeric && type0.IsNumeric() && type1.IsNumeric()) {
      if (type0 == type1) status = OK;
      else if (type0.IsDouble()) status = PROMOTE1_DOUBLE;
      else if (type1.IsDouble()) status = PROMOTE0_DOUBLE;
      else if (type1.IsInt())    status = PROMOTE0_INT;
      else if (type0.IsInt())    status = PROMOTE1_INT;
    }

    // If we are adding characters or strings
    if (op == "+" && type0.IsAlpha()) {
      if (type0 == type1)
        status = OK;
      else if (type0.IsString() && type1.IsChar())
        status = PROMOTE1_STRING;
      else if (type0.IsChar() && type1.IsString())
        status = PROMOTE0_STRING;
    }

    if constexpr (DEBUG) {
      // Print the current status.
      switch (status) {
      case PROMOTE0_INT:    std::cerr << "...Promoting 0 to INT"    << std::endl;  break;
      case PROMOTE0_DOUBLE: std::cerr << "...Promoting 0 to DOUBLE" << std::endl;  break;
      case PROMOTE0_STRING: std::cerr << "...Promoting 0 to String" << std::endl;  break;
      case PROMOTE1_INT:    std::cerr << "...Promoting 1 to INT"    << std::endl;  break;
      case PROMOTE1_DOUBLE: std::cerr << "...Promoting 1 to DOUBLE" << std::endl;  break;
      case PROMOTE1_STRING: std::cerr << "...Promoting 1 to String" << std::endl;  break;
      default: break;
      }
    }

    // Resolve the current status.
    switch (status) {
    case INVALID: 
      Error(file_pos, "Cannot use operator '", op, "' on types ", type0.Name(), " and ", type1.Name());
      break;
    case OK:              break;
    case PROMOTE0_INT:    AdaptChild<ASTNode_ToInt>(0);    break;
    case PROMOTE0_DOUBLE: AdaptChild<ASTNode_ToDouble>(0); break;
    case PROMOTE0_STRING: AdaptChild<ASTNode_ToString>(0); break;
    case PROMOTE1_INT:    AdaptChild<ASTNode_ToInt>(1);    break;
    case PROMOTE1_DOUBLE: AdaptChild<ASTNode_ToDouble>(1); break;
    case PROMOTE1_STRING: AdaptChild<ASTNode_ToString>(1); break;
    // I'm adapting in Parse. Will move it to here after I commit
    }
  }

  void ToWAT_Assign(Control & control) {
    if (!GetChild(0).CanAssign()) {
      Error(file_pos, "Left-hand-side of assignment must be a variable.");
    }
    
    ChildToWAT(1, control, true);      // Generate the value to assign
    GetChild(0).ToAssignWAT(control);  // Do the assignment
    ChildToWAT(0, control, true);      // Place the current value of var on the stack.
  }

  void ToWAT_AND(Control & control) {
    control.CommentLine("Setup the && operation");
    ChildToWAT(0, control, true); // First value sets the condition.
    control.Code("(if (result i32)").Comment("Setup for && operator")
           .Code("  (then").Indent(4);
    ChildToWAT(1, control, true); // If first value was true, result is second value.
    control.Code("(i32.const 0)").Comment("Put a zero on the stack for comparison)")
           .Code("(i32.ne)").Comment("Set any non-zero value to one.)")
           .Indent(-4)
           .Code("  )")
           .Code("  (else ")
           .Code("    (i32.const 0)").Comment("First clause of && was false.")
           .Code("  )")
           .Code(")")
           .CommentLine("End of && operation");
  }

  void ToWAT_OR(Control & control) {
    control.CommentLine("Setup the || operation");
    ChildToWAT(0, control, true); // First value sets the condition.
    control.Code("(if (result i32)").Comment("Setup for || operator")
           .Code("  (then")
           .Code("    (i32.const 1)").Comment("First clause of || was true.")
           .Code("  )")
           .Code("  (else ")
           .Indent(4);
    ChildToWAT(1, control, true); // If first value was true, result is true.
    control.Code("(i32.const 0)").Comment("Put a zero on the stack for comparison)")
           .Code("(i32.ne)").Comment("Set any non-zero value to one.)")
           .Indent(-4)
           .Code("  )")
           .Code(")")
           .CommentLine("End of || operation");
  }

  void ToWAT_Multiply(Control & control) {
    Type type = GetChild(0).ReturnType(control.symbols);
    if (type.IsNumeric()) {
      // Standard mathematical multiple.
      control.Code("(", type.ToWAT(), ".mul)").Comment("Stack2 * Stack1");
    }
  }

  void ToWAT_Add(Control & control) {
    Type type = GetChild(0).ReturnType(control.symbols);
    if (type.IsNumeric()) {
      // Standard mathematical addition.
      control.Code("(", type.ToWAT(), ".add)").Comment("Stack2 + Stack1");
    }
    else if (type.IsString())
    {
      control.Code("(call $_str_concat)")
        .Comment("concat new strings and return the newPosition");
    }
  }

  /* For keepsake
  void ToWAT_String(Control& control)
  {
    auto const& symbols = control.symbols;

    if (op == "+")  // Expect only char or string
    {
      if (GetChild(1).ReturnType(symbols).IsAlpha()) // String concat
      {
        control.CommentLine("Setup String concatenation");
        // Tell children to put themselves the stack
        ChildToWAT(0, control, true);
        ChildToWAT(1, control, true);
        control.Code("(call $_str_concat)")
        .Comment("concat new strings and return the newPosition");
      }

    }

  }
  */

  bool ToWAT(Control & control) override {
    assert(NumChildren() == 2);

    // If we are doing an assignment or boolean logic, we need to handle it specially.
    if (op == "=") { ToWAT_Assign(control); return true; }
    if (op == "&&") { ToWAT_AND(control); return true; }
    if (op == "||") { ToWAT_OR(control); return true; }

    ChildToWAT(0, control, true); // Calculate the first arg (so it's top of the stack)
    ChildToWAT(1, control, true); // Calculate the second arg (so it's one down on the stack)

    std::string type = GetChild(0).ReturnType(control.symbols).ToWAT();
    std::string extra = (type == "i32") ? "_s" : "";

    if (op == "*")  { ToWAT_Multiply(control); return true; }
    if (op == "/")  { control.Code("(", type, ".div", extra, ")").Comment("Stack2 / Stack1"); return true; }
    if (op == "%")  { control.Code("(", type, ".rem", extra, ")").Comment("Stack2 % Stack1"); return true; }
    if (op == "+")  { ToWAT_Add(control); return true; }
    if (op == "-")  { control.Code("(", type, ".sub)").Comment("Stack2 - Stack1"); return true; }

    if (op == "<")  { control.Code("(", type, ".lt", extra, ")").Comment("Stack2 < Stack1"); return true; }
    if (op == "<=") { control.Code("(", type, ".le", extra, ")").Comment("Stack2 <= Stack1"); return true; }
    if (op == ">")  { control.Code("(", type, ".gt", extra, ")").Comment("Stack2 > Stack1"); return true; }
    if (op == ">=") { control.Code("(", type, ".ge", extra, ")").Comment("Stack2 >= Stack1"); return true; }
    if (op == "==") { control.Code("(", type, ".eq)").Comment("Stack2 == Stack1"); return true; }
    if (op == "!=") { control.Code("(", type, ".ne)").Comment("Stack2 != Stack1"); return true; }


    return false;
  }
};

class ASTNode_CharLit : public ASTNode {
protected:
  int value = '\0';

public:
  ASTNode_CharLit(FilePos file_pos, int value)
    : ASTNode(file_pos), value(value) { }

  std::string GetTypeName() const override { return std::string("CHAR_LIT: ") + std::to_string(((int) value)); }

  Type ReturnType(const SymbolTable & /* symbols */) const override {
    // For now, ops do not change the return type.
    return Type("char");
  }

  bool ToWAT(Control & control) override {
    control.Code("(i32.const ", value, ")").Comment("Put a char \\", value, " on the stack");
    return true;
  }
};

class ASTNode_IntLit : public ASTNode {
protected:
  int value = 0.0;

public:
  ASTNode_IntLit(FilePos file_pos, int value)
    : ASTNode(file_pos), value(value) { }

  std::string GetTypeName() const override { return std::string("INT_LIT:") + std::to_string(value); }

  Type ReturnType(const SymbolTable & /* symbols */) const override {
    // For now, ops do not change the return type.
    return Type("int");
  }

  bool ToWAT(Control & control) override {
    control.Code("(i32.const ", value, ")").Comment("Put a ", value, " on the stack");
    return true;
  }
};

class ASTNode_FloatLit : public ASTNode {
protected:
  double value = 0.0;

public:
  ASTNode_FloatLit(FilePos file_pos, double value)
    : ASTNode(file_pos), value(value) { }

  std::string GetTypeName() const override { return "FLOAT_LIT"; }

  Type ReturnType(const SymbolTable & /* symbols */) const override {
    // For now, ops do not change the return type.
    return Type("double");
  }

  bool ToWAT(Control & control) override {
    control.Code("(f64.const ", value, ")").Comment("Put a ", value, " on the stack");
    return true;
  }
};

class ASTNode_Var : public ASTNode {
protected:
  static constexpr size_t MAX_ID = 1000000;
  size_t var_id = MAX_ID;

  void TestOK() const { assert(var_id < MAX_ID); }
public:
  ASTNode_Var(FilePos file_pos, size_t id) : ASTNode(file_pos), var_id(id) { TestOK(); }
  ASTNode_Var(const emplex::Token & token, SymbolTable & symbols)
    : ASTNode(token), var_id(symbols.GetVarID(token.lexeme)) { TestOK(); }

  std::string GetTypeName() const override { return std::string("VAR: ") + std::to_string(var_id); }

  bool CanAssign() const override { return true; }
  void ToAssignWAT(Control & control) override {
    TestOK();
    const std::string var_name = control.symbols.GetName(var_id);
    control.Code("(local.set $var", var_id, ")").Comment("Set var '", var_name, "' from stack");
  }

  Type ReturnType(const SymbolTable & symbols) const override {
    // For now, ops do not change the return type.
    TestOK();
    return symbols.GetType(var_id);
  }

  bool ToWAT(Control & control) override {
    TestOK();
    const std::string var_name = control.symbols.GetName(var_id);

    control.Code("(local.get $var", var_id, ")").Comment("Place var '", var_name, "' onto stack");
    return true;
  }

};

class ASTNode_Function_Call : public ASTNode_Parent {
  emplex::Token fun_token;
  size_t fun_id;


public:
  
  ASTNode_Function_Call(emplex::Token fun_info, size_t fun_id)
    : ASTNode_Parent(fun_info), fun_token(fun_info), fun_id(fun_id) {}

  // Function call guarantees a return. 
  // Might not terminate owning function though
  bool MayReturn() const override { return true; }

  std::string GetTypeName() const override {
    return "Function Call";
  }

  bool ToWAT(Control& control) {
    control.CommentLine("Function call setup");
    
    Type fun_type = control.symbols.GetType(fun_id);

    for (size_t i = 0; i < NumChildren(); ++i) {
      // Error check that the correct type was passed into the function
      if (GetChild(i).ReturnType(control.symbols) != fun_type.ParamType(i)) {
        Error(fun_token, "Invalid type for param", i);
      }

      // Put the argument on the stack for use
      ChildToWAT(i, control, true);
    }

    control.Code("(call $", fun_token.lexeme, ")")
      .Comment("Call the function");

    return true;  // Function calls always return a value
  }

  Type ReturnType(const SymbolTable & symbols) const override {
    return symbols.GetType(fun_id).ReturnType();
  }

};

class ASTNode_StringLit : public ASTNode {
  std::string str;
  size_t pos;

public:

  ASTNode_StringLit(emplex::Token token) : ASTNode(token), str(token.lexeme) {
    ReplaceAll(str, "\"", "");
  }

  std::string GetTypeName() const override { return "STRING_LIT"; }

  Type ReturnType(const SymbolTable&) const override {
    return Type("string");
  }

  void InitializeWAT(Control & control) override {
    pos = control.Data(str);
  }
  
  
  bool ToWAT(Control & control) override {
    control.Code("(i32.const ", pos, ")")
      .Comment("put the starting address of ", str, " on stack");
    return true;
  }

};
