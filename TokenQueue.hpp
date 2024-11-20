#pragma once

// A dynamic token manager.
// 
// Example usages:
//   TokenQueue tokens;
//   tokens.Load(filename);          // Load a file
//   auto token = tokens.Use();      // Get the next token and advance
//   bool found = tokens.UseIf('$'); // Use the next token IF it is a dollar sign
//   auto token2 = tokens.Peek();    // Get the next token _without_ advancing
//
//   // Use the next token; if it is NOT an IDENTIFIER token, give an error and exit!
//   auto token3 = tokens.Use(Lexer::ID_IDENTIFIER);
//   

#include <assert.h>
#include <vector>

#include "lexer.hpp"

class TokenQueue {
private:
  emplex::Lexer lexer;

  std::vector<emplex::Token> tokens{};
  size_t token_id = 0;

  const emplex::Token eof_token{0, "_EOF_", 0, 0};

  // == Helper functions ==
  void Cleanup() {
    assert(token_id <= tokens.size());
    if (token_id) {
      tokens.erase(tokens.begin(), tokens.begin()+token_id);
      token_id = 0;
    }
  }

public:
  void Reset() { tokens.resize(0); token_id = 0; }

  // Load in tokens from a stream.
  void Load(std::istream & is) {
    Cleanup();
    auto new_tokens = lexer.Tokenize(is);
    if (tokens.size() == 0) std::swap(tokens, new_tokens);
    else tokens.insert( tokens.end(), new_tokens.begin(), new_tokens.end() );
  }

  // Load in tokens from a string.
  void Load(const std::string & str) {
    Cleanup();
    auto new_tokens = lexer.Tokenize(str);
    if (tokens.size() == 0) std::swap(tokens, new_tokens);
    else tokens.insert( tokens.end(), new_tokens.begin(), new_tokens.end() );
  }

  // Count remaining tokens.
  size_t Size() const { return tokens.size() - token_id; }

  // Test if there are ANY tokens remaining.
  bool Any() const { return token_id < tokens.size(); }

  // Test if there are NO tokens remaining.
  bool None() const { return token_id >= tokens.size();; }

  // Test if a specific token is next.
  bool Is(int id) const { return Any() && Peek() == id; }

  // Get the next token, but don't remove it from the queue.
  const emplex::Token & Peek() const {
    if (None()) return eof_token;
    return tokens[token_id];
  }

  // Get the next token, removing it from the queue.
  const emplex::Token & Use() {
    if (None()) return eof_token;
    return tokens[token_id++];
  }

  // Get and remove the next token, give provided error if is not expected id.
  template <typename... Ts>
  const emplex::Token & Use(int id, Ts &&... message) {
    if (!Is(id)) {
      if constexpr (sizeof...(Ts) == 0) {
        Error( CurFilePos(), "Expected token of type ", emplex::Lexer::TokenName(id),
                          ", but found type ", emplex::Lexer::TokenName(Peek()));
      }
      else Error( CurFilePos(), std::forward<Ts>(message)...);
    }
    return Use();
  }

  // If the next token is one of the provided ids, use it an return it.
  // Otherwise, don't use it and return 0.
  template <typename... Ts>
  int UseIf(int id, Ts... args) {
    if (Is(id)) { return Use(); }
    return UseIf(args...);
  }

  // Base case for UseIf
  int UseIf() { return 0; }

  void Rewind() {
    assert(token_id > 0);
    token_id--;
  }

  // Get the current lexeme.
  std::string CurLexeme() const { return Any() ? Peek().lexeme : ""; }

  // Get the current line number.
  size_t CurLine() const { return Any() ? Peek().line_id : 0; }

  // Get the current column number.
  size_t CurColumn() const { return Any() ? Peek().col_id : 0; }

  FilePos CurFilePos() const {
    if (Any()) return FilePos{ Peek().line_id, Peek().col_id };
    else return FilePos{0,0};
  }

};
