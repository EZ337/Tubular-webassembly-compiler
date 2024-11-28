#include <assert.h>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ASTNode.hpp"
#include "Control.hpp"
#include "lexer.hpp"
#include "SymbolTable.hpp"
#include "TokenQueue.hpp"

class Tubular {
private:
  using ast_ptr_t = std::unique_ptr<ASTNode>;
  using fun_ptr_t = std::unique_ptr<ASTNode_Function>;

  TokenQueue tokens;
  std::vector<fun_ptr_t> functions{};

  struct OpInfo {
    size_t level;
    char assoc;   // l=left; r=right; n=non
  };
  std::unordered_map<std::string, OpInfo> op_map{};

  Control control;

  // == HELPER FUNCTIONS

  template <typename... Ts>
  void TriggerError(Ts... message) {
    if (tokens.None()) tokens.Rewind();
    Error(tokens.CurFilePos(), std::forward<Ts>(message)...);
  }

  template <typename NODE_T, typename... ARG_Ts>
  static std::unique_ptr<NODE_T> MakeNode(ARG_Ts &&... args) {
    return std::make_unique<NODE_T>( std::forward<ARG_Ts>(args)... );
  }

  ast_ptr_t MakeVarNode(emplex::Token token) {
    return MakeNode<ASTNode_Var>(token, control.symbols);
  }

  Type GetReturnType(const ast_ptr_t & node_ptr) const {
    return node_ptr->ReturnType(control.symbols);
  }

  // Take in the provided node and add an ASTNode converter to make it a double, as needed.
  // Return if a change was made.
  ast_ptr_t PromoteToDouble(ast_ptr_t && node_ptr) {
    if (!node_ptr->ReturnType(control.symbols).IsDouble()) {
      return MakeNode<ASTNode_ToDouble>(std::move(node_ptr));
    }
    return node_ptr;
  }

  // Take in the provided node and add an ASTNode converter to make it a double, as needed.
  // Return if a change was made.
  ast_ptr_t DemoteToInt(ast_ptr_t && node_ptr) {
    if (node_ptr->ReturnType(control.symbols).IsDouble()) {
      return MakeNode<ASTNode_ToInt>(std::move(node_ptr));
    }
    return node_ptr;
  }

  void SetupOperators() {
    // Setup operator precedence.
    size_t cur_prec = 0;
    op_map["("]  = op_map["!"]  =               OpInfo{cur_prec++, 'n'};
    op_map["*"]  = op_map["/"]  = op_map["%"] = OpInfo{cur_prec++, 'l'};
    op_map["+"]  = op_map["-"]  =               OpInfo{cur_prec++, 'l'};
    op_map["<"]  = op_map["<="] = op_map[">"] = op_map[">="] = OpInfo{cur_prec++, 'n'};
    op_map["=="] = op_map["!="] =               OpInfo{cur_prec++, 'n'};
    op_map["&&"] =                              OpInfo{cur_prec++, 'l'};
    op_map["||"] =                              OpInfo{cur_prec++, 'l'};
    op_map["="]  =                              OpInfo{cur_prec++, 'r'};
  }

public:
  Tubular(std::string filename) {    
    std::ifstream in_file(filename);              // Load the input file
    if (in_file.fail()) {
      std::cerr << "ERROR: Unable to open file '" << filename << "'." << std::endl;
      exit(1);
    }

    tokens.Load(in_file);  // Load all tokens from the file.

    SetupOperators();
  }

  // Convert any token representing a unary value into an ASTNode.
  // (i.e., a leaf in an expression and associated unary operators)
  ast_ptr_t Parse_UnaryTerm() {
    const emplex::Token & token = tokens.Use();

    if (token == '+') return Parse_UnaryTerm(); // (Operator + does nothing...)

    if (token == '-' || token == '!') {  // Add node for unary prefix
      return MakeNode<ASTNode_Math1>(token, Parse_UnaryTerm());
    }

    // Check main terms.
    ast_ptr_t out;
    switch (token.id) {
    case '(': // Allow full expressions in parentheses.
      out = Parse_Expression();
      tokens.Use(')');
      break;
    case emplex::Lexer::ID_ID:
      if (!control.symbols.Has(token.lexeme)) {
        Error(token, "Unknown variable '", token.lexeme, "'.");
      }
      if (tokens.Is('(')) {
        out = Parse_Function_Call(token);
      }
      else
      {
        out = MakeVarNode(token);
      }
      break;
    case emplex::Lexer::ID_LIT_INT:
      out = MakeNode<ASTNode_IntLit>(token, std::stoi(token.lexeme));
      break;
    case emplex::Lexer::ID_LIT_CHAR:
      out = MakeNode<ASTNode_CharLit>(token, token.lexeme[1]);
      break;
    case emplex::Lexer::ID_LIT_FLOAT:
      out = MakeNode<ASTNode_FloatLit>(token, std::stod(token.lexeme));
      break;
    case emplex::Lexer::ID_SQRT:
      tokens.Use('(');
      out = PromoteToDouble(Parse_Expression());
      tokens.Use(')');
      out = MakeNode<ASTNode_Math1>(token, std::move(out));
      break;
    default:
      Error(token, "Unexpected token '", token.lexeme, "'");
    }

    // @CAO Check for '(' or '[' to know if this is a function call or array index?

    // Check to see if the term is followed by a type modifier.
    if (tokens.UseIf(':')) {
      auto type_token = tokens.Use(emplex::Lexer::ID_TYPE, "Expected a type specified after ':'.");
      if (type_token.lexeme == "double") out = MakeNode<ASTNode_ToDouble>(std::move(out));
      else if (type_token.lexeme == "int") out = MakeNode<ASTNode_ToInt>(std::move(out));
    }

    return out;
  }

  // Parse expressions.  The level input determines how restrictive this parse should be.
  // Only continue processing with types at the target level or higher.
  ast_ptr_t Parse_Expression(size_t prec_limit=1000) {
    // Any expression must begin with a variable name or a literal value.
    ast_ptr_t cur_node = Parse_UnaryTerm();

    size_t skip_prec = 1000; // If we get a non-associative op, we must skip next one.

    // While there are more tokens to process, try to expand this expression.
    while (tokens.Any()) {
      // Peek at the next token; if it is an op, keep going and get its info.
      auto op_token = tokens.Peek();
      if (!op_map.count(op_token.lexeme)) break;  // Not an op token; stop here!
      OpInfo op_info = op_map[op_token.lexeme];

      // If precedence of next operator is too high, return what we have.
      if (op_info.level > prec_limit) break;

      // If the next precedence is not allowed, throw an error.
      if (op_info.level == skip_prec) {
        Error(op_token, "Operator '", op_token.lexeme, "' is non-associative.");
      }

      // If we made it here, we have a binary operation to use, so consume it.
      tokens.Use();

      // Find the allowed precedence for the next term.
      size_t next_limit = op_info.level;
      if (op_info.assoc != 'r') --next_limit;

      // Load the next term.
      ast_ptr_t node2 = Parse_Expression(next_limit);

      // Build the new node.
      cur_node = MakeNode<ASTNode_Math2>(op_token, std::move(cur_node), std::move(node2));

      // If operator is non-associative, skip the current precedence for next loop.
      skip_prec = (op_info.assoc == 'n') ? op_info.level : 1000;
    }

    return cur_node;
  }

  ast_ptr_t Parse_Statement() {
    // Test what kind of statement this is and call the appropriate function...
    switch (tokens.Peek()) {
      using namespace emplex;
      case Lexer::ID_TYPE:   return Parse_Statement_Declare();
      case Lexer::ID_IF:     return Parse_Statement_If();
      case Lexer::ID_WHILE:  return Parse_Statement_While();
      case Lexer::ID_RETURN: return Parse_Statement_Return();
      case Lexer::ID_BREAK:  return Parse_Statement_Break();
      case Lexer::ID_CONTINUE: return Parse_Statement_Continue();
      case '{': return Parse_StatementList();
      case ';':
        tokens.Use();
        return nullptr;
      default: return Parse_Statement_Expression();
    }
  }

  ast_ptr_t Parse_Statement_Declare() {
    auto type_token = tokens.Use();
    const auto id_token =
      tokens.Use(emplex::Lexer::ID_ID, "Declarations must have a type followed by identifier.");
    control.symbols.AddVar(type_token, id_token);
    if (tokens.UseIf(';')) {
      return nullptr;  // Variable added, nothing else to do.
    }
    auto op_token =
      tokens.Use('=', "Expected ';' or '=' after declaration of variable '", id_token.lexeme, "'.");
    auto rhs_node = Parse_Expression();
    tokens.Use(';');

    auto lhs_node = MakeVarNode(id_token);
    Type lhs_type = lhs_node->ReturnType(control.symbols);
    Type rhs_type = rhs_node->ReturnType(control.symbols);

    return MakeNode<ASTNode_Math2>(id_token, "=", std::move(lhs_node), std::move(rhs_node));
  }

  ast_ptr_t Parse_Statement_If() {
    auto if_token = tokens.Use(emplex::Lexer::ID_IF);
    tokens.Use('(', "If commands must be followed by a '(");
    ast_ptr_t condition = Parse_Expression();
    tokens.Use(')');
    ast_ptr_t action = Parse_Statement();

    // Check if we need to add on an "else" branch
    if (tokens.UseIf(emplex::Lexer::ID_ELSE)) {
      ast_ptr_t alt = Parse_Statement();
      return MakeNode<ASTNode_If>(if_token, std::move(condition), std::move(action), std::move(alt));
    }

    return MakeNode<ASTNode_If>(if_token, std::move(condition), std::move(action));
  }

  ast_ptr_t Parse_Statement_While() {
    auto while_token = tokens.Use(emplex::Lexer::ID_WHILE);
    tokens.Use('(', "While commands must be followed by a '(");
    ast_ptr_t condition = Parse_Expression();
    tokens.Use(')');
    ast_ptr_t action = Parse_Statement();
    return MakeNode<ASTNode_While>(while_token, std::move(condition), std::move(action));
  }

  ast_ptr_t Parse_Statement_Return() {
    auto token = tokens.Use(emplex::Lexer::ID_RETURN);
    ast_ptr_t return_expr = Parse_Statement_Expression();
    return MakeNode<ASTNode_Return>(token, std::move(return_expr));
  }

  ast_ptr_t Parse_Statement_Break() {
    auto token = tokens.Use(emplex::Lexer::ID_BREAK);
    return MakeNode<ASTNode_Break>(token);
  }

  ast_ptr_t Parse_Statement_Continue() {
    auto token = tokens.Use(emplex::Lexer::ID_CONTINUE);
    return MakeNode<ASTNode_Continue>(token);
  }

  ast_ptr_t Parse_Statement_Expression() {
    ast_ptr_t out = Parse_Expression();
    tokens.Use(';');
    return out;
 }

  ast_ptr_t Parse_StatementList() {
    auto out_node = MakeNode<ASTNode_Block>(tokens.Peek());
    tokens.Use('{', "Statement blocks must start with '{'.");
    control.symbols.PushScope();
    while (tokens.Any() && tokens.Peek() != '}') {
      ast_ptr_t statement = Parse_Statement();
      if (statement) out_node->AddChild( std::move(statement) );
    }
    control.symbols.PopScope();
    tokens.Use('}', "Statement blocks must end with '}'.");
    return out_node;
  }

ast_ptr_t Parse_Function_Call(emplex::Token fun_token) {
  // At this point, fun_token is the token containing the function name
  
  tokens.Use(); // Use the '(' token initiating a function call

  size_t fun_id = control.symbols.GetVarID(fun_token.lexeme);
  Type fun_type = control.symbols.GetType(fun_id);

  if (!fun_type.IsFunction()) // treated a variable as if it's a function
  {
    Error(fun_token, fun_token.lexeme, " cannot be used as a function");
  }

  // std::string str = ToString("function information:\nFunction id: ", fun_id, "\n Function name: ",
  //   fun_token.lexeme, "\nFunction return type: ", fun_type.ReturnType().ToWAT());

  // std::cout << str << std::endl;

  auto fun_call = MakeNode<ASTNode_Function_Call>(fun_token, fun_id);

  for (size_t i = 0; i < fun_type.NumParams(); ++i) {
    //ast_ptr_t arg = Parse_Statement_Expression();
    auto arg = Parse_Expression();  // Can be inlined. Just for debugging rn
    
    // Type mismatch check can happen here. Might just do it in TypeCheck
    fun_call.get()->AddChild(std::move(arg));
    if (!tokens.UseIf(',')) {
      break;  // Break if we don't separate
    }
  }

  tokens.Use(')'); // Expecting an end of function call

  return fun_call;
}

  // A function has the format:
  //    function ID ( PARAMETERS ) : TYPE { STATEMENT_BLOCK }
  //    The initial ID is the function name.
  //    PARAMETERS can be empty or a series of comma-separated "TYPE ID" declaring parameters
  //    TYPE can be int, char, or double and is used as the return type.
  //    STATEMENT BLOCK is a series of statements to run, ending in a return statement.
  fun_ptr_t Parse_Function() {
    using namespace emplex;
    tokens.Use(Lexer::ID_FUNCTION, "Outermost scope must define functions.");
    control.symbols.PushScope();  // Enter a special scope for the function.
    auto name_token = tokens.Use(Lexer::ID_ID, "Function must have a name.");
    tokens.Use('(', "Function declaration must have '(' after name.");
    std::vector<size_t> param_ids;
    std::vector<Type> param_types;
    while (!tokens.UseIf(')')) {
      auto type_token = tokens.Use(Lexer::ID_TYPE);
      param_types.emplace_back(type_token);
      const auto id_token =
        tokens.Use(emplex::Lexer::ID_ID, "Function parameters must have a type followed by identifier.");
      size_t param_id = control.symbols.AddVar(type_token, id_token);
      param_ids.push_back(param_id);
      if (!tokens.UseIf(',') && !tokens.Is(')')) {
        TriggerError("Parameters must be separated by commas (','; found '", tokens.Peek().lexeme, "'.");
      }
    }
    tokens.Use(':');
    Type return_type( tokens.Use(Lexer::ID_TYPE) );

    // Now that we have the function signature, let the symbol table know about it.
    size_t fun_id = control.symbols.AddFunction(name_token, param_types, return_type);

    // Now parse the body of this function.
    control.symbols.ClearFunctionVars();
    ast_ptr_t body = Parse_StatementList();
    control.symbols.PopScope(); // Leave the function scope.

    if (!body->IsReturn()) {
      Error(name_token, "Function '", name_token.lexeme, "' must guarantee a return statement through all paths.");
    }

    auto out_node = MakeNode<ASTNode_Function>(name_token, fun_id, param_ids, std::move(body));
    out_node->SetVars( control.symbols.GetFunctionVars() );
    return out_node;
  }

  void Parse() {
    // Outer layer can only be function definitions.
    while (tokens.Any()) {
      functions.push_back( Parse_Function() );
      // functions.back()->TypeCheck(control.symbols);
    }
  }

  void ToWAT() {
    control.Code("(module");
    control.Indent(2);

    // Manage DATA
    control.CommentLine(";; Define a memory block with ten pages (640KB)");
    control.Code("(memory (export \"memory\") 1)");
    for (auto & fun_ptr : functions) {
      fun_ptr->InitializeWAT(control);
    }
    control.Code("(global $free_mem (mut i32) (i32.const ", control.wat_mem_pos, "))")
           .Code("");

    control.Code(";; Function to allocate a string; add one to size and places null there.")
           .Code("(func $_alloc_str (param $size i32) (result i32)")
           .Code("  (local $null_pos i32) ;; Local variable to place null terminator.")
           .Code("  (global.get $free_mem)").Comment("Old free mem is alloc start.")
           .Code("  (global.get $free_mem)").Comment("Adjust new free mem.")
           .Code("  (local.get $size)")
           .Code("  (i32.add)")
           .Code("  (local.set $null_pos)")
           .Code("  (i32.store8 (local.get $null_pos) (i32.const 0))").Comment("Place null terminator.")
           .Code("  (i32.add (i32.const 1) (local.get $null_pos))")
           .Code("  (global.set $free_mem)").Comment("Update free memory start.")
           .Code(")")
           .Code("");

    for (auto & fun_ptr : functions) {
      fun_ptr->ToWAT(control);
    }

    control.Indent(-2);
    control.Code(")").Comment("END program module");
  }

  void PrintCode(std::ostream& os = std::cout) const { control.PrintCode(os); }
  void PrintSymbols() const { control.symbols.Print(); }
  void PrintAST() const {
    for (auto & fun_ptr : functions) {
      fun_ptr->Print();
    }
  }
};


int main(int argc, char * argv[])
{
  if (argc != 2) {
    std::cout << "Format: " << argv[0] << " [filename]" << std::endl;
    exit(1);
  }


  Tubular prog(argv[1]);
  prog.Parse();

  // prog.PrintSymbols();
  // prog.PrintAST();
  // std::cout << "---------" << std::endl;
  prog.ToWAT();
  // std::cout << "---------" << std::endl;
  // prog.PrintSymbols();
  // prog.PrintAST();

  std::ofstream os("ez.wat");
  prog.PrintCode((std::string(argv[1]) == "ez_test") ? os : std::cout);
  os.close();
}
