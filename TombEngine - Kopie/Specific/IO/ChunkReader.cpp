#include "framework.h"
#include "Specific/IO/ChunkReader.h"

int ChunkReader::readInt32()
{
	int value = 0;
	m_stream->Read(reinterpret_cast<char*>(&value), 4);
	return value;
}

short ChunkReader::readInt16()
{
	short value = 0;
	m_stream->Read(reinterpret_cast<char*>(&value), 2);
	return value;
}

ChunkReader::ChunkReader(int expectedMagicNumber, BaseStream* stream)
{
	m_isValid = false;

	if (stream == NULL)
		return;

	m_stream = stream;

	// Check the magic number
	int magicNumber = readInt32();
	if (magicNumber != expectedMagicNumber)
		return;

	// TODO: future use for compression
	m_stream->Seek(4, SeekOrigin::CURRENT);
	m_emptyChunk = new ChunkId(NULL, 0);
	m_isValid = true;
}

ChunkReader::~ChunkReader()
{
	delete m_emptyChunk;
}

bool ChunkReader::IsValid()
{
	return m_isValid;
}

bool ChunkReader::ReadChunks(bool(*func)(ChunkId* parentChunkId, int maxSize, int arg), int arg)
{
	do
	{
		std::unique_ptr<ChunkId> chunkId = ChunkId::FromStream(m_stream);
		if (chunkId->EqualsTo(m_emptyChunk)) // End reached
			break;

		// Read up to a 64 bit number for the chunk size
		__int64 chunkSize = LEB128::ReadLong(m_stream);

		// Try loading chunk content
		bool chunkRecognized = false;
		int startPos = m_stream->GetCurrentPosition();

		chunkRecognized = func(chunkId.get(), chunkSize, arg);
		int readDataCount = m_stream->GetCurrentPosition() - startPos;

		// Adjust _stream position if necessary
		if (readDataCount != chunkSize)
			m_stream->Seek(chunkSize - readDataCount, SeekOrigin::CURRENT);
	}
	while (true);

	return true;
}

bool ChunkReader::ReadChunks(std::function<bool(ChunkId*, long, int)> func, int arg)
{
	do
	{
		std::unique_ptr<ChunkId> chunkId = ChunkId::FromStream(m_stream);
		if (chunkId->EqualsTo(m_emptyChunk)) // End reached
			break;

		// Read up to a 64 bit number for the chunk size
		__int64 chunkSize = LEB128::ReadLong(m_stream);

		// Try loading chunk content
		bool chunkRecognized = false;
		int startPos = m_stream->GetCurrentPosition();

		chunkRecognized = func(chunkId.get(), chunkSize, arg);
		int readDataCount = m_stream->GetCurrentPosition() - startPos;

		// Adjust _stream position if necessary
		if (readDataCount != chunkSize)
			m_stream->Seek(chunkSize - readDataCount, SeekOrigin::CURRENT);
	}
	while (true);

	return true;
}

char* ChunkReader::ReadChunkArrayOfBytes(__int64 length)
{
	char* value = (char*)malloc(length);
	m_stream->Read(value, length);
	return value;
}

bool ChunkReader::ReadChunkBool(__int64 length)
{
	return (LEB128::ReadByte(m_stream) != 0);
}

__int64 ChunkReader::ReadChunkLong(__int64 length)
{
	return LEB128::ReadLong(m_stream);
}

int ChunkReader::ReadChunkInt32(__int64 length)
{
	return LEB128::ReadInt32(m_stream);
}

unsigned int ChunkReader::ReadChunkUInt32(__int64 length)
{
	return LEB128::ReadUInt32(m_stream);
}

short ChunkReader::ReadChunkInt16(__int64 length)
{
	return LEB128::ReadInt16(m_stream);
}

unsigned short ChunkReader::ReadChunkUInt16(__int64 length)
{
	return LEB128::ReadUInt16(m_stream);
}

byte ChunkReader::ReadChunkByte(__int64 length)
{
	return LEB128::ReadByte(m_stream);
}

BaseStream* ChunkReader::GetRawStream()
{
	return m_stream;
}
