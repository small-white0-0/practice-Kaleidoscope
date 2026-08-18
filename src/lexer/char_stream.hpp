#ifndef PRACTICE_KALEIDOSCOPE_CHAR_STREAM_HPP
#define PRACTICE_KALEIDOSCOPE_CHAR_STREAM_HPP
#include <optional>
#include <string>


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

#endif //PRACTICE_KALEIDOSCOPE_CHAR_STREAM_HPP
