#include "framework.h"
#include "Specific/Structures/BitField.h"

namespace TEN::Utils
{
	const BitField BitField::Empty	 = BitField(0);
	const BitField BitField::Default = BitField(DEFAULT_SIZE);

	BitField::BitField()
	{
		_chunks.resize((DEFAULT_SIZE + (CHUNK_SIZE - 1)) / CHUNK_SIZE);
		_size = DEFAULT_SIZE;
	}

	BitField::BitField(unsigned int size)
	{
		_chunks.resize((size + (CHUNK_SIZE - 1)) / CHUNK_SIZE);
		_size = size;
	}

	BitField::BitField(const std::initializer_list<bool>& bits)
	{
		_chunks.resize((bits.size() + (CHUNK_SIZE - 1)) / CHUNK_SIZE);
		_size = (unsigned int)bits.size();

		unsigned int bitID = 0;
		for (auto it = bits.begin(); it != bits.end(); it++, bitID++)
		{
			unsigned int localBitID = bitID % CHUNK_SIZE;
			unsigned int chunkID = bitID / CHUNK_SIZE;

			bool bit = *it;
			if (bit)
				_chunks[chunkID] |= (ChunkType)1 << localBitID;
		}
	}

	BitField::BitField(const std::vector<ChunkType>& bitChunks, unsigned int size)
	{
		_chunks = bitChunks;
		_size = std::min(size, (unsigned int)bitChunks.size() * CHUNK_SIZE);
	}

	BitField::BitField(const std::string& bitString)
	{
		_chunks.resize((bitString.size() + (CHUNK_SIZE - 1)) / CHUNK_SIZE);
		_size = (unsigned int)bitString.size();

		unsigned int bitID = 0;
		for (char bit : bitString)
		{
			unsigned int i = bitID % CHUNK_SIZE;
			unsigned int chunkID = bitID / CHUNK_SIZE;

			if (bit == '1')
				_chunks[chunkID] |= (ChunkType)1 << i;

			bitID++;
		}
	}

	unsigned int BitField::GetSize() const
	{
		return _size;
	}

	unsigned int BitField::GetCount() const
	{
		unsigned int count = 0;
		for (auto chunk : _chunks)
		{
			for (unsigned int localBitID = 0; localBitID < CHUNK_SIZE; localBitID++)
			{
				bool bit = bool(chunk & ((ChunkType)1 << localBitID));
				if (bit)
					count++;
			}
		}

		return count;
	}

	const std::vector<BitField::ChunkType>& BitField::GetChunks() const
	{
		return _chunks;
	}

	void BitField::Set(unsigned int bitID)
	{
		if constexpr (DEBUG_BUILD)
		{
			if (!IsBitIDCorrect(bitID))
				return;
		}

		unsigned int localBitId = bitID % CHUNK_SIZE;
		unsigned int chunkID = bitID / CHUNK_SIZE;
		_chunks[chunkID] |= (ChunkType)1 << localBitId;
	}

	void BitField::Set(const std::vector<unsigned int>& bitIds)
	{
		for (unsigned int bitID : bitIds)
			Set(bitID);
	}

	void BitField::SetAll()
	{
		Fill(true);
	}

	void BitField::Clear(unsigned int bitID)
	{
		if constexpr (DEBUG_BUILD)
		{
			if (!IsBitIDCorrect(bitID))
				return;
		}

		unsigned int localBitID = bitID % CHUNK_SIZE;
		unsigned int chunkID = bitID / CHUNK_SIZE;
		_chunks[chunkID] &= ~((ChunkType)1 << localBitID);
	}

	void BitField::Clear(const std::vector<unsigned int>& bitIds)
	{
		for (unsigned int bitID : bitIds)
			Clear(bitID);
	}

	void BitField::ClearAll()
	{
		Fill(false);
	}

	void BitField::Flip(unsigned int bitID)
	{
		if constexpr (DEBUG_BUILD)
		{
			if (!IsBitIDCorrect(bitID))
				return;
		}

		unsigned int localBitID = bitID % CHUNK_SIZE;
		unsigned int chunkID = bitID / CHUNK_SIZE;
		_chunks[chunkID] ^= (ChunkType)1 << localBitID;
	}

	void BitField::Flip(const std::vector<unsigned int>& bitIds)
	{
		for (unsigned int bitID : bitIds)
			Flip(bitID);
	}

	void BitField::FlipAll()
	{
		for (unsigned int chunkID = 0; chunkID < _chunks.size(); chunkID++)
			_chunks[chunkID] = ~_chunks[chunkID];

		unsigned int endBitCount = _size % CHUNK_SIZE;
		if (endBitCount > 0)
		{
			for (unsigned int localBitID = endBitCount; localBitID < CHUNK_SIZE; localBitID++)
				_chunks.back() &= ~((ChunkType)1 << localBitID);
		}
	}

	bool BitField::IsEmpty() const
	{
		if (_chunks.empty())
			return true;

		for (auto chunk : _chunks)
		{
			if (chunk != (ChunkType)0)
				return false;
		}

		return true;
	}

	bool BitField::Test(unsigned int bitID) const
	{
		if constexpr (DEBUG_BUILD)
		{
			if (!IsBitIDCorrect(bitID))
				return false;
		}

		unsigned int localBitID = bitID % CHUNK_SIZE;
		unsigned int chunkID = bitID / CHUNK_SIZE;
		return bool(_chunks[chunkID] & ((ChunkType)1 << localBitID));
	}

	bool BitField::Test(const std::vector<unsigned int>& bitIds, bool testAny) const
	{
		for (unsigned int bitID : bitIds)
		{
			if (testAny)
			{
				if (Test(bitID))
					return true;
			}
			else // Test all.
			{
				if (!Test(bitID))
					return false;
			}
		}

		return !testAny;
	}

	bool BitField::TestAny() const
	{
		for (auto chunk : _chunks)
		{
			if (chunk != 0)
				return true;
		}

		return false;
	}

	bool BitField::TestAll() const
	{
		for (unsigned int chunkID = 0; chunkID < (_chunks.size() - 1); chunkID++)
		{
			if (_chunks[chunkID] != (ChunkType)~0)
				return false;
		}

		unsigned int endBitCount = _size % CHUNK_SIZE;
		if (endBitCount > 0)
		{
			auto validMask = ((ChunkType)1 << endBitCount) - 1;
			if ((_chunks.back() & validMask) != validMask)
				return false;
		}

		return true;
	}

	void BitField::Resize(unsigned int size)
	{
		_chunks.resize((size + (CHUNK_SIZE - 1)) / CHUNK_SIZE);
		_size = size;

		unsigned int endBitCount = size % CHUNK_SIZE;
		if (endBitCount > 0)
		{
			for (unsigned int localBitID = endBitCount; localBitID < CHUNK_SIZE; localBitID++)
				_chunks.back() &= ~((ChunkType)1 << localBitID);
		}
	}

	std::string BitField::ToString() const
	{
		auto bitString = std::string();
		bitString.reserve(_size);

		for (unsigned int chunkID = 0; chunkID < _chunks.size(); chunkID++)
		{
			for (unsigned int localBitID = 0; localBitID < CHUNK_SIZE; localBitID++)
			{
				if (((chunkID * CHUNK_SIZE) + localBitID) >= _size)
					break;

				bool bit = _chunks[chunkID] & ((ChunkType)1 << localBitID);
				bitString += bit ? '1' : '0';
			}
		}

		return bitString;
	}

	BitField::ChunkType BitField::ToPackedBits() const
	{
		if (_chunks.empty())
			return 0;

		return _chunks.front();
	}

	bool BitField::operator ==(const BitField& bitField) const
	{
		if (_size != bitField.GetSize())
			return false;

		for (unsigned int chunkID = 0; chunkID < _chunks.size(); chunkID++)
		{
			if (_chunks[chunkID] != bitField.GetChunks()[chunkID])
				return false;
		}

		return true;
	}

	bool BitField::operator !=(const BitField& bitField) const
	{
		return !(*this == bitField);
	}

	BitField& BitField::operator &=(const BitField& bitField)
	{
		for (unsigned int chunkID = 0; chunkID < std::min(_chunks.size(), bitField.GetChunks().size()); chunkID++)
			_chunks[chunkID] &= bitField.GetChunks()[chunkID];

		_chunks.resize(std::min((unsigned int)_chunks.size(), bitField.GetSize()));
		_size = std::min(_size, bitField.GetSize());
		return *this;
	}

	BitField& BitField::operator |=(const BitField& bitField)
	{
		_chunks.resize(std::max(_chunks.size(), bitField.GetChunks().size()));
		_size = std::max(_size, bitField.GetSize());

		for (unsigned int chunkID = 0; chunkID < bitField.GetChunks().size(); chunkID++)
			_chunks[chunkID] |= bitField.GetChunks()[chunkID];

		return *this;
	}

	BitField& BitField::operator ^=(const BitField& bitField)
	{
		_chunks.resize(std::max(_chunks.size(), bitField.GetChunks().size()));
		_size = std::max(_size, bitField.GetSize());

		for (unsigned int chunkID = 0; chunkID < std::min(_chunks.size(), bitField.GetChunks().size()); chunkID++)
			_chunks[chunkID] ^= bitField.GetChunks()[chunkID];

		return *this;
	}

	BitField BitField::operator &(const BitField& bitField) const
	{
		auto chunks = std::vector<ChunkType>(std::min(_chunks.size(), bitField.GetChunks().size()));
		for (unsigned int chunkID = 0; chunkID < chunks.size(); chunkID++)
			chunks[chunkID] = _chunks[chunkID] & bitField.GetChunks()[chunkID];

		return BitField(chunks, std::min(_size, bitField.GetSize()));
	}

	BitField BitField::operator |(const BitField& bitField) const
	{
		auto chunks = std::vector<ChunkType>(std::max(_chunks.size(), bitField.GetChunks().size()));

		for (unsigned int chunkID = 0; chunkID < _chunks.size(); chunkID++)
			chunks[chunkID] |= _chunks[chunkID];

		for (unsigned int chunkID = 0; chunkID < bitField.GetChunks().size(); chunkID++)
			chunks[chunkID] |= bitField.GetChunks()[chunkID];

		return BitField(chunks, std::max(_size, bitField.GetSize()));
	}

	BitField BitField::operator ^(const BitField& bitField) const
	{
		auto chunks = std::vector<ChunkType>(std::max(_chunks.size(), bitField.GetChunks().size()));

		for (unsigned int chunkID = 0; chunkID < chunks.size(); chunkID++)
		{
			if (chunkID < _chunks.size() && chunkID < bitField.GetChunks().size())
			{
				chunks[chunkID] = _chunks[chunkID] ^ bitField.GetChunks()[chunkID];
			}
			else if (chunkID < _chunks.size())
			{
				chunks[chunkID] = _chunks[chunkID];
			}
			else if (chunkID < bitField.GetChunks().size())
			{
				chunks[chunkID] = bitField.GetChunks()[chunkID];
			}
		}

		return BitField(chunks, std::max(_size, bitField.GetSize()));
	}

	BitField BitField::operator ~() const
	{
		auto newBitField = *this;
		newBitField.FlipAll();
		return newBitField;
	}

	bool BitField::operator ==(unsigned int packedBits) const
	{
		if (_chunks.empty())
			return (packedBits == 0);

		return (_chunks.front() == packedBits);
	}
	
	bool BitField::operator !=(unsigned int packedBits) const
	{
		return !(*this == packedBits);
	}

	BitField& BitField::operator =(unsigned int packedBits)
	{
		_chunks.resize(1);
		_chunks.front() = packedBits;
		_size = DEFAULT_SIZE;

		return *this;
	}

	BitField& BitField::operator &=(unsigned int packedBits)
	{
		_chunks.resize(1);
		_chunks.front() &= packedBits;
		_size = DEFAULT_SIZE;

		return *this;
	}

	BitField& BitField::operator |=(unsigned int packedBits)
	{
		_chunks.resize(1);
		_chunks.front() |= packedBits;
		_size = DEFAULT_SIZE;

		return *this;
	}
	
	unsigned int BitField::operator &(unsigned int packedBits) const
	{
		if (_chunks.empty())
			return 0;

		return (_chunks.front() & packedBits);
	}

	unsigned int BitField::operator |(unsigned int packedBits) const
	{
		if (_chunks.empty())
			return packedBits;

		return (_chunks.front() | packedBits);
	}

	void BitField::Fill(bool value)
	{
		auto fillChunk = value ? ~(ChunkType)0 : (ChunkType)0;
		std::fill(_chunks.begin(), _chunks.end(), fillChunk);

		unsigned int endBitCount = _size % CHUNK_SIZE;
		if (endBitCount > 0 && value)
			_chunks.back() &= ((ChunkType)1 << endBitCount) - 1;
	}

	bool BitField::IsBitIDCorrect(unsigned int bitID) const
	{
		if (bitID >= _size)
		{
			TENLog(std::string("BitField attempted to access bit at invalid index."), LogLevel::Warning);
			return false;
		}

		return true;
	}
}
