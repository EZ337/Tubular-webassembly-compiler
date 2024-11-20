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

  TokenQueue tokens;

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

  Type GetReturnType(const ast_ptr_t & node_ptr) const {
    return node_ptr->ReturnType(control.symbols);
  }

public:
  Tubular(std::string filename) {    
    std::ifstream in_file(filename);              // Load the input file
    if (in_file.fail()) {
      std::cerr << "ERROR: Unable to open file '" << filename << "'." << std::endl;
      exit(1);
    }

    tokens.Load(in_file);  // Load all tokens from the file.
  }

  ast_ptr_t Parse_Statement() {
    // Test what kind of statement this is and call the appropriate function...
    switch (tokens.Peek()) {
      using namespace emplex;
      // case Lexer::ID_TYPE:   return Parse_Statement_Declare();
      // case Lexer::ID_IF:     return Parse_Statement_If();
      // case Lexer::ID_WHILE:  return Parse_Statement_While();
      // case Lexer::ID_RETURN: return Parse_Statement_Return();
      // case Lexer::ID_BREAK:  return Parse_Statement_Break();
      // case Lexer::ID_CONTINUE: return Parse_Statement_Continue();
      case '{': return Parse_StatementList();
      case ';':
        tokens.Use();
        return nullptr;
      // default: return Parse_Statement_Expression();
    }

    return nullptr;
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


  void Parse() {
    // Outer layer can only be function definitions.
    while (tokens.Any()) {
      // functions.push_back( Parse_Function() );
      // functions.back()->TypeCheck(control.symbols);
    }
  }

  void ToWAT() {
    control.Code("(module");
    control.Indent(2);

    // Manage DATA
    control.CommentLine(";; Define a memory block with ten pages (640KB)");
    control.Code("(memory (export \"memory\") 1)");

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

    // for (auto & fun_ptr : functions) {
    //   fun_ptr->ToWAT(control);
    // }

    control.Indent(-2);
    control.Code(")").Comment("END program module");
  }

  void PrintCode() const { control.PrintCode(); }
  void PrintSymbols() const { control.symbols.Print(); }
  void PrintAST() const {
    // for (auto & fun_ptr : functions) {
    //   fun_ptr->Print();
    // }
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
  prog.PrintCode();
}
