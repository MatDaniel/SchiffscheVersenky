#include "Materials.hpp"

Material::Material()
    : m_albedo(nullptr)
    , m_normal(nullptr)
    , m_mrao(nullptr)
{
}

Material::Material(Texture *albedo, Texture *normal, Texture *mrao)
    : m_albedo(albedo)
    , m_normal(normal)
    , m_mrao(mrao)
{
}

//----------------//
// INSTANTIATIONS //
//----------------//

const Materials::InstanceMap Materials::map {{ }};