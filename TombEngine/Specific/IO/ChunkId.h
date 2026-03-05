#pragma once
#include <memory>
#include "Specific/IO/LEB128.h"
#include "Specific/IO/Streams.h"

struct ChunkId
{
private:
	unsigned char*		m_chunkBytes;
	int		m_length;

public:
	ChunkId(char* bytes, int length);

	~ChunkId();

	static std::unique_ptr<ChunkId> FromString(const char* str);

	static std::unique_ptr<ChunkId> FromString(std::string* str);

	static std::unique_ptr<ChunkId> FromStream(BaseStream* stream);

	void ToStream(BaseStream* stream);

	unsigned char* GetBytes();

	int GetLength();

	bool EqualsTo(ChunkId* other);
};