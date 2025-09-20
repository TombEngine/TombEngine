#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "Specific/IO/Streams.h"

struct LEB128
{
	static const uint64_t MaximumSize1Byte = 63L;
	static const uint64_t MaximumSize2Byte = 8191L;
	static const uint64_t MaximumSize3Byte = 1048575L;
	static const uint64_t MaximumSize4Byte = 134217727L;
	static const uint64_t MaximumSize5Byte = 17179869183L;
	static const uint64_t MaximumSize6Byte = 2199023255551L;
	static const uint64_t MaximumSize7Byte = 281474976710655L;
	static const uint64_t MaximumSize8Byte = 36028797018963968L;
	static const uint64_t MaximumSize9Byte = 4611686018427387903L;
	static const uint64_t MaximumSize10Byte = UINT64_MAX;

	static long ReadLong(BaseStream* stream)
	{
		long result = 0;
		int currentShift = 0;

		byte currentByte;
		do
		{
			stream->Read(reinterpret_cast<char *>(&currentByte), 1);
			
			result |= (long)(currentByte & 0x7F) << currentShift;
			currentShift += 7;
		}
		while ((currentByte & 0x80) != 0);

		// Sign extend
		int shift = 64 - currentShift;
		if (shift > 0)
			result = (result << shift) >> shift;

		return result;
	}

	static int ReadInt32(BaseStream* stream)
	{
		long long value = ReadLong(stream);
		return static_cast<int>(std::min(std::max(value, static_cast<long long>(INT32_MIN)), static_cast<long long>(INT32_MAX)));
	}

	static short ReadInt16(BaseStream* stream)
	{
		long value = ReadLong(stream);
		return static_cast<short>(std::min(std::max(value, static_cast<long>(INT16_MIN)), static_cast<long>(INT16_MAX)));
	}

	static byte ReadByte(BaseStream* stream)
	{
		long value = ReadLong(stream);
		return static_cast<byte>(std::min(std::max(value, 0L), static_cast<long>(UINT8_MAX)));
	}	

	static unsigned int ReadUInt32(BaseStream* stream)
	{
		long long value = ReadLong(stream);
		return static_cast<unsigned int>(std::max(std::min(static_cast<unsigned long>(value), static_cast<unsigned long>(UINT32_MAX)), 0UL));
	}

	static unsigned short ReadUInt16(BaseStream* stream)
	{
		long value = ReadLong(stream);
		return static_cast<unsigned short>(std::min(std::max(value, 0L), static_cast<long>(UINT16_MAX)));
	}

	static void Write(BaseStream* stream, long value, long maximumSize)
	{
		do
		{
			// Write byte
			byte currentByte = ((byte)(value & 0x7F));
			if (maximumSize >> 6 == 0 || maximumSize >> 6 == -1)
			{
				stream->WriteByte(currentByte);

				if (value >> 6 != 0 && value >> 6 != -1)
					throw "Unable to write integer because the available space overflowed.";

				return;
			}

			byte b = currentByte | 0x80;
			stream->WriteByte(b);

			// Move data to next byte
			value >>= 7;
			maximumSize >>= 7;
		} while (true);
	}

	static void Write(BaseStream* stream, long value)
	{
		Write(stream, value, value);
	}

	static int GetLength(BaseStream* stream, long value)
	{
		int length = 1;
		value >>= 6;
		while (value > 0)
		{
			value >>= 7;
			length += 1;
		}
		return length;
	}
};
