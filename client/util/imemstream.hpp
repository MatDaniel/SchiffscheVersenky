#ifndef SHIFVRSKY_IMEMSTREAM_HPP
#define SHIFVRSKY_IMEMSTREAM_HPP

// From https://stackoverflow.com/a/13059195 (modified version)

#include <streambuf>
#include <istream>

struct membuf : std::streambuf {

    membuf(char const* base, size_t size);

};

struct imemstream : virtual membuf, std::istream {
    
    imemstream(char const* base, size_t size);

};

#endif // SHIFVRSKY_IMEMSTREAM_HPP