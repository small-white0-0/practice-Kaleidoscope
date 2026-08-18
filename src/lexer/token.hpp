#ifndef PRACTICE_KALEIDOSCOPE_TOKEN_HPP
#define PRACTICE_KALEIDOSCOPE_TOKEN_HPP

#include <cassert>
#include <variant>

#include "char_stream.hpp"

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

struct Var {
    Location loc;
    bool operator==(const Var &) const { return true; }
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

using Token = std::variant<std::monostate, Def, Extern, If, Then, Else, For, In, Unary, Binary, Var, Identifier, Number,
    Char>;

#endif //PRACTICE_KALEIDOSCOPE_TOKEN_HPP
