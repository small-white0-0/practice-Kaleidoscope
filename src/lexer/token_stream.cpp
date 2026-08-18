#include "token_stream.hpp"
#include "common.hpp"

Token TokenStream::nextToken() {
    /**
     * 这个函数内是先解析下一个token,然后将cur指向的token作为返回结果。
     * 同时会更新cur为next,last更新为cur.
     * 如果next是空，且cur是空，则不会更新cur和last.
     */
    // 消除空白和#开始的注释
    while (true) {
        // 消耗前导space
        while (isspace(static_cast<unsigned char>(stream->peek().value_or('X')))) {
            stream->next();
        }
        if (stream->peek().value_or('\0') == '#') {
            auto has_comment = false;
            while (stream->peek().value_or('\n') != '\n') {
                if (!has_comment) {
                    debug("comment: ");
                    has_comment = true;
                }
                auto c = stream->next().value();
                debug(c);
            }
            if (has_comment) debug('\n');
        } else {
            break;
        }
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
        } else if (id == "var") {
            next = Var{loc};
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
    return last;
}
