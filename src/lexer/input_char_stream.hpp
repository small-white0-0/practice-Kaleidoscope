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

    std::optional<char> last() override;

    Position postion() override;

private:
    Position pos = {"cin", 0, 0};
    std::optional<char> prev = std::nullopt;
};

#endif //PRACTICE_KALEIDOSCOPE_INPUT_CHAR_STREAM_HPP
