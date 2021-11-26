#ifndef SHIFVRSKY_RESOURCE_HPP
#define SHIFVRSKY_RESOURCE_HPP

#include <string>

#include "util/imemstream.hpp"

/**
 * @brief This is a view of an embedded resource.
 */
class Resource
{
public:

    // Constructors
    //--------------
    Resource(int id, const char *type = "RT_RCDATA");

    // Getters
    //---------

    inline size_t size() const noexcept
    {
        return m_size;
    }

    inline const void *data() const noexcept
    {
        return m_data;
    }

    inline imemstream stream() const
    {
        return imemstream((const char*)m_data, m_size);
    }

    inline std::string str() const
    {
        return std::string((const char*)m_data, m_size);
    }

private:

    // Properties
    //------------
    
    size_t m_size;
    void* m_data;

};

#endif // SHIFVRSKY_RESOURCE_HPP