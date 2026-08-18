#ifndef PRACTICE_KALEIDOSCOPE_TOKEN_STREAM_HPP
#define PRACTICE_KALEIDOSCOPE_TOKEN_STREAM_HPP

#include <memory>
#include "token.hpp"


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

#endif //PRACTICE_KALEIDOSCOPE_TOKEN_STREAM_HPP
