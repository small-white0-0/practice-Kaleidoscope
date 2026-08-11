#include <cassert>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>
#include <vector>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/PassManager.h"                    // TheFPM
#include "llvm/Transforms/Scalar/LoopPassManager.h"  // TheLAM
#include "llvm/Analysis/CGSCCPassManager.h"        // TheCGAM
#include "llvm/Passes/PassBuilder.h"                 // PassBuilder
#include "llvm/Passes/StandardInstrumentations.h"  // StandardInstrumentations

#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

class CodeGenContext;

struct Position {
    std::string file;
    int line;
    int column;

    bool operator==(const Position &) const = default;
};

class CharStream {
public:
    virtual ~CharStream() = default;

    /// 获取当前留的第一个字符，但是不消耗字符。
    virtual std::optional<char> peek() =0;

    /// 获取并消耗当前流第一个字符。
    virtual std::optional<char> next() =0;

    /// 获取被消耗的字符的最后一个。
    virtual std::optional<char> last() =0;

    /// 获取当前的第一个字符的位置。
    virtual Position postion() =0;
};

/// 需要保证cin不被使用，属于测试用，不安全。
class InputCharStream final : public CharStream {
public:
    InputCharStream() = default;

    InputCharStream(InputCharStream const &) = delete;

    InputCharStream &operator=(InputCharStream const &) = delete;

    InputCharStream(InputCharStream &&) = default;

    InputCharStream &operator=(InputCharStream &&) = default;

    std::optional<char> peek() override;

    std::optional<char> next() override;

    std::optional<char> last() override;

    Position postion() override;

private:
    Position pos = {"cin", 0, 0};
    std::optional<char> prev = std::nullopt;
};

std::optional<char> InputCharStream::peek() {
    auto c = std::cin.peek();
    if (c == EOF) {
        return std::nullopt;
    }
    return {c};
}

std::optional<char> InputCharStream::next() {
    auto c = std::cin.get();
    if (c == EOF) {
        return std::nullopt;
    }
    if (c == '\n') {
        this->pos.line++;
        this->pos.column = 0;
    } else {
        this->pos.column++;
    }
    return {c};
}

std::optional<char> InputCharStream::last() {
    return this->prev;
}

Position InputCharStream::postion() {
    return this->pos;
}

/*
 * Token的解析部分
 */
struct Location {
    Position start;
    Position end;

    Location(Position &&start, Position &&end) : start{start}, end{end} {
    }

    bool operator==(const Location &) const = default;
};

struct Def {
    Location loc;

public:
    bool operator==(const Def &) const {
        return true;
    }
};

struct Extern {
    Location loc;

public:
    bool operator==(const Extern &) const {
        return true;
    }
};

struct If {
    Location loc;
    bool operator==(const If &) const { return true; }
};

struct Then {
    Location loc;
    bool operator==(const Then &) const { return true; }
};

struct Else {
    Location loc;
    bool operator==(const Else &) const { return true; }
};

struct For {
    Location loc;
    bool operator==(const For &) const { return true; }
};

struct In {
    Location loc;
    bool operator==(const In &) const { return true; }
};

struct Unary {
    Location loc;
    bool operator==(const Unary &) const { return true; }
};

struct Binary {
    Location loc;
    bool operator==(const Binary &) const { return true; }
};

struct Identifier {
    std::string name;
    Location loc;

public:
    bool operator==(const Identifier &other) const {
        return this->name == other.name;
    }
};

struct Number {
    long long int value;
    long long int div;
    Location loc;

public:
    Number(const long long int value, const long long int div, Location &&loc) : value{value}, div{div},
        loc(std::move(loc)) {
        assert(div > 0);
    }

    [[nodiscard]] double getValue() const {
        return static_cast<double>(value) / static_cast<double>(div);
    }

    bool operator==(const Number &other) const = default;
};

struct Char {
    char value;
    Location loc;

    Char(char value, Location &&loc) : value{value}, loc{loc} {
    }
};

using Token = std::variant<std::monostate, Def, Extern, If, Then, Else, For, In, Unary, Binary, Identifier, Number,
    Char>;

class TokenStream {
    std::unique_ptr<CharStream> stream;
    Token cur = {};
    Token last = {};

public:
    explicit TokenStream(std::unique_ptr<CharStream> stream) : stream(std::move(stream)) {
        // 解析首个token,填充cur.
        nextToken();
    }

    /// 读取并消耗首个分析到的token
    Token nextToken();

    /// 读取不消耗首个token
    Token peekToken() {
        return this->cur;
    }

    /// 获取到上一个token
    Token lastToken() {
        return this->last;
    }
};

// NOLINTNEXTLINE(readability-make-member-function-const)
void a() {
}

Token TokenStream::nextToken() {
    /**
     * 这个函数内是先解析下一个token,然后将cur指向的token作为返回结果。
     * 同时会更新cur为next,last更新为cur.
     * 如果next是空，且cur是空，则不会更新cur和last.
     */
    // 消耗前导space
    while (isspace(static_cast<unsigned char>(stream->peek().value_or('X')))) {
        stream->next();
    }
    // 无后续输入
    if (!stream->peek()) {
        if (!std::holds_alternative<std::monostate>(cur)) {
            last = std::move(cur);
            cur = {};
            return last;
        }
        return cur;
    }
    Token next;
    Position start = this->stream->postion();
    // [a-zA-Z][a-zA-Z0-9]*
    if (isalpha(stream->peek().value_or('\0'))) {
        std::string id;

        id.push_back(stream->next().value());
        while (isalnum(stream->peek().value_or('\0'))) {
            id.push_back(stream->next().value());
        }
        // 处理关键字
        auto loc = Location(std::move(start), stream->postion());
        if (id == "def") {
            next = Def{loc};
        } else if (id == "extern") {
            next = Extern{loc};
        } else if (id == "if") {
            next = If{loc};
        } else if (id == "then") {
            next = Then{loc};
        } else if (id == "else") {
            next = Else{loc};
        } else if (id == "for") {
            next = For{loc};
        } else if (id == "in") {
            next = In{loc};
        } else if (id == "unary") {
            next = Unary{loc};
        } else if (id == "binary") {
            next = Binary{loc};
        } else {
            next = Identifier{id, loc};
        }
    } else if (isdigit(stream->peek().value_or('\0'))) {
        // [0-9]+\.[0-9]*
        long long int value = stream->next().value() - '0';
        long long div = 1;
        while (isdigit(stream->peek().value_or('\0'))) {
            value = value * 10 + (stream->next().value() - '0');
        }
        if (stream->peek().value_or('\0') == '.') {
            stream->next();
            while (isdigit(stream->peek().value_or('\0'))) {
                value = value * 10 + (stream->next().value() - '0');
                div *= 10;
            }
        }
        auto loc = Location(std::move(start), stream->postion());
        next = Number{value, div, (std::move(loc))};
    } else {
        // 非空白的任意单个字符
        auto loc = Location(std::move(start), stream->postion());
        next = Char{stream->next().value(), std::move(loc)};
    }
    assert(!std::holds_alternative<std::monostate>(next));
    last = std::move(cur);
    cur = std::move(next);
    if (const auto cur_p = std::get_if<Char>(&cur);
        cur_p != nullptr && cur_p->value == ';') {
        auto has_comment = false;
        while (stream->peek().value_or('\n') != '\n') {
            if (!has_comment) {
                std::cout << "comment: ";
                has_comment = true;
            }
            auto c = stream->next();
            std::cout << c.value();
        }
        if (has_comment) std::cout << '\n';
    }
    return last;
}

/*
 * 语法解析部分，输出ast
 */

static auto GLOBAL_BINARY_OPS = std::map<char, int>();

class ExprAst {
public:
    virtual ~ExprAst() = default;

    virtual llvm::Value *gen_code(CodeGenContext &ctx) =0;
};

class NumberExprAst final : public ExprAst {
public:
    Number number;

    explicit NumberExprAst(Number number) : number{std::move(number)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;
};

class VarExprAst final : public ExprAst {
public:
    Identifier identifier;

    explicit VarExprAst(Identifier identifier) : identifier{std::move(identifier)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;
};

class BinaryExprAst final : public ExprAst {
public:
    char ops;
    std::unique_ptr<ExprAst> left, right;

    BinaryExprAst(char op, std::unique_ptr<ExprAst> left,
                  std::unique_ptr<ExprAst> right) : ops{op}, left{std::move(left)}, right{std::move(right)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;
};

class UnaryExprAst final : public ExprAst {
public:
    Char ops;
    std::unique_ptr<ExprAst> operand;

    UnaryExprAst(Char op, std::unique_ptr<ExprAst> operand) : ops{std::move(op)}, operand{std::move(operand)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;
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
};

class IfExprAst final : public ExprAst {
public:
    std::unique_ptr<ExprAst> condition;
    std::unique_ptr<ExprAst> thenExpr;
    std::unique_ptr<ExprAst> elseExpr;

    IfExprAst(std::unique_ptr<ExprAst> condition, std::unique_ptr<ExprAst> thenExpr,
              std::unique_ptr<ExprAst> elseExpr) : condition{
                                                       std::move(condition)
                                                   }, thenExpr{std::move(thenExpr)}, elseExpr{std::move(elseExpr)} {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;
};

class ForExprAst final : public ExprAst {
public:
    Identifier var;
    Number init;
    std::unique_ptr<ExprAst> endCondition;
    std::optional<Number> step;
    std::unique_ptr<ExprAst> body;

    ForExprAst(Identifier var, Number init,
               std::unique_ptr<ExprAst> endCondition,
               std::optional<Number> step, std::unique_ptr<ExprAst> body)
        : var(std::move(var)), init(std::move(init)), endCondition(std::move(endCondition)),
          step(std::move(step)), body(std::move(body)) {
    }

    llvm::Value *gen_code(CodeGenContext &ctx) override;
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

template<typename T, typename... Ts>
T &expect(std::variant<Ts...> &opt, const std::string &msg) {
    if (!std::holds_alternative<T>(opt))
        throw std::runtime_error(msg);
    return std::get<T>(opt);
}

template<typename T, typename... Ts>
T &&expect(std::variant<Ts...> &&opt, const std::string &msg) {
    if (!std::holds_alternative<T>(opt))
        throw std::runtime_error(msg);
    return std::get<T>(std::move(opt)); // 移动
}

bool isChar(const Token &tok, const char tc) {
    if (auto *c = std::get_if<Char>(&tok))
        return c->value == tc;
    return false;
}


std::unique_ptr<ExprAst> parseExpr(TokenStream &stream);

std::unique_ptr<ExprAst> parsePrimaryExpr(TokenStream &stream) {
    auto first_token = stream.nextToken();
    if (std::holds_alternative<std::monostate>(first_token)) {
        throw std::runtime_error{"not a token when parse primary expr"};
    }
    if (std::holds_alternative<Number>(first_token)) {
        return std::make_unique<NumberExprAst>(std::get<Number>(first_token));
    } else if (std::holds_alternative<Identifier>(first_token)) {
        auto id = std::get<Identifier>(first_token);
        // only identifier
        if (!isChar(stream.peekToken(), '(')) {
            return std::make_unique<VarExprAst>(id);
        }
        // for call function
        stream.nextToken(); // eat (
        std::vector<std::unique_ptr<ExprAst> > args;
        while (true) {
            // end args list
            if (isChar(stream.peekToken(), ')')) {
                stream.nextToken(); // eat )
                break;
            }
            // parse arg
            args.push_back(parseExpr(stream));
            // , or )
            if (isChar(stream.peekToken(), ',')) {
                stream.nextToken(); // eat ,
            } else if (!isChar(stream.peekToken(), ')')) {
                throw std::runtime_error{"Expected ',' or ')' after arg in call funtion."};
            }
        }
        return std::make_unique<CallExprAst>(id, std::move(args));
    } else if (isChar(first_token, '(')) {
        auto expr = parseExpr(stream);
        if (!isChar(stream.peekToken(), ')')) {
            throw std::runtime_error{"Expected ')' after expression in Paren."};
        }
        stream.nextToken(); // eat )
        return expr;
    } else {
        throw std::runtime_error{"unknown token"};
    }
}

std::unique_ptr<IfExprAst> parseIfExpr(TokenStream &stream) {
    // eat 'if' token
    stream.nextToken();
    auto cond = parseExpr(stream);
    // only eat 'then'. if nextToken isn't 'then', throw error.
    if (!std::holds_alternative<Then>(stream.nextToken())) {
        throw std::runtime_error{"Expected 'then' keyword."};
    }
    auto thenExpr = parseExpr(stream);
    if (!std::holds_alternative<Else>(stream.nextToken())) {
        throw std::runtime_error{"Expected 'Else' keyword."};
    }
    auto elseExpr = parseExpr(stream);
    return std::make_unique<IfExprAst>(std::move(cond), std::move(thenExpr), std::move(elseExpr));
}

std::unique_ptr<ForExprAst> parseForExpr(TokenStream &stream) {
    // eat for keyword
    stream.nextToken();
    // variable
    if (!std::holds_alternative<Identifier>(stream.peekToken())) {
        throw std::runtime_error{"Expected 'identifier' after 'for'."};
    }
    auto variable = std::get<Identifier>(stream.nextToken());
    // eat just and only '='
    if (!isChar(stream.nextToken(), '=')) {
        throw std::runtime_error{"Expected '='"};
    }
    if (!std::holds_alternative<Number>(stream.peekToken())) {
        throw std::runtime_error{"Expected 'number' for variable statement."};
    }
    auto init = std::get<Number>(stream.nextToken());
    if (!isChar(stream.nextToken(), ',')) {
        throw std::runtime_error{"Expected ','"};
    }
    auto endCondition = parseExpr(stream);
    std::optional<Number> step = std::nullopt;
    // 可选的step
    if (isChar(stream.peekToken(), ',')) {
        // 有step
        stream.nextToken(); // eat ,
        if (!std::holds_alternative<Number>(stream.peekToken())) {
            throw std::runtime_error{"Expected 'number' for step"};
        }
        step = std::get<Number>(stream.nextToken());
    }
    if (!std::holds_alternative<In>(stream.nextToken())) {
        throw std::runtime_error{"Expected 'in'"};
    }
    auto body = parseExpr(stream);
    return std::make_unique<ForExprAst>(std::move(variable), std::move(init), std::move(endCondition),
                                        std::move(step), std::move(body));
}

std::unique_ptr<UnaryExprAst> parseUnaryExpr(TokenStream &stream) {
    auto op = std::get<Char>(stream.nextToken());
    auto primaryExpr = parsePrimaryExpr(stream);
    return std::make_unique<UnaryExprAst>(std::move(op), std::move(primaryExpr));
}

std::unique_ptr<ExprAst> tryParseBinaryExpr(const int prePriority, std::unique_ptr<ExprAst> left, TokenStream &stream) {
    auto get_priority = [](const char op) {
        if (const auto priority = GLOBAL_BINARY_OPS.find(op); priority != GLOBAL_BINARY_OPS.end()) {
            return priority->second;
        }
        return 0;
    };
    while (true) {
        auto may_op = stream.peekToken();
        if (!std::holds_alternative<Char>(may_op)) {
            // not binary expr or unary expr
            return left;
        }
        auto op = std::get<Char>(may_op).value;
        // check priority
        if (prePriority >= get_priority(op)) {
            return left;
        }
        // eat op
        stream.nextToken();
        // right primary expr
        std::unique_ptr<ExprAst> right;
        auto curToken = stream.peekToken();
        if (std::holds_alternative<Char>(curToken) && !isChar(curToken, '(') && !isChar(curToken, ')')) {
            // this should be a unary expr
            right = parseUnaryExpr(stream);
        } else {
            right = parsePrimaryExpr(stream);
        }
        right = tryParseBinaryExpr(get_priority(op), std::move(right), stream);
        left = std::make_unique<BinaryExprAst>(op, std::move(left), std::move(right));
    }
}

std::unique_ptr<ExprAst> parseExpr(TokenStream &stream) {
    auto firstToken = stream.peekToken();
    if (std::holds_alternative<If>(firstToken)) {
        return parseIfExpr(stream);
    }
    if (std::holds_alternative<For>(firstToken)) {
        return parseForExpr(stream);
    }
    // if 'char' start, it should be a unary operate.
    std::unique_ptr<ExprAst> left;
    if (std::holds_alternative<Char>(firstToken) && !isChar(firstToken, '(') && !isChar(firstToken, ')')) {
        left = parseUnaryExpr(stream);
    } else {
        left = parsePrimaryExpr(stream);
    }
    return tryParseBinaryExpr(0, std::move(left), stream);
}

PrototypeAst parsePrototype(TokenStream &stream) {
    // 处理binary和unary
    std::unique_ptr<Identifier> name;
    std::optional<uint8_t> precedence = std::nullopt;
    int8_t kind = 0; // 0 is common, 1 is unary, 2 is binary.
    auto firstToken = stream.nextToken();
    if (std::holds_alternative<Binary>(firstToken)) {
        auto loc = std::get<Binary>(firstToken).loc;
        auto op = expect<Char>(stream.nextToken(), "Expect single char as operate.").value;
        if (std::holds_alternative<Number>(stream.peekToken())) {
            auto mayPrecedence = expect<Number>(stream.nextToken(), "Expect single number as precedence.");
            if (mayPrecedence.div != 1 || mayPrecedence.getValue() >= 100) {
                throw std::runtime_error{"Expect precedence number is integer and 0~100."};
            }
            precedence = static_cast<uint8_t>(mayPrecedence.getValue());
        } else {
            precedence = 99;
        }
        name = std::make_unique<Identifier>(Identifier({std::string{"binary"} + op}, loc));
        kind = 2;
    } else if (std::holds_alternative<Unary>(firstToken)) {
        const auto loc = std::get<Unary>(firstToken).loc;
        const auto op = expect<Char>(stream.nextToken(), "Expect single char as operate.").value;
        name = std::make_unique<Identifier>(Identifier({std::string{"unary"} + op}, loc));
        kind = 1;
    } else if (std::holds_alternative<Identifier>(firstToken)) {
        name = std::make_unique<Identifier>(std::get<Identifier>(firstToken));
    } else {
        throw std::runtime_error{"Expected identifier or binary, unary operator."};
    }
    std::vector<Identifier> args;
    // 消耗'('
    if (expect<Char>(stream.nextToken(), "expect char").value != '(') {
        throw std::runtime_error{"Expected '('"};
    }
    while (true) {
        // 处理 ')'
        auto token = stream.nextToken();
        if (std::holds_alternative<std::monostate>(token)) {
            throw std::runtime_error{"Expected identifier or ), but got 'EOF'"};
        }
        if (isChar(token, ')')) {
            // end args list.
            break;
        }
        args.push_back(expect<Identifier>(token, "Expect identifier"));
        // , or )
        if (isChar(stream.peekToken(), ',')) {
            stream.nextToken(); // eat ,
        } else if (!isChar(stream.peekToken(), ')')) {
            throw std::runtime_error{"Expected ',' or ')' after arg in prototype."};
        }
    }
    // 检查参数数量
    if (kind == 1 && args.size() != 1) {
        throw std::runtime_error{"unary function should have and only have one argument."};
    }
    if (kind == 2 && args.size() != 2) {
        throw std::runtime_error{"binary function should have and only have two arguments."};
    }
    return {std::move(*name), std::move(args), precedence};
}

DefinitionAst parseDefinition(TokenStream &stream) {
    auto prototype = parsePrototype(stream);
    auto body = parseExpr(stream);
    return {prototype, std::move(body)};
}

ExternAst parseExtern(TokenStream &stream) {
    auto prototype = parsePrototype(stream);
    return ExternAst{
        std::move(prototype)
    };
}

DefinitionAst parseTopLevelExpr(TokenStream &stream) {
    if (auto E = parseExpr(stream)) {
        // Make an anonymous proto.
        Location loc = {Position{"__anon_expr_file", 0, 0}, Position{"__anon_expr_file", 0, 0}};
        auto proto = PrototypeAst{
            Identifier{"__anon_expr", loc},
            std::move(std::vector<Identifier>())
        };
        return DefinitionAst{proto, std::move(E)};
    }
    throw std::runtime_error{"Expected '__anon_expr'"};
}

/**
 * 翻译为llvm ir部分
 */
class CodeGenContext {
public:
    // 核心 LLVM 对象
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<> > Builder;

    // llvm pass 对象
    std::unique_ptr<llvm::FunctionPassManager> TheFPM;

    std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
    std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
    std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
    std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
    std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
    std::unique_ptr<llvm::StandardInstrumentations> TheSI;


    // 符号表
    std::vector<std::map<std::string, llvm::Value *> > NamedValuesStack;

    // 二元算符运算表
    std::map<char, std::function<llvm::Value*(llvm::Value *, llvm::Value *, CodeGenContext &)> > BinOpMap;

    CodeGenContext() {
        TheContext = std::make_unique<llvm::LLVMContext>();
        TheModule = std::make_unique<llvm::Module>("my_module", *TheContext);
        Builder = std::make_unique<llvm::IRBuilder<> >(*TheContext);

        TheFPM = std::make_unique<llvm::FunctionPassManager>();
        TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();
        TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
        TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
        TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
        ThePIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
        TheSI = std::make_unique<llvm::StandardInstrumentations>(*TheContext,/*debugging*/true);

        TheSI->registerCallbacks(*ThePIC, TheMAM.get());

        // Register analysis passes
        llvm::PassBuilder PB;
        PB.registerModuleAnalyses(*TheMAM);
        PB.registerFunctionAnalyses(*TheFAM);
        PB.registerLoopAnalyses(*TheLAM);
        PB.registerCGSCCAnalyses(*TheCGAM);
        PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);

        // 添加kaleidoscope教程中给的一些优化pass
        // Do simple "peephole" optimizations and bit-twiddling optzns.
        TheFPM->addPass(llvm::InstCombinePass());
        // Reassociate expressions.
        TheFPM->addPass(llvm::ReassociatePass());
        // Eliminate Common SubExpressions.
        TheFPM->addPass(llvm::GVNPass());
        // Simplify the control flow graph (deleting unreachable blocks, etc).
        TheFPM->addPass(llvm::SimplifyCFGPass());
    }

    void EnterScope() { NamedValuesStack.emplace_back(); }
    void ExitScope() { NamedValuesStack.pop_back(); }

    void setBinOp(const char op,
                  const std::function<llvm::Value *(llvm::Value *, llvm::Value *, CodeGenContext &)> &fn) {
        BinOpMap[op] = fn;
    }

    bool addName(const std::string &name, llvm::Value *value) {
        auto &cur_scope = NamedValuesStack.back();
        if (const auto it = cur_scope.find(name); it != cur_scope.end()) {
            return false;
        }
        cur_scope[name] = value;
        return true;
    }

    llvm::Value *LookupName(const std::string &Name);
};

llvm::Value *CodeGenContext::LookupName(const std::string &Name) {
    for (const auto &nameValues: std::ranges::reverse_view(NamedValuesStack)) {
        if (const auto it = nameValues.find(Name); it != nameValues.end()) {
            return it->second;
        }
    }
    return nullptr;
}


llvm::Value *DefinitionAst::gen_code(CodeGenContext &ctx) {
    ctx.EnterScope();
    auto fun = ctx.TheModule->getFunction(this->prototype.identifier.name);
    if (!fun) {
        fun = llvm::dyn_cast<llvm::Function>(this->prototype.gen_code(ctx));
    } else if (fun->arg_size() != this->prototype.args.size()) {
        throw std::runtime_error{"参数不匹配"};
    } else if (!fun->empty()) {
        throw std::runtime_error{"重复定义"};
    }

    auto enter_block = llvm::BasicBlock::Create(*ctx.TheContext, "entry", fun);
    ctx.Builder->SetInsertPoint(enter_block);

    for (auto &arg: fun->args()) {
        ctx.addName(std::string(arg.getName()), &arg);
    }

    llvm::Value *retValue = this->body->gen_code(ctx);
    ctx.Builder->CreateRet(retValue);
    llvm::verifyFunction(*fun);
    // 调用pass进行优化
    ctx.TheFPM->run(*fun, *ctx.TheFAM);
    ctx.ExitScope();

    // 如果是binary操作符自定义函数，需要注册到BinOpMap中
    if (this->prototype.precedence) {
        std::string name = this->prototype.identifier.name;
        char op = name.back();
        ctx.setBinOp(op, [name](auto l, auto r, CodeGenContext &ctx) {
            auto fun = ctx.TheModule->getFunction(name);
            if (!fun) {
                throw std::runtime_error{"Unknow error for lack binary definition "};
            }
            return static_cast<llvm::Value *>(ctx.Builder->CreateCall(fun, std::vector{l, r}));
        });
    }
    return fun;
}

llvm::Value *ExternAst::gen_code(CodeGenContext &ctx) {
    return this->prototype.gen_code(ctx);
}


llvm::Value *PrototypeAst::gen_code(CodeGenContext &ctx) {
    // prototype only : double(double...)
    std::vector args_type{this->args.size(), llvm::Type::getDoubleTy(*ctx.TheContext)};
    auto fun_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(*ctx.TheContext), args_type, false);
    auto fun = llvm::Function::Create(fun_type,
                                      llvm::Function::ExternalLinkage,
                                      this->identifier.name,
                                      ctx.TheModule.get());
    unsigned Idx = 0;
    for (auto &arg: fun->args())
        arg.setName(this->args[Idx++].name);

    return fun;
}

llvm::Value *NumberExprAst::gen_code(CodeGenContext &ctx) {
    return llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(this->number.getValue()));
}


llvm::Value *VarExprAst::gen_code(CodeGenContext &ctx) {
    auto v = ctx.LookupName(identifier.name);
    if (!v) {
        throw std::runtime_error{"引用变量不存在"};
    }
    return v;
}


llvm::Value *BinaryExprAst::gen_code(CodeGenContext &ctx) {
    auto l = left->gen_code(ctx);
    auto r = right->gen_code(ctx);
    if (!l || !r) {
        throw std::runtime_error{"binary lacked."};
    }
    if (ctx.BinOpMap[ops]) {
        return ctx.BinOpMap[ops](l, r, ctx);
    }
    throw std::runtime_error{"binary gen failed."};
}

llvm::Value *UnaryExprAst::gen_code(CodeGenContext &ctx) {
    const auto funName = std::string("unary") + this->ops.value;
    const auto fun = ctx.TheModule->getFunction(funName);
    if (!fun) {
        throw std::runtime_error{"Unknown unary referenced"};
    }
    const auto arg = this->operand->gen_code(ctx);

    return ctx.Builder->CreateCall(fun, arg, "unarycall");
}

llvm::Value *IfExprAst::gen_code(CodeGenContext &ctx) {
    // entry, for condition
    auto condV = this->condition->gen_code(ctx);
    condV = ctx.Builder->CreateFCmpONE(condV, llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(0.0)), "ifcond");
    // 这里就先这么写了，下一个练手项目再做好scope的管理。
    auto fun = ctx.Builder->GetInsertBlock()->getParent();
    auto thenBB = llvm::BasicBlock::Create(*ctx.TheContext, "thenBB");
    auto elseBB = llvm::BasicBlock::Create(*ctx.TheContext, "elseBB");
    auto mergeBB = llvm::BasicBlock::Create(*ctx.TheContext, "ifcont");

    ctx.Builder->CreateCondBr(condV, thenBB, elseBB);

    //then basic block
    fun->insert(fun->end(), thenBB);
    ctx.Builder->SetInsertPoint(thenBB);
    auto thenV = this->thenExpr->gen_code(ctx);
    ctx.Builder->CreateBr(mergeBB);
    // because thenExpr gencode may change the basic block
    auto thenBBOut = ctx.Builder->GetInsertBlock();

    // else basic block
    fun->insert(fun->end(), elseBB);
    ctx.Builder->SetInsertPoint(elseBB);
    auto elseV = this->elseExpr->gen_code(ctx);
    ctx.Builder->CreateBr(mergeBB);
    auto elseBBOut = ctx.Builder->GetInsertBlock();

    // if merge out
    fun->insert(fun->end(), mergeBB);
    ctx.Builder->SetInsertPoint(mergeBB);
    auto pn = ctx.Builder->CreatePHI(llvm::Type::getDoubleTy(*ctx.TheContext), 2, "iftemp");
    pn->addIncoming(thenV, thenBBOut);
    pn->addIncoming(elseV, elseBBOut);
    return pn;
}

llvm::Value *ForExprAst::gen_code(CodeGenContext &ctx) {
    ctx.EnterScope();
    auto fun = ctx.Builder->GetInsertBlock()->getParent();
    auto preLoopBB = ctx.Builder->GetInsertBlock();
    auto loopBB = llvm::BasicBlock::Create(*ctx.TheContext, "loopBB");
    auto loopEndBB = llvm::BasicBlock::Create(*ctx.TheContext, "loopEndBB");
    // enter loop
    ctx.Builder->CreateBr(loopBB);
    fun->insert(fun->end(), loopBB);
    ctx.Builder->SetInsertPoint(loopBB);
    // get init value as 循环变量
    auto loopV = ctx.Builder->CreatePHI(llvm::Type::getDoubleTy(*ctx.TheContext),
                                        2, this->var.name);
    ctx.addName(this->var.name, loopV);
    loopV->addIncoming(llvm::ConstantFP::get(llvm::Type::getDoubleTy(*ctx.TheContext), this->init.getValue()),
                       preLoopBB);

    // 计算下一次的变量值
    llvm::Value *nextVar;
    if (this->step) {
        nextVar = ctx.Builder->CreateFAdd(
            loopV, llvm::ConstantFP::get(llvm::Type::getDoubleTy(*ctx.TheContext), this->step->getValue()), "nextVar");
    } else {
        nextVar = ctx.Builder->CreateFAdd(
            loopV, llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(1.0)), "nextVar");
    }
    // 执行表达式
    auto loopE = this->body->gen_code(ctx);
    // 判断是否结束
    auto loopEndCond = this->endCondition->gen_code(ctx);
    loopEndCond = ctx.Builder->CreateFCmpONE(loopEndCond, llvm::ConstantFP::get(*ctx.TheContext, llvm::APFloat(0.0)));
    // 补完loopV的赋值第二部分
    auto loopBBOut = ctx.Builder->GetInsertBlock();
    loopV->addIncoming(nextVar, loopBBOut);
    // jump
    ctx.Builder->CreateCondBr(loopEndCond, loopBB, loopEndBB);
    // loop结束了
    fun->insert(fun->end(), loopEndBB);
    ctx.Builder->SetInsertPoint(loopEndBB);
    auto loopRet = ctx.Builder->CreatePHI(llvm::Type::getDoubleTy(*ctx.TheContext),
                                          1, "loopret");
    loopRet->addIncoming(loopE, loopBBOut);
    ctx.ExitScope();

    return loopRet;
}

llvm::Value *CallExprAst::gen_code(CodeGenContext &ctx) {
    llvm::Function *CalleeF = ctx.TheModule->getFunction(this->identifier.name);
    if (!CalleeF)
        throw std::runtime_error{"Unknown function referenced"};

    // If argument mismatch error.
    if (CalleeF->arg_size() != this->args.size())
        throw std::runtime_error("Incorrect # arguments passed");

    std::vector<llvm::Value *> ArgsV;
    for (unsigned i = 0, e = this->args.size(); i != e; ++i) {
        ArgsV.push_back(this->args[i]->gen_code(ctx));
        if (!ArgsV.back())
            return nullptr;
    }

    return ctx.Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

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
    GLOBAL_BINARY_OPS['<'] = 5;
    GLOBAL_BINARY_OPS['>'] = 5;
    GLOBAL_BINARY_OPS['+'] = 10;
    GLOBAL_BINARY_OPS['-'] = 10;
    GLOBAL_BINARY_OPS['*'] = 20;
    GLOBAL_BINARY_OPS['/'] = 20;
    auto ctx = std::make_unique<CodeGenContext>();
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
    } catch (std::exception &e) {
        std::cerr << "报错信息：" << e.what() << "===================\n" << std::endl;
    }
    ctx->TheModule->print(llvm::errs(), nullptr); //print(llvm::outs());

    return 0;
}
