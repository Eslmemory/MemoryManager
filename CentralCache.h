#pragma once
#include "Common.h"

class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &s_Instance;
	}

	Span* GetOneSpan(SpanList& list, size_t byteSize);

	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
	void ReleaseListToSpan(void* start, size_t size);

private:
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;

private:
	SpanList m_SpanList[NFREELIST];
	static CentralCache s_Instance;
};