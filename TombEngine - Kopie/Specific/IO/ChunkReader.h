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

	char* ReadChunkArrayOfBytes(__int64 length);

	bool ReadChunkBool(__int64 length);

	__int64 ReadChunkLong(__int64 length);

	int ReadChunkInt32(__int64 length);

	unsigned int ReadChunkUInt32(__int64 length);

	short ReadChunkInt16(__int64 length);

	unsigned short ReadChunkUInt16(__int64 length);

	byte ReadChunkByte(__int64 length);

	BaseStream* GetRawStream();
};
