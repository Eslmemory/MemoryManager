#include "CentralCache.h"
#include "ThreadCache.h"

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	// ÂýÔö³¤Ëã·¨
	size_t batchNum = min(m_FreeList[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (m_FreeList[index].MaxSize() == batchNum) {
		m_FreeList[index].MaxSize() += 1;
	}

	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);

	assert(actualNum > 0);
	if (actualNum == 1) {
		assert(start == end);
		return start;
	}
	else {
		m_FreeList[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size) {
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());
	CentralCache::GetInstance()->ReleaseListToSpan(start, size);
}

void* ThreadCache::Allocate(size_t size) {
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!m_FreeList[index].Empty()) {
		return m_FreeList[index].Pop();
	}
	else {
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(ptr);
	assert(size <= MAX_SIZE);

	size_t index = SizeClass::Index(size);
	m_FreeList[index].Push(ptr);

	if (m_FreeList[index].Size() >= m_FreeList[index].MaxSize()) {
		ListTooLong(m_FreeList[index], size);
	}
}