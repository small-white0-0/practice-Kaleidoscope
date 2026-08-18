#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"


#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DIBuilder.h"

#include "lexer/input_char_stream.hpp"
#include "lexer/token_stream.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "codegen/context.hpp"

void mainLoop(TokenStream &stream, CodeGenContext &ctx) {
    while (true) {
        auto first_token = stream.peekToken();
        if (std::holds_alternative<std::monostate>(first_token)) {
            return;
        }
        if (isChar(first_token, ';')) {
            stream.nextToken();
        } else if (std::holds_alternative<Def>(first_token)) {
            stream.nextToken(); // eat def
            auto def = parseDefinition(stream);
            def.gen_code(ctx);
        } else if (std::holds_alternative<Extern>(first_token)) {
            stream.nextToken(); // eat extern
            auto extern_statement = parseExtern(stream);
            extern_statement.gen_code(ctx);
        } else {
            auto top_level_expr = parseTopLevelExpr(stream);
            top_level_expr.gen_code(ctx);
        }
    }
}

int main() {
    //
    GLOBAL_BINARY_OPS['='] = 1;
    GLOBAL_BINARY_OPS['<'] = 5;
    GLOBAL_BINARY_OPS['>'] = 5;
    GLOBAL_BINARY_OPS['+'] = 10;
    GLOBAL_BINARY_OPS['-'] = 10;
    GLOBAL_BINARY_OPS['*'] = 20;
    GLOBAL_BINARY_OPS['/'] = 20;
    auto ctx = std::make_unique<CodeGenContext>();
    ctx->setBinOp('=', [](auto l, auto r, CodeGenContext &ctx) {
        if (!l->getType()->isPointerTy()) {
            throw std::runtime_error{"left value of '=' needs pointer type"};
        }
        return ctx.Builder->CreateStore(r, l);
    });
    ctx->setBinOp('<', [](auto l, auto r, CodeGenContext &ctx) {
        auto L = ctx.Builder->CreateFCmpULT(l, r, "cmptmp");
        // Convert bool 0/1 to double 0.0 or 1.0
        return ctx.Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*ctx.TheContext), "booltmp");
    });
    ctx->setBinOp('>', [](auto l, auto r, CodeGenContext &ctx) {
        auto L = ctx.Builder->CreateFCmpUGT(l, r, "cmptmp");
        // Convert bool 0/1 to double 0.0 or 1.0
        return ctx.Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*ctx.TheContext), "booltmp");
    });
    ctx->setBinOp('+', [](auto l, auto r, CodeGenContext &ctx) {
        return ctx.Builder->CreateFAdd(l, r);
    });
    ctx->setBinOp('-', [](auto l, auto r, CodeGenContext &ctx) {
        return ctx.Builder->CreateFSub(l, r);
    });
    ctx->setBinOp('*', [](auto l, auto r, CodeGenContext &ctx) {
        return ctx.Builder->CreateFMul(l, r);
    });
    ctx->setBinOp('/', [](auto l, auto r, CodeGenContext &ctx) {
        return ctx.Builder->CreateFDiv(l, r);
    });

    auto stream = TokenStream{std::move(std::make_unique<InputCharStream>())};
    try {
        mainLoop(stream, *ctx);
        ctx->TheModule->print(llvm::outs(), nullptr); //print(llvm::outs());
        ctx->TheDIBuilder->finalize();
#ifdef gen_obj_file
        ctx->genObjFile(std::string("code_out.o"));
#endif
    } catch (std::exception &e) {
        std::cerr << "报错信息：" << e.what() << "===================\n" << std::endl;
        return 1;
    }

    return 0;
}
