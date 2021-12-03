#include "Resource.hpp"

#include <Windows.h>
#include <WinUser.h>

Resource::Resource(int id, const char *type)
{
    auto hResource = FindResourceA(nullptr, MAKEINTRESOURCEA(id), type);
    auto hMemory = LoadResource(nullptr, hResource);
    m_size = SizeofResource(nullptr, hResource);
    m_data = LockResource(hMemory);
}