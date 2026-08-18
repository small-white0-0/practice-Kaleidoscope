#include "parser.hpp"

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
            return std::make_unique<VariableExprAst>(id);
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
    auto ifKey = std::get<If>(stream.nextToken());
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
    return std::make_unique<IfExprAst>(ifKey, std::move(cond), std::move(thenExpr), std::move(elseExpr));
}

std::unique_ptr<ForExprAst> parseForExpr(TokenStream &stream) {
    // eat for keyword
    auto forKey = std::get<For>(stream.nextToken());
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
    return std::make_unique<ForExprAst>(forKey, std::move(variable), std::move(init), std::move(endCondition),
                                        std::move(step), std::move(body));
}

std::unique_ptr<VarExprAst> parseVarExpr(TokenStream &stream) {
    // eat 'var' keyword
    const auto varKey = stream.nextToken();
    if (!std::holds_alternative<Var>(varKey)) {
        throw std::runtime_error{"Expected 'var'"};
    }
    std::vector<std::tuple<Identifier, std::unique_ptr<ExprAst> > > variables;
    // 处理 identifier = expr (, identifier = expr )*
    do {
        auto id = std::get<Identifier>(stream.nextToken());
        if (!isChar(stream.nextToken(), '=')) {
            throw std::runtime_error{"Expected '='"};
        }
        auto expr = parseExpr(stream);
        variables.emplace_back(id, std::move(expr));
        if (isChar(stream.peekToken(), ',')) {
            stream.nextToken();
        } else {
            // 没有分隔符，认为该结束了。
            break;
        }
    } while (std::holds_alternative<Identifier>(stream.peekToken()));
    if (!std::holds_alternative<In>(stream.nextToken())) {
        throw std::runtime_error{"Expected 'in'"};
    }
    auto body = parseExpr(stream);
    return std::make_unique<VarExprAst>(std::get<Var>(varKey), std::move(variables), std::move(body));
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
    if (std::holds_alternative<Var>(firstToken)) {
        return parseVarExpr(stream);
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
    // 提前注册优先级，确保递归定义binary操作时，可以parse时正确获取到优先级
    if (prototype.precedence.has_value()) {
        std::string name = prototype.identifier.name;
        char op = name.back();
        GLOBAL_BINARY_OPS[op] = prototype.precedence.value();
    }
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
