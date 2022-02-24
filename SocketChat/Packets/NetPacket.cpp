#include "NetPacket.h"

NetPacket::NetPacket()
{
	this->readIterator = 0;
	this->data.reserve(128);
}

NetPacket::NetPacket(unsigned char* buf, unsigned int length)
{
	this->readIterator = 0;
	this->data.resize(length);
	memcpy(this->data.data(), buf, length);
}

NetPacket::~NetPacket()
{
}

std::string NetPacket::ReadString()
{
	int stringLength = this->ReadField<int>();
	
	std::string s;
	s.resize(stringLength);

	for (size_t i = 0; i < s.size(); ++i)
	{
		s[i] = this->ReadField<char>();
	}

	return s;
}

void NetPacket::WriteString(const std::string& s)
{
	this->WriteField<int>(s.size());

	for (size_t i = 0; i < s.size(); ++i)
	{
		this->WriteField<char>(s[i]);
	}
}

void NetPacket::ReadByteArray(uint8_t* dataPtr, size_t size)
{
	memcpy(dataPtr, this->data.data() + readIterator, size);
	readIterator += size;
}

void NetPacket::WriteByteArray(uint8_t* dataPtr, size_t size)
{
	this->data.resize(this->data.size() + size);
	memcpy(this->data.data() + this->data.size() - size, dataPtr, size);
}
