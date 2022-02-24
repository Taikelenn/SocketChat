#pragma once

#include <string>
#include <vector>

template <typename T>
struct is_serializable : std::integral_constant<bool, std::is_arithmetic<T>::value || std::is_enum<T>::value> {};

class NetPacket {
private:
	std::vector<uint8_t> data;
	int readIterator;

public:
	NetPacket();
	NetPacket(unsigned char* buf, unsigned int length);
	~NetPacket();

	inline size_t GetLength() { return data.size(); }
	inline uint8_t* GetData() { return data.data(); }
	inline void SetReadIterator(int newIter) { readIterator = newIter; }

	template <typename T>
	T ReadField()
	{
		static_assert(is_serializable<T>::value, "Deserialization to this type is unsupported");

		unsigned char tmp[sizeof(T)];

		for (int i = 0; i < sizeof(T); ++i)
		{
			tmp[i] = data.at(readIterator++);
		}

		T retval;
		memcpy(&retval, tmp, sizeof(T));

		return retval;
	}

	template <typename T>
	void WriteField(T buf)
	{
		static_assert(is_serializable<T>::value, "Serialization of this type is unsupported");

		unsigned char tmp[sizeof(T)];
		memcpy(tmp, &buf, sizeof(T));

		for (int i = 0; i < sizeof(T); ++i)
		{
			data.push_back(tmp[i]);
		}
	}

	std::string ReadString();
	void WriteString(const std::string& s);

	void ReadByteArray(uint8_t* data, size_t size);
	void WriteByteArray(uint8_t* data, size_t size);
};
