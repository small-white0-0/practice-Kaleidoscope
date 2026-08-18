#ifndef PRACTICE_KALEIDOSCOPE_AST_HPP
#define PRACTICE_KALEIDOSCOPE_AST_HPP

#include "lexer/token.hpp"
#include <memory>
#include <vector>
#include <cstdint>

// 前向声明
class CodeGenContext;

namespace llvm {
    class Value;
}

// 正式声明

class ExprAst {
public:
    virtual ~ExprAst() = default;

    virtual llvm::Value *gen_code(CodeGenContext &ctx) =0;

    virtual Position getLocation() =0;
};

class NumberExprAst final : public ExprAst {
public:
    Number number;

    explicit NumberExprAst(Number number) : number{std::move(number)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        return number.loc.start;
    }
};

class VariableExprAst final : public ExprAst {
public:
    Identifier identifier;

    explicit VariableExprAst(Identifier identifier) : identifier{std::move(identifier)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        return identifier.loc.start;
    }
};

class BinaryExprAst final : public ExprAst {
public:
    char ops;
    std::unique_ptr<ExprAst> left, right;

    BinaryExprAst(char op, std::unique_ptr<ExprAst> left,
                  std::unique_ptr<ExprAst> right) : ops{op}, left{std::move(left)}, right{std::move(right)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        // 如果选left，配合左结合的运算符，会一直是第一个操作符的位置，
        // 对于换行的情况会存在前后跳跃。
        // 所以改成right,这样至少不会发生不停跳跃的情况。
        // 虽然个人感觉，使用ops的位置会更好，但是，因为解析的时候，没有保留Char对象，位置丢了，就这样吧。
        return right->getLocation();
    }
};

class UnaryExprAst final : public ExprAst {
public:
    Char ops;
    std::unique_ptr<ExprAst> operand;

    UnaryExprAst(Char op, std::unique_ptr<ExprAst> operand) : ops{std::move(op)}, operand{std::move(operand)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        return ops.loc.start;
    }
};


class CallExprAst final : public ExprAst {
public:
    Identifier identifier;
    std::vector<std::unique_ptr<ExprAst> > args;

    CallExprAst(Identifier identifier, std::vector<std::unique_ptr<ExprAst> > args) : identifier{
            std::move(identifier)
        },
        args{std::move(args)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        return identifier.loc.start;
    }
};

class IfExprAst final : public ExprAst {
public:
    If ifKey;
    std::unique_ptr<ExprAst> condition;
    std::unique_ptr<ExprAst> thenExpr;
    std::unique_ptr<ExprAst> elseExpr;

    IfExprAst(If ifKey, std::unique_ptr<ExprAst> condition, std::unique_ptr<ExprAst> thenExpr,
              std::unique_ptr<ExprAst> elseExpr) : ifKey(std::move(ifKey)), condition{
                                                       std::move(condition)
                                                   }, thenExpr{std::move(thenExpr)}, elseExpr{std::move(elseExpr)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        return ifKey.loc.start;
    }
};

class ForExprAst final : public ExprAst {
public:
    For forKey;
    Identifier var;
    Number init;
    std::unique_ptr<ExprAst> endCondition;
    std::optional<Number> step;
    std::unique_ptr<ExprAst> body;

    ForExprAst(For forKey, Identifier var, Number init,
               std::unique_ptr<ExprAst> endCondition,
               std::optional<Number> step, std::unique_ptr<ExprAst> body)
        : forKey(std::move(forKey)), var(std::move(var)), init(std::move(init)), endCondition(std::move(endCondition)),
          step(std::move(step)), body(std::move(body)) {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        return forKey.loc.start;
    }
};

class VarExprAst final : public ExprAst {
public:
    Var varKey;
    std::vector<std::tuple<Identifier, std::unique_ptr<ExprAst> > > variables;
    std::unique_ptr<ExprAst> body;

    VarExprAst(Var varKey, std::vector<std::tuple<Identifier, std::unique_ptr<ExprAst> > > variables,
               std::unique_ptr<ExprAst> body) : varKey(std::move(varKey)), variables(std::move(variables)),
                                                body(std::move(body)) {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;

    Position getLocation() override {
        return varKey.loc.start;
    }
};

class PrototypeAst {
public:
    Identifier identifier;
    std::vector<Identifier> args;
    std::optional<uint8_t> precedence;

    PrototypeAst(Identifier identifier, std::vector<Identifier> args,
                 std::optional<uint8_t> precedence = std::nullopt) : identifier{std::move(identifier)},
                                                                     args{std::move(args)},
                                                                     precedence(std::move(precedence)) {
    }


    llvm::Value *gen_code(CodeGenContext &ctx);
};

class DefinitionAst {
public:
    PrototypeAst prototype;
    std::unique_ptr<ExprAst> body;

    DefinitionAst(PrototypeAst prototype, std::unique_ptr<ExprAst> body) : prototype{std::move(prototype)},
                                                                           body{std::move(body)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx);
};

class ExternAst {
public:
    PrototypeAst prototype;

    explicit ExternAst(PrototypeAst prototype) : prototype{std::move(prototype)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx);
};

#endif //PRACTICE_KALEIDOSCOPE_AST_HPP
