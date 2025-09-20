#include "framework.h"
#include "Specific/IO/Streams.h"

using std::ios;
using std::string;
using std::ifstream;
using std::fstream;
using std::ofstream;

bool BaseStream::ReadBytes(byte* value, int length)
{
	return Read(reinterpret_cast<char*>(value), length);
}

bool BaseStream::ReadByte(byte* value)
{
	return Read(reinterpret_cast<char*>(value), 1);
}

bool BaseStream::ReadBool(bool* value)
{
	return Read(reinterpret_cast<char*>(value), 1);
}

bool BaseStream::ReadInt16(short* value)
{
	return Read(reinterpret_cast<char*>(value), 2);
}

bool BaseStream::ReadInt32(int* value)
{
	return Read(reinterpret_cast<char*>(value), 4);
}

bool BaseStream::ReadFloat(float* value)
{
	return Read(reinterpret_cast<char*>(value), 4);
}

bool BaseStream::ReadString(char** value) 
{
	int length;
	ReadInt32(&length);
	*value = (char*)malloc(length + 1);
	Read(*value, length);
	(*value)[length] = NULL;

	return true;
}

bool BaseStream::ReadString(string* value) 
{
	int length;
	ReadInt32(&length);
	char* buffer = (char*)malloc(length + 1);
	Read(buffer, length);
	buffer[length] = NULL;
	*value = string(buffer);
	free(buffer);

	return true;
}

bool BaseStream::ReadVector2(Vector2* value)
{
	ReadFloat(&value->x);
	ReadFloat(&value->y);

	return true;
}

bool BaseStream::ReadVector3(Vector3* value)
{
	ReadFloat(&value->x);
	ReadFloat(&value->y);
	ReadFloat(&value->z);

	return true;
}

bool BaseStream::ReadVector4(Vector4* value)
{
	ReadFloat(&value->x);
	ReadFloat(&value->y);
	ReadFloat(&value->z);
	ReadFloat(&value->w);

	return true;
}

bool BaseStream::ReadQuaternion(Quaternion* value)
{
	ReadFloat(&value->x);
	ReadFloat(&value->y);
	ReadFloat(&value->z);
	ReadFloat(&value->w);

	return true;
}

bool BaseStream::ReadBoundingBox(BoundingBox* value)
{
	Vector3 minPos;
	Vector3 maxPos;

	ReadVector3(&minPos);
	ReadVector3(&maxPos);

	BoundingBox::CreateFromPoints(*value, minPos, maxPos);

	return true;
}

bool BaseStream::ReadBoundingSphere(BoundingSphere* sphere)
{
	Vector3 center;
	float radius;

	ReadVector3(&center);
	ReadFloat(&radius);

	sphere->Center = center;
	sphere->Radius = radius;

	return true;
}

bool BaseStream::WriteBytes(byte* value, int length)
{
	return Write(reinterpret_cast<char*>(value), length);
}

bool BaseStream::WriteByte(byte value)
{
	return Write(reinterpret_cast<char*>(&value), 1);
}

bool BaseStream::WriteInt16(short value)
{
	return Write(reinterpret_cast<char*>(&value), 2);
}

bool BaseStream::WriteInt32(int value)
{
	return Write(reinterpret_cast<char*>(&value), 4);
}

bool BaseStream::WriteBool(bool value)
{
	return Write(reinterpret_cast<char*>(&value), 1);
}

bool BaseStream::WriteFloat(float value)
{
	return Write(reinterpret_cast<char*>(&value), 4);
}

bool BaseStream::WriteString(char const* str)
{
	int length = (int)strlen(str);
	WriteInt32(length);
	Write(str, length);
	return true;
}

MemoryStream::MemoryStream(char* buffer, int size)
{
	m_buffer = (char*)malloc(size);
	m_startBuffer = m_buffer;
	memcpy(m_buffer, buffer, size);
	m_size = size;
}

MemoryStream::MemoryStream(int size)
{
	m_buffer = (char*)malloc(size);
	m_startBuffer = m_buffer;
	m_size = size;
}

MemoryStream::~MemoryStream()
{
	free(m_startBuffer);
}

bool MemoryStream::Read(char* buffer, int length)
{
	memcpy(buffer, m_buffer, length);
	m_buffer += length;
	return true;
}

bool MemoryStream::Write(char* buffer, int length)
{
	memcpy(m_buffer, buffer, length);
	m_buffer += length;
	return true;
}

int MemoryStream::GetCurrentPosition()
{
	return (m_buffer - m_startBuffer);
}

bool MemoryStream::Seek(int seek, SeekOrigin origin)
{
	if (origin == SeekOrigin::BEGIN)
		m_buffer = m_startBuffer + seek;
	else
		m_buffer += seek;

	return true;
}

bool MemoryStream::IsEOF()
{
	return (GetCurrentPosition() > m_size);
}

bool MemoryStream::Close()
{
	return true;
}

FileStream::FileStream(char* fileName, bool read, bool write)
{
	int mode = 0;
	if (read)
		mode |= ifstream::binary | fstream::in;

	if (write)
		mode |= ofstream::binary | fstream::out | fstream::trunc;

	m_stream.open(fileName, mode);
}

FileStream::~FileStream()
{
	m_stream.close();
}

bool FileStream::Read(char* buffer, int length)
{
	m_stream.read(buffer, length);
	return true;
}

bool FileStream::Write(char const* buffer, int length)
{
	m_stream.write(buffer, length);
	return true;
}

int FileStream::GetCurrentPosition()
{
	return (int)(m_stream.tellg());
}

bool FileStream::Seek(int seek, SeekOrigin origin)
{
	m_stream.seekg(seek, (origin == SeekOrigin::BEGIN ? m_stream.beg : m_stream.cur));
	return true;
}

bool FileStream::IsEOF()
{
	return (m_stream.eof());
}

bool FileStream::Close() 
{
	m_stream.flush();
	m_stream.close();
	return true;
}
