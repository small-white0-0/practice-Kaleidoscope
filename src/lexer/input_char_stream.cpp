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
    // line和col有效值都是从1开始的。
    // 0目前好像算是一种特殊的未知信息的意思。
    //
    // 开始前line是0,col是0，表示行和列都是未知的。
    // 换行之后col也是0.因为当前指向的是'\n'.
    // 实际属于上一行的行尾，而不是新行的第一个字符。
    // 所以注意此处的0也表示未指定。

    // 正确设置line的初始值为1.
    if (this->pos.line == 0) {
        this->pos.line = 1;
    }
    // 根据字符开始修改line和col.
    if (c == '\n') {
        this->pos.line++;
        this->pos.column = 0;
    } else {
        this->pos.column++;
    }
    // 现在line和column都是指向c的位置的。
    return {c};
}

std::optional<char> InputCharStream::last() {
    return this->prev;
}

Position InputCharStream::postion() {
    return this->pos;
}
