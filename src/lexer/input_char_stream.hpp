#ifndef PRACTICE_KALEIDOSCOPE_INPUT_CHAR_STREAM_HPP
#define PRACTICE_KALEIDOSCOPE_INPUT_CHAR_STREAM_HPP

#include "char_stream.hpp"

class InputCharStream final : public CharStream {
public:
    InputCharStream() = default;

    InputCharStream(InputCharStream const &) = delete;

    InputCharStream &operator=(InputCharStream const &) = delete;

    InputCharStream(InputCharStream &&) = default;

    InputCharStream &operator=(InputCharStream &&) = default;

    std::optional<char> peek() override;

    std::optional<char> next() override;

    Position position() override;

private:
    /// dwarf标准的line和col有效值都是从1开始的。
    /// 0目前好像算是一种特殊的未知信息的意思。
    ///
    /// 设置pos指向第一个字符，因为没有读取就预先设置了pos，
    /// 所以可能存在pos指向的非字符内容位置。
    /// 但是正常获取pos前会使用peek进行解析，
    /// 所以非法的pos不会被使用。
    Position pos = {"cin", 1, 1};
};

#endif //PRACTICE_KALEIDOSCOPE_INPUT_CHAR_STREAM_HPP
