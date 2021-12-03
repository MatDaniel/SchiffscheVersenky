#ifndef SHIFVRSKY_BUFFERS_HPP
#define SHIFVRSKY_BUFFERS_HPP

#include <glad/glad.h>
#include <typeinfo>

namespace MappedBufferConstants
{
	
	// Constants
	static constexpr GLenum FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

}

template <class T>
class MappedBuffer
{
public:

	inline ~MappedBuffer()
	{
		glDeleteBuffers(1, &m_id);
	}

	inline GLuint id() const noexcept
	{
		return m_id;
	}

	inline auto ptr() const noexcept
	{
		return m_ptr;
	}

protected:

	GLuint m_id { 0 };
	std::remove_pointer_t<T>* m_ptr { nullptr };

};

namespace impl
{

	template <class T, GLsizeiptr Count>
	struct static_mapped_buf
	{
		using type = std::conditional_t<(Count > 1), T*, T>;
	};

	template <class T, GLsizeiptr Count>
	using static_mapped_buf_t = typename static_mapped_buf<T, Count>::type;

}

template <class T, GLsizeiptr Count = 1>
class StaticMappedBuffer : public MappedBuffer<impl::static_mapped_buf_t<T, Count>>
{
public:

	// Constants
	using MappedType = typename impl::static_mapped_buf_t<T, Count>;
	static constexpr GLenum MAPPING_FLAGS = MappedBufferConstants::FLAGS;

	inline StaticMappedBuffer()
		: MappedBuffer<MappedType>()
	{
		glCreateBuffers(1, &this->m_id);
		glNamedBufferStorage(this->m_id, sizeof(T) * Count, nullptr, MAPPING_FLAGS);
		this->m_ptr = (MappedType*)glMapNamedBufferRange(this->m_id, 0, sizeof(T) * Count, MAPPING_FLAGS);
	}

};

template <class T>
class DynamicMappedBuffer : public MappedBuffer<T*>
{
public:

	// Constants
	static constexpr GLenum MAPPING_FLAGS = MappedBufferConstants::FLAGS;

	inline void resize(GLsizeiptr count) noexcept
	{
		if (m_count < count)
		{
			glDeleteBuffers(1, &this->m_id);
			glCreateBuffers(1, &this->m_id);
			glNamedBufferStorage(this->m_id, sizeof(T) * count, nullptr, MAPPING_FLAGS);
			this->m_ptr = (T*)glMapNamedBufferRange(this->m_id, 0, sizeof(T) * count, MAPPING_FLAGS);
			m_count = count;
		}
	}

private:

	GLsizeiptr m_count;

};

#endif // SHIFVRSKY_BUFFERS_HPP