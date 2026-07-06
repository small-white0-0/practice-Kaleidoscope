#include <cassert>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

class CharStream {
public:
    virtual ~CharStream() = default;

    /// 获取当前留的第一个字符，但是不消耗字符。
    virtual std::optional<char> peek() =0;

    /// 获取并消耗当前流第一个字符。
    virtual std::optional<char> next() =0;

    /// 获取被消耗的字符的最后一个。
    virtual std::optional<char> last() =0;
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

private:
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
    return {c};
}

std::optional<char> InputCharStream::last() {
    return this->prev;
}

/*
 * Token的解析部分
 */
struct Def {
public:
    bool operator==(const Def &) const {
        return true;
    }
};

struct Extern {
public:
    bool operator==(const Extern &) const {
        return true;
    }
};

struct Identifier {
    std::string name;

public:
    bool operator==(const Identifier &other) const {
        return this->name == other.name;
    }
};

struct Number {
    long long int value;
    long long int div;

public:
    Number(long long int value, long long int div) : value{value}, div(div) {
    }

    bool operator==(const Number &other) const = default;
};

using Token = std::variant<Def, Extern, Identifier, Number, char>;

class TokenStream {
    std::unique_ptr<CharStream> stream;
    std::optional<Token> cur;
    std::optional<Token> last;

public:
    explicit TokenStream(std::unique_ptr<CharStream> stream) : stream(std::move(stream)) {
        // 解析首个token,填充cur.
        nextToken();
    }

    /// 读取并消耗首个分析到的token
    std::optional<Token> nextToken();

    /// 读取不消耗首个token
    std::optional<Token> peekToken() {
        return this->cur;
    }

    /// 获取到上一个token
    std::optional<Token> lastToken() {
        return this->last;
    }
};

// NOLINTNEXTLINE(readability-make-member-function-const)
std::optional<Token> TokenStream::nextToken() {
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
        if (cur) {
            last = std::move(cur);
            cur = std::nullopt;
            return last;
        }
        return cur;
    }
    std::optional<Token> next;
    // [a-zA-Z][a-zA-Z0-9]*
    if (isalpha(stream->peek().value_or('\0'))) {
        std::string id;

        id.push_back(stream->next().value());
        while (isalnum(stream->peek().value_or('\0'))) {
            id.push_back(stream->next().value());
        }
        // 处理关键字
        if (id == "def") {
            next = Def{};
        } else if (id == "extern") {
            next = Extern{};
        } else {
            next = Identifier{id};
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
        next = Number{value, div};
    } else {
        // 非空白的任意单个字符
        next = stream->next().value();
    }
    assert(next.has_value());
    last = std::move(cur);
    cur = std::move(next);
    return last;
}

/*
 * 语法解析部分，输出ast
 */

static auto GLOBAL_BINARY_OPS = std::map<char, int>();

class ExprAst {
public:
    virtual ~ExprAst() = default;
};

class NumberExprAst final : public ExprAst {
    Number number;

public:
    explicit NumberExprAst(Number number) : number{number} {
    }
};

class VarExprAst final : public ExprAst {
    Identifier identifier;

public:
    explicit VarExprAst(Identifier identifier) : identifier{std::move(identifier)} {
    }
};

class BinaryExprAst final : public ExprAst {
    char ops;
    std::unique_ptr<ExprAst> left, right;

public:
    BinaryExprAst(char op, std::unique_ptr<ExprAst> left,
                  std::unique_ptr<ExprAst> right) : ops{op}, left{std::move(left)}, right{std::move(right)} {
    }
};

class CallExprAst final : public ExprAst {
    Identifier identifier;
    std::vector<std::unique_ptr<ExprAst> > args;

public:
    CallExprAst(Identifier identifier, std::vector<std::unique_ptr<ExprAst> > args) : identifier{
            std::move(identifier)
        },
        args{std::move(args)} {
    }
};

class PrototypeAst {
    Identifier identifier;
    std::vector<Identifier> args;

public:
    PrototypeAst(Identifier identifier, std::vector<Identifier> args) : identifier{std::move(identifier)},
                                                                        args{std::move(args)} {
    }
};

class DefinitionAst {
    PrototypeAst prototype;
    std::unique_ptr<ExprAst> body;

public:
    DefinitionAst(PrototypeAst prototype, std::unique_ptr<ExprAst> body) : prototype{std::move(prototype)},
                                                                           body{std::move(body)} {
    }
};

class ExternAst {
    PrototypeAst prototype;

public:
    explicit ExternAst(PrototypeAst prototype) : prototype{std::move(prototype)} {
    }
};

template<typename T, typename... Ts>
T &expect(std::optional<std::variant<Ts...> > &opt, const std::string &msg) {
    if (!opt || !std::holds_alternative<T>(*opt))
        throw std::runtime_error(msg);
    return std::get<T>(*opt);
}

template<typename T, typename... Ts>
T &&expect(std::optional<std::variant<Ts...> > &&opt, const std::string &msg) {
    if (!opt || !std::holds_alternative<T>(*opt))
        throw std::runtime_error(msg);
    return std::get<T>(std::move(*opt)); // 移动
}

template<typename T, typename... Ts>
bool expect_is(const std::optional<std::variant<Ts...> > &opt, const T &compare_obj) {
    if (!opt || !std::holds_alternative<T>(*opt))
        return false;
    return std::get<T>(*opt) == compare_obj;
}

std::unique_ptr<ExprAst> parseExpr(TokenStream &stream);

std::unique_ptr<ExprAst> parsePrimaryExpr(TokenStream &stream) {
    std::optional<Token> first_token = stream.nextToken();
    if (!first_token) {
        throw std::runtime_error{"not a token when parse primary expr"};
    }
    if (std::holds_alternative<Number>(first_token.value())) {
        return std::make_unique<NumberExprAst>(std::get<Number>(first_token.value()));
    } else if (std::holds_alternative<Identifier>(first_token.value())) {
        auto id = std::get<Identifier>(first_token.value());
        // only identifier
        if (!expect_is(stream.peekToken(), '(')) {
            return std::make_unique<VarExprAst>(id);
        }
        // for call function
        stream.nextToken(); // eat (
        std::vector<std::unique_ptr<ExprAst> > args;
        while (true) {
            // end args list
            if (expect_is(stream.peekToken(), ')')) {
                stream.nextToken(); // eat )
                break;
            }
            // parse arg
            args.push_back(parseExpr(stream));
            // , or )
            if (expect_is(stream.peekToken(), ',')) {
                stream.nextToken(); // eat ,
            } else if (!expect_is(stream.peekToken(), ')')) {
                throw std::runtime_error{"Expected ',' or ')' after arg in call funtion."};
            }
        }
        return std::make_unique<CallExprAst>(id, std::move(args));
    } else if (expect_is(first_token, '(')) {
        auto expr = parseExpr(stream);
        if (!expect_is(stream.peekToken(), ')')) {
            throw std::runtime_error{"Expected ')' after expression in Paren."};
        }
        stream.nextToken(); // eat )
        return expr;
    } else {
        throw std::runtime_error{"unknown token"};
    }
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
        if (!may_op || !std::holds_alternative<char>(may_op.value())) {
            // not binary expr
            return left;
        }
        auto op = std::get<char>(may_op.value());
        // check priority
        if (prePriority >= get_priority(op)) {
            return left;
        }
        // eat op
        stream.nextToken();
        // right primary expr
        auto right = parsePrimaryExpr(stream);
        right = tryParseBinaryExpr(get_priority(op), std::move(right), stream);
        left = std::make_unique<BinaryExprAst>(op, std::move(left), std::move(right));
    }
}

std::unique_ptr<ExprAst> parseExpr(TokenStream &stream) {
    auto primaryExpr = parsePrimaryExpr(stream);
    return tryParseBinaryExpr(0, std::move(primaryExpr), stream);
}

PrototypeAst parsePrototype(TokenStream &stream) {
    auto name = expect<Identifier>(stream.nextToken(), "Expect identifier as prototype name.");
    std::vector<Identifier> args;

    // 消耗'('
    if (expect<char>(stream.nextToken(), "expect char") != '(') {
        throw std::runtime_error{"Expected '('"};
    }
    while (true) {
        // 处理 ')'
        auto token = stream.nextToken();
        if (!token) {
            throw std::runtime_error{"Expected identifier or ), but got 'EOF'"};
        }
        if (std::holds_alternative<char>(token.value()) && std::get<char>(token.value()) == ')') {
            // end args list.
            break;
        }
        args.push_back(expect<Identifier>(token, "Expect identifier"));
        // , or )
        if (expect_is(stream.peekToken(), ',')) {
            stream.nextToken(); // eat ,
        } else if (!expect_is(stream.peekToken(), ')')) {
            throw std::runtime_error{"Expected ',' or ')' after arg in prototype."};
        }
    }
    return {std::move(name), std::move(args)};
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
        auto proto = PrototypeAst{
            {"__anon_expr"},
            std::move(std::vector<Identifier>())
        };
        return DefinitionAst{proto, std::move(E)};
    }
    throw std::runtime_error{"Expected '__anon_expr'"};
}

void mainLoop(TokenStream &stream) {
    while (true) {
        auto first_token = stream.peekToken();
        if (!first_token.has_value()) {
            return;
        }
        if (expect_is(first_token, ';')) {
            stream.nextToken();
        } else if (expect_is(first_token, Def{})) {
            stream.nextToken(); // eat def
            auto def = parseDefinition(stream);
        } else if (expect_is(first_token, Extern{})) {
            stream.nextToken(); // eat extern
            auto extern_statement = parseExtern(stream);
        } else {
            auto top_level_expr = parseTopLevelExpr(stream);
        }
    }
}

int main() {
    auto stream = TokenStream{std::move(std::make_unique<InputCharStream>())};
    try {
        mainLoop(stream);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
