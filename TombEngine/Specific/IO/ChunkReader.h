#pragma once
#include "Specific/IO/ChunkId.h"
#include "Specific/IO/LEB128.h"
#include "Specific/IO/Streams.h"


class ChunkReader
{
private:
	bool m_isValid;
	ChunkId* m_emptyChunk = nullptr;
	BaseStream* m_stream = nullptr;

	int readInt32();

	short readInt16();

public:
	ChunkReader(int expectedMagicNumber, BaseStream* stream);

	~ChunkReader();

	bool IsValid();

	bool ReadChunks(bool(*func)(ChunkId* parentChunkId, int maxSize, int arg), int arg);

	bool ReadChunks(std::function<bool(ChunkId*, long, int)> func, int arg);

	char* ReadChunkArrayOfBytes(long long length);

	bool ReadChunkBool(long long length);

	long long ReadChunkLong(long long length);

	int ReadChunkInt32(long long length);

	unsigned int ReadChunkUInt32(long long length);

	short ReadChunkInt16(long long length);

	unsigned short ReadChunkUInt16(long long length);

	unsigned char ReadChunkByte(long long length);

	BaseStream* GetRawStream();
};
