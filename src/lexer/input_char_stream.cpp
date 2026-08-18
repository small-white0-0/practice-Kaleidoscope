#include "input_char_stream.hpp"

#include <iostream>


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

    // pos是要指向c的下一个字符的，所以换行之后，
    // col=1,表示下一行的第一个字符，而不是当前的'\n'
    // 不过，这个有可能pos指向的EOF或'\n'或其他非法内容。
    // 但是，会根据peek的值，判断是否要使用该position,
    // 所以没有问题。

    // 根据字符开始修改line和col.
    if (c == '\n') {
        this->pos.line++;
        this->pos.column = 1;
    } else {
        this->pos.column++;
    }
    // 现在line和column都是指向c的下一个字符的位置，也就是peek获取的字符的位置。
    return {c};
}

Position InputCharStream::position() {
    return this->pos;
}
