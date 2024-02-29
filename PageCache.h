#pragma once
#include "Common.h"
#include "MemoryPool.h"
#include "TCMalloc.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &s_Instance;
	}

	Span* MapObjectToSpan(void* obj);
	void ReleaseSpanToPageCache(Span* span);
	Span* NewSpan(size_t k); // k is the page counts

private:
	PageCache() {}
	PageCache(const PageCache&) = delete;

public:
	std::mutex m_PageMtx;
private:
	SpanList m_SpanList[NPAGES];
	MemoryPool<Span> m_SpanPool;
	TCMalloc_One<32 - PAGE_SHIFT> m_IdSpanMap;
	//std::unordered_map<PAGE_ID, Span*> m_IdSpanMap;
	static PageCache s_Instance;
};