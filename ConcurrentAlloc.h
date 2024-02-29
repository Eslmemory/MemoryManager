#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "MemoryPool.h"

static void* ConcurrentAlloc(size_t size) {
	if (size > MAX_SIZE) {
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kPage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->m_PageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kPage);
		span->ObjSize = size;
		PageCache::GetInstance()->m_PageMtx.unlock();

		void* ptr = (void*)(span->PageID << PAGE_SHIFT);
		return ptr;
	}
	else {
		if (pTLSThreadCache == nullptr) {
			static MemoryPool<ThreadCache> pool;
			pTLSThreadCache = pool.New();
		}
		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr) {
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->ObjSize;
	if (size > MAX_SIZE) {
		PageCache::GetInstance()->m_PageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->m_PageMtx.unlock();
	}
	else {
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}