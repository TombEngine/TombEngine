#pragma once

#include <istream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <d3d11.h>
#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

enum SeekOrigin
{
	BEGIN,
	CURRENT
};

class BaseStream
{
public:
	virtual bool Read(char* buffer, int length) = 0;
	virtual bool Write(char const * buffer, int length) = 0;
	virtual int GetCurrentPosition() = 0;
	virtual bool Seek(int seek, SeekOrigin origin) = 0;
	virtual bool IsEOF() = 0;
	virtual bool Close() = 0;
	
	bool ReadBytes(byte* value, int length);

	bool ReadByte(byte* value);

	bool ReadBool(bool* value);

	bool ReadInt16(short* value);

	bool ReadInt32(int* value);

	bool ReadFloat(float* value);

	bool ReadString(char** value);

	bool ReadString(std::string* value);

	bool ReadVector2(Vector2* value);

	bool ReadVector3(Vector3* value);

	bool ReadVector4(Vector4* value);

	bool ReadQuaternion(Quaternion* value);

	bool ReadBoundingBox(BoundingBox* value);

	bool ReadBoundingSphere(BoundingSphere* sphere);

	bool WriteBytes(byte* value, int length);

	bool WriteByte(byte value);

	bool WriteInt16(short value);

	bool WriteInt32(int value);

	bool WriteBool(bool value);

	bool WriteFloat(float value);

	bool WriteString(char const * str);
};

class MemoryStream : public BaseStream
{
private:
	char* m_startBuffer;
	char* m_buffer;
	int m_size;

public:
	MemoryStream(char* buffer, int size);

	MemoryStream(int size);

	~MemoryStream();

	bool Read(char* buffer, int length);

	bool Write(char* buffer, int length);

	int GetCurrentPosition();

	bool Seek(int seek, SeekOrigin origin);

	bool IsEOF();

	bool Close();
};

class FileStream : public BaseStream
{
private:
	std::fstream m_stream;

public:
	FileStream(char* fileName, bool read, bool write);

	~FileStream();

	bool Read(char* buffer, int length);

	bool Write(char const * buffer, int length);

	int GetCurrentPosition();

	bool Seek(int seek, SeekOrigin origin);

	bool IsEOF();

	bool Close();
};
