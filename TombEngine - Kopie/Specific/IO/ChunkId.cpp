#include "framework.h"
#include "Specific/IO/ChunkId.h"

using std::string;

ChunkId::ChunkId(char* bytes, int length)
{
	if (length == 0)
	{
		m_chunkBytes = nullptr;
		m_length = 0;
	}
	else
	{
		m_chunkBytes = (byte*)malloc(length);
		memcpy(m_chunkBytes, bytes, length);
		m_length = length;
	}
}

ChunkId::~ChunkId()
{
	if (m_chunkBytes != nullptr)
		delete m_chunkBytes;
}

std::unique_ptr<ChunkId> ChunkId::FromString(const char* str)
{
	return std::make_unique<ChunkId>((char*)str, (int)strlen(str));
}

std::unique_ptr<ChunkId> ChunkId::FromString(string* str)
{
	return std::make_unique<ChunkId>((char*)str->c_str(), (int)str->length());
}

std::unique_ptr<ChunkId> ChunkId::FromStream(BaseStream* stream)
{
	int idLength = LEB128::ReadInt32(stream);
	char* buffer = (char*)malloc(idLength);
	stream->Read(buffer, idLength);
	std::unique_ptr<ChunkId> chunk = std::make_unique<ChunkId>(buffer, idLength);
	free(buffer);
	return chunk;
}

void ChunkId::ToStream(BaseStream* stream)
{
	LEB128::Write(stream, m_length);
	stream->WriteBytes(m_chunkBytes, m_length);
}

byte* ChunkId::GetBytes()
{
	return m_chunkBytes;
}

int ChunkId::GetLength() 
{
	return m_length;
}

bool ChunkId::EqualsTo(ChunkId* other)
{
	if (m_length != other->GetLength())
		return false;

	return (strncmp((const char*)m_chunkBytes, (const char*)other->GetBytes(), m_length) == 0);
}
