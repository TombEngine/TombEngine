#pragma once
#include "Specific/IO/Streams.h"
#include "Specific/IO/LEB128.h"
#include "Specific/IO/ChunkId.h"

struct ChunkWritingState
{
private:
	BaseStream* m_stream;
	__int64 m_chunkSizePosition;
	__int64 m_previousPosition;
	__int64 m_maximumSize;

public:
	ChunkWritingState(BaseStream* stream, ChunkId* chunkID, __int64 maximumSize)
	{
		m_stream = stream;

		// Write chunk ID
		chunkID->ToStream(m_stream);

		// Write chunk size
		m_chunkSizePosition = m_stream->GetCurrentPosition();
		LEB128::Write(m_stream, 0, maximumSize);

		// Prepare for writeing chunk content
		m_previousPosition = m_stream->GetCurrentPosition();
		m_maximumSize = maximumSize;
	}

	void EndWrite()
	{
		// Update chunk size
		long newPosition = m_stream->GetCurrentPosition();
		long chunkSize = newPosition - m_previousPosition;
		m_stream->Seek(m_chunkSizePosition, SeekOrigin::BEGIN);
		LEB128::Write(m_stream, chunkSize, m_maximumSize);
		m_stream->Seek(newPosition, SeekOrigin::BEGIN);
	}
};

class ChunkWriter {
private:
	BaseStream*		m_stream;

public:
	ChunkWriter(int magicNumber, BaseStream* stream)
	{
		m_stream = stream;

		m_stream->WriteInt32(magicNumber);
		m_stream->WriteInt32(0);
	}

	BaseStream* GetRawStream()
	{
		return m_stream;
	}

	void WriteChunkEnd()
	{
		m_stream->WriteByte(0);
	}

	void WriteChunkEmpty(ChunkId* chunkID)
	{
		chunkID->ToStream(m_stream);
		LEB128::Write(m_stream, 0);
	}

	void WriteChunkArrayOfBytes(ChunkId* chunkID, byte* value, int length)
	{
		chunkID->ToStream(m_stream);
		LEB128::Write(m_stream, length);
		m_stream->WriteBytes(value, length);
	}

	void WriteChunkInt(ChunkId* chunkID, __int64 value)
	{
		chunkID->ToStream(m_stream);
		LEB128::Write(m_stream, LEB128::GetLength(m_stream, value));
		LEB128::Write(m_stream, value);
	}

	ChunkWritingState* WriteChunk(ChunkId* chunkID, __int64 maximumSize = LEB128::MaximumSize4Byte)
	{
		return new ChunkWritingState(m_stream, chunkID, maximumSize);
	}

	void WriteChunk(ChunkId* chunkID, void(*writeChunk)(int, int), int arg1, int arg2, __int64 maximumSize = LEB128::MaximumSize4Byte)
	{
		ChunkWritingState* state = WriteChunk(chunkID, maximumSize);
		writeChunk(arg1, arg2);
		state->EndWrite();
		delete state;
	}

	void WriteChunkWithChildren(ChunkId* chunkID, void(*writeChunk)(int, int), int arg1, int arg2, __int64 maximumSize = LEB128::MaximumSize4Byte)
	{
		ChunkWritingState* state = WriteChunk(chunkID, maximumSize);
		writeChunk(arg1, arg2);
		WriteChunkEnd();
		state->EndWrite();
		delete state;
	}
};
