module;

#include <glad/glad.h>
#include <utility>

export module Draw.Buffers;

static constexpr GLenum PRESISTENT_FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

export template <class T>
class MappedBuffer
{
public:

	inline MappedBuffer()
		: m_id(0)
		, m_ptr(nullptr)
	{
	}
	
	inline MappedBuffer(MappedBuffer&& other)
		: m_id(other.m_id)
		, m_ptr(other.m_ptr)
	{
		other.m_id = 0;
		other.m_ptr = nullptr;
	}

	inline MappedBuffer& operator=(MappedBuffer&& other)
	{
		m_id = other.m_id;
		m_ptr = other.m_ptr;
		other.m_id = 0;
		other.m_ptr = nullptr;
		return *this;
	}

	MappedBuffer(const MappedBuffer& other) = delete;
	MappedBuffer& operator=(const MappedBuffer& other) = delete;

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

	GLuint m_id;
	T* m_ptr;

};

export template <class T, GLsizeiptr Count = 1>
class StaticMappedBuffer : public MappedBuffer<T>
{
public:

	inline StaticMappedBuffer()
		: MappedBuffer<T>()
	{
		glCreateBuffers(1, &this->m_id);
		glNamedBufferStorage(this->m_id, sizeof(T) * Count, nullptr, PRESISTENT_FLAGS);
		this->m_ptr = (T*)glMapNamedBufferRange(this->m_id, 0, sizeof(T) * Count, PRESISTENT_FLAGS);
	}

	inline StaticMappedBuffer(StaticMappedBuffer&& other)
		: MappedBuffer(std::move(other))
	{
	}

	inline StaticMappedBuffer& operator=(StaticMappedBuffer&& other)
	{
		MappedBuffer::operator=(std::move(other));
		return *this;
	}

	StaticMappedBuffer(const StaticMappedBuffer& other) = delete;
	StaticMappedBuffer& operator=(const StaticMappedBuffer& other) = delete;

};

export template <class T>
class DynamicMappedBuffer : public MappedBuffer<T>
{
public:

	inline DynamicMappedBuffer()
		: MappedBuffer<T>()
		, m_count(0)
	{
	}

	inline DynamicMappedBuffer(DynamicMappedBuffer&& other)
		: MappedBuffer(std::move(other))
		, m_count(other.m_count)
	{
		other.m_count = 0;
	}

	inline DynamicMappedBuffer& operator=(DynamicMappedBuffer&& other)
	{
		MappedBuffer::operator=(std::move(other));
		m_count = other.m_count;
		other.m_count = 0;
		return *this;
	}

	DynamicMappedBuffer(const DynamicMappedBuffer& other) = delete;
	DynamicMappedBuffer& operator=(const DynamicMappedBuffer& other) = delete;

	inline void resize(GLsizeiptr count) noexcept
	{
		if (m_count < count)
		{
			glDeleteBuffers(1, &this->m_id);
			glCreateBuffers(1, &this->m_id);
			glNamedBufferStorage(this->m_id, sizeof(T) * count, nullptr, PRESISTENT_FLAGS);
			this->m_ptr = (T*)glMapNamedBufferRange(this->m_id, 0, sizeof(T) * count, PRESISTENT_FLAGS);
			m_count = count;
		}
	}

private:

	GLsizeiptr m_count;

};