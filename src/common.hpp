#ifndef PRACTICE_KALEIDOSCOPE_COMMON_HPP
#define PRACTICE_KALEIDOSCOPE_COMMON_HPP
#include <iostream>

// #define enable_optimize
// #define gen_obj_file

template<typename T>
void debug(const T str, std::ostream *stream = &std::cerr) {
#if 0
    *stream << str;
#endif
}

#endif //PRACTICE_KALEIDOSCOPE_COMMON_HPP
