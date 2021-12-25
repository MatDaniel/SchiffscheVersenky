#pragma once

#include <xhash>

inline uint64_t fnFNV(const uint8_t* pBuffer, const uint8_t* const pBufferEnd)
{
    const uint64_t  MagicPrime = 0x00000100000001b3;
    uint64_t Hash = 0xcbf29ce484222325;

    for (; pBuffer < pBufferEnd; pBuffer++)
        Hash = (Hash ^ *pBuffer) * MagicPrime;   // bitweises XOR and then multiplyw

    return Hash;
}

#define SHIV_FNV_HASH(Type) \
template<> \
struct std::hash<Type> \
{ \
    inline std::size_t operator()(Type const& val) const noexcept \
    { \
        return fnFNV((const uint8_t*)&val, (const uint8_t*)&val + 1); \
    } \
};
