#ifndef PRACTICE_KALEIDOSCOPE_PARSER_HPP
#define PRACTICE_KALEIDOSCOPE_PARSER_HPP

#include <map>

#include "ast.hpp"
#include "lexer/token_stream.hpp"

inline auto GLOBAL_BINARY_OPS = std::map<char, int>();

DefinitionAst parseDefinition(TokenStream &stream);

ExternAst parseExtern(TokenStream &stream);

DefinitionAst parseTopLevelExpr(TokenStream &stream);

bool isChar(const Token &tok, const char tc);
#endif //PRACTICE_KALEIDOSCOPE_PARSER_HPP
