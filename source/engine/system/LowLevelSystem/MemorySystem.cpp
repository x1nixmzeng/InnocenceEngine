#include "MemorySystem.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include "../LowLevelSystem/LogSystem.h"
#include "TimeSystem.h"

namespace InnoMemorySystem
{
	static const uint32_t s_BlockSizes[] = {
		// 4-increments
		4,  8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48,
		52, 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96,

		// 32-increments
		128, 160, 192, 224, 256, 288, 320, 352, 384,
		416, 448, 480, 512, 544, 576, 608, 640,

		// 64-increments
		704, 768, 832, 896, 960, 1024
	};

	// number of elements in the block size array
	static const uint32_t s_NumBlockSizes = sizeof(s_BlockSizes) / sizeof(s_BlockSizes[0]);

	// largest valid block size
	static const uint32_t s_MaxBlockSize = s_BlockSizes[s_NumBlockSizes - 1];

	const unsigned long  m_maxPoolSize = 1024 * 1024 * 512;
	static const unsigned int m_minFreeBlockSize = 48;
	unsigned long  m_totalPoolSize;
	unsigned long  m_availablePoolSize;

	static const unsigned int m_boundCheckSize = 16;
	unsigned char  m_startBoundMarker[m_boundCheckSize] = { '[','I','n','n','o','C','h','u','c','k','S','t','a','r','t',']' };
	unsigned char  m_endBoundMarker[m_boundCheckSize] = { '[','I','n','n','o','C','h','u','c','k','.','.','E','n','d',']' };

	unsigned char* m_poolMemoryPtr = nullptr;

	class Chunk
	{
	public:
		Chunk(unsigned int chuckSize) : m_next(nullptr),
			m_prev(nullptr),
			m_blockSize(chuckSize),
			m_free(true) {};

		Chunk*  m_next;
		Chunk*  m_prev;
		unsigned int   m_blockSize;
		bool    m_free;
	};

	objectStatus m_MemorySystemStatus = objectStatus::SHUTDOWN;
}

InnoLowLevelSystem_EXPORT bool InnoMemorySystem::setup()
{
	// Allocate memory pool
	m_poolMemoryPtr = nullptr;
	m_poolMemoryPtr = ::new unsigned char[m_maxPoolSize];
	std::memset(m_poolMemoryPtr, 0xCC, m_maxPoolSize);
	m_totalPoolSize = m_maxPoolSize;

	// first free chuck
	Chunk l_freeChunk(m_maxPoolSize);
	std::memcpy(m_poolMemoryPtr, m_startBoundMarker, m_boundCheckSize);
	std::memcpy(m_poolMemoryPtr + m_boundCheckSize, &l_freeChunk, sizeof(Chunk));
	std::memcpy(m_poolMemoryPtr + m_maxPoolSize - m_boundCheckSize, m_endBoundMarker, m_boundCheckSize);
	m_availablePoolSize = m_maxPoolSize - sizeof(Chunk) - m_boundCheckSize * 2;
	return true;
}

InnoLowLevelSystem_EXPORT bool InnoMemorySystem::initialize()
{
	m_MemorySystemStatus = objectStatus::ALIVE;
	InnoLogSystem::printLog("MemorySystem has been initialized.");
	return true;
}

InnoLowLevelSystem_EXPORT bool InnoMemorySystem::update()
{
	return true;
}

InnoLowLevelSystem_EXPORT bool InnoMemorySystem::terminate()
{
	::delete[] m_poolMemoryPtr;
	m_MemorySystemStatus = objectStatus::ALIVE;
	InnoLogSystem::printLog("MemorySystem has been terminated.");
	return true;
}

InnoLowLevelSystem_EXPORT void * InnoMemorySystem::allocate(unsigned long size)
{
	// add bound check size
	// [StartBound + Chuck + data + EndBound]
	unsigned long l_requiredSize = size + sizeof(Chunk) + m_boundCheckSize * 2;
	// alignment to 4
	// [StartBound + Chuck + data + EndBound + alignment]
	for (size_t i = 0; i < s_NumBlockSizes; i++)
	{
		if (l_requiredSize <= s_BlockSizes[i])
		{
			l_requiredSize = s_BlockSizes[i];
			break;
		}
	}
	// Now search for a block big enough from the beginning of the pool, double linked list, O(n)
	//[                 suitable block                 ] is larger than
	//[StartBound + Chuck + data + EndBound + alignment]
	Chunk* l_suitableChuckPtr = reinterpret_cast<Chunk*>(m_poolMemoryPtr + m_boundCheckSize);
	while (l_suitableChuckPtr)
	{
		if (l_suitableChuckPtr->m_free && l_suitableChuckPtr->m_blockSize >= l_requiredSize) { break; }
		l_suitableChuckPtr = l_suitableChuckPtr->m_next;
	}

	// If no block is found, return nullptr
	if (!l_suitableChuckPtr)
	{ 
		InnoLogSystem::printLog("MemorySystem: Can't allocate memory!");
		return nullptr; 
	}

	// If the block is valid, create a new free block with
	// what remains of the block memory
	unsigned long l_freeBlockSize = l_suitableChuckPtr->m_blockSize - l_requiredSize;
	//[      suitable block       ] is divided to
	//[required block + free block]
	unsigned char* l_suitableChuckPtr_UC = reinterpret_cast<unsigned char*>(l_suitableChuckPtr);
	Chunk* l_freeChuckPtr = reinterpret_cast<Chunk*>(l_suitableChuckPtr_UC + l_requiredSize);
	Chunk l_freeChuck(l_freeBlockSize);
	// assign the next block's prev pointer to this free block
	l_freeChuck.m_prev = l_suitableChuckPtr;
	l_freeChuck.m_next = l_suitableChuckPtr->m_next;
	if (l_freeChuck.m_next)
	{
		l_freeChuck.m_next->m_prev = l_freeChuckPtr;
	}

	// set allocated block chuck data
	l_suitableChuckPtr->m_next = l_freeChuckPtr;
	l_suitableChuckPtr->m_blockSize = l_requiredSize;
	l_suitableChuckPtr->m_free = false;

	// Fill the block with end bound marker and filling flags

	std::memset(l_suitableChuckPtr_UC + sizeof(Chunk), 0xAB, size);
	std::memcpy(l_suitableChuckPtr_UC - m_boundCheckSize + l_requiredSize - m_boundCheckSize, m_endBoundMarker, m_boundCheckSize);
	
	unsigned char* l_freeChuckPtr_UC = reinterpret_cast<unsigned char*>(l_freeChuckPtr);
	std::memcpy(l_freeChuckPtr_UC - m_boundCheckSize, m_startBoundMarker, m_boundCheckSize);
	std::memcpy(l_freeChuckPtr_UC, &l_freeChuck, sizeof(Chunk));
	std::memcpy(l_freeChuckPtr_UC - m_boundCheckSize + l_freeBlockSize - m_boundCheckSize, m_endBoundMarker, m_boundCheckSize);

	// update the pool size
	m_availablePoolSize -= l_requiredSize;

	return (l_suitableChuckPtr_UC + sizeof(Chunk));
}

InnoLowLevelSystem_EXPORT void InnoMemorySystem::free(void * ptr)
{
	// is a valid node?
	if (!ptr) return;
	Chunk* l_chuckPtr = reinterpret_cast<Chunk*>((unsigned char*)ptr - sizeof(Chunk));
	if (l_chuckPtr->m_free) return;

	unsigned long l_fullFreeBlockSize = l_chuckPtr->m_blockSize;
	m_availablePoolSize += l_fullFreeBlockSize;

	Chunk* l_freeChuckPtr = l_chuckPtr;
	Chunk* l_prevChuckPtr = l_chuckPtr->m_prev;
	Chunk* l_nextChuckPtr = l_chuckPtr->m_next;

	if (l_prevChuckPtr)
	{
		// If the node before is free merge it with this one
		if (l_prevChuckPtr->m_free)
		{
			l_freeChuckPtr = l_prevChuckPtr;
			if (l_freeChuckPtr->m_prev)
			{
				l_prevChuckPtr = l_freeChuckPtr->m_prev;
			}
			else
			{
				l_prevChuckPtr = nullptr;
			}

			// Include the prev node in the block size so we trash it as well
			l_fullFreeBlockSize += l_prevChuckPtr->m_blockSize;
		}
	}
	// If there is a next one, we need to update its pointer
	if (l_nextChuckPtr)
	{
		// We will re point the next of the freer's prev
		l_nextChuckPtr->m_prev = l_freeChuckPtr;

		// Include the next node in the block size if it is
		// free so we trash it as well
		if (l_nextChuckPtr->m_free)
		{
			l_fullFreeBlockSize += l_nextChuckPtr->m_blockSize;

			// We will point to next's next
			if (l_nextChuckPtr->m_next)
			{
				l_nextChuckPtr = l_nextChuckPtr->m_next;
				l_nextChuckPtr->m_prev = l_freeChuckPtr;
			}
			else
			{
				l_nextChuckPtr = nullptr;
			}
		}
	}
	// Create the free block
	unsigned char* l_freeChuckPtr_UC = reinterpret_cast<unsigned char*>(l_freeChuckPtr);

	Chunk l_freeChuck(l_fullFreeBlockSize);
	l_freeChuck.m_prev = l_prevChuckPtr;
	l_freeChuck.m_next = l_nextChuckPtr;
	std::memcpy(l_freeChuckPtr_UC - m_boundCheckSize, m_startBoundMarker, m_boundCheckSize);
	std::memcpy(l_freeChuckPtr_UC, &l_freeChuck, sizeof(Chunk));
	std::memset(l_freeChuckPtr_UC + sizeof(Chunk), 0xCC, l_fullFreeBlockSize - sizeof(Chunk) - m_boundCheckSize * 2);
	std::memcpy(l_freeChuckPtr_UC + l_fullFreeBlockSize - m_boundCheckSize * 2, m_endBoundMarker, m_boundCheckSize);
}

InnoLowLevelSystem_EXPORT void InnoMemorySystem::serializeImpl(void * ptr)
{
	if (!ptr) return;
	char* l_ptr_UC = reinterpret_cast<char*>(ptr);
	Chunk* l_chuckPtr = reinterpret_cast<Chunk*>(l_ptr_UC - sizeof(Chunk));
	if (l_chuckPtr->m_free) return;

	unsigned long l_fullBlockSize = l_chuckPtr->m_blockSize;

	std::ofstream l_file;
	//l_file.open("../serializationTest" + InnoTimeSystem->getCurrentTimeInLocalForOutput() + ".innoAsset", std::ios::out | std::ios::trunc | std::ios::binary);
	l_file.open("../serializationTest.innoAsset", std::ios::out | std::ios::trunc | std::ios::binary);
	l_file.write(l_ptr_UC, l_fullBlockSize - sizeof(Chunk) - m_boundCheckSize * 2);
	l_file.close();
}

InnoLowLevelSystem_EXPORT void * InnoMemorySystem::deserializeImpl(unsigned long size, const std::string & filePath)
{
	std::ifstream l_file;
	l_file.open(filePath, std::ios::binary);
	// get pointer to associated buffer object
	std::filebuf* pbuf = l_file.rdbuf();

	// get file size using buffer's members
	std::size_t l_size = pbuf->pubseekoff(0, l_file.end, l_file.in);
	pbuf->pubseekpos(0, l_file.in);

	// allocate memory to contain file data
	char* buffer = new char[l_size];

	// get file data
	pbuf->sgetn(buffer, l_size);
	unsigned char* buffer_UC = reinterpret_cast<unsigned char*>(buffer);

	unsigned char* l_ptr = reinterpret_cast<unsigned char*>(allocate(size));
	
	std::memcpy(l_ptr, buffer_UC, size);
	
	l_file.close();

	return l_ptr;
}

InnoLowLevelSystem_EXPORT void InnoMemorySystem::dumpToFile(bool fullDump)
{
	std::ofstream l_file;
	l_file.open("../" + InnoTimeSystem::getCurrentTimeInLocalForOutput() + ".innoMemoryDump");

	l_file << "InnoMemory Pool Dump File ----------------------------------\n";
	l_file << "Total Size: " << m_totalPoolSize << "\n";
	l_file << "Free Size: " << m_availablePoolSize << "\n";

	Chunk* block = reinterpret_cast<Chunk*>(m_poolMemoryPtr + m_boundCheckSize);

	while (block)
	{
		if (block->m_free)
		{
			l_file << "Free: " << block << "[Bytes: " << block->m_blockSize << "]\n";
		}
		else
		{
			l_file << "Used: " << block << "[Bytes: " << block->m_blockSize << "]\n";
		}
		block = block->m_next;
	}

	if (fullDump)
	{
		l_file << "\n\nMemory Dump:\n";
		unsigned char* ptr = m_poolMemoryPtr;
		unsigned char* charPtr = m_poolMemoryPtr;

		l_file << "Start: " << ptr << "\n";
		unsigned char i = 0;

		// Write the hex memory data
		unsigned long long bytesPerLine = 32;

		l_file << "\n" << ptr << ": " << *(ptr);
		++ptr;
		for (i = 1; ((unsigned long long)(ptr - m_poolMemoryPtr) < m_totalPoolSize); ++i, ++ptr)
		{
			if (i == bytesPerLine)
			{
				// Write all the chars for this line now
				l_file << "  " << charPtr;
				for (unsigned long long charI = 0; charI < bytesPerLine; ++charI, ++charPtr)
					l_file << *charPtr;
				charPtr = ptr;

				// Write the new line memory data
				l_file << "\n" << ptr << ": " << *(ptr);
				i = 0;
			}
			else
				l_file << *(ptr);
		}

		// Fill any gaps in the tab
		if ((unsigned long long)(ptr - m_poolMemoryPtr) >= m_totalPoolSize)
		{
			unsigned long long lastLineBytes = i;
			for (i; i < bytesPerLine; i++)
				l_file << " --";

			// Write all the chars for this line now
			l_file << "  " << charPtr;
			for (unsigned long long charI = 0; charI < lastLineBytes; ++charI, ++charPtr)
				l_file << *charPtr;
			charPtr = ptr;
		}
	}

	l_file.close();
}

objectStatus InnoMemorySystem::getStatus()
{
	return m_MemorySystemStatus;
}