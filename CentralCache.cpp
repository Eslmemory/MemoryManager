#include "PageCache.h"
#include "CentralCache.h"

CentralCache CentralCache::s_Instance;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
    Span* it = list.Begin();
    while (it != list.End()) {
        if (it->FreeList != nullptr)
            return it;
        else
            it = it->Next;
    }

    list.m_Mtx.unlock();

    // get span from pageCache
    PageCache::GetInstance()->m_PageMtx.lock();
    Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
    span->IsUse = true;
    span->ObjSize = size;
    PageCache::GetInstance()->m_PageMtx.unlock();

    char* start = (char*)(span->PageID << PAGE_SHIFT);
    size_t bytes = span->N << PAGE_SHIFT;
    char* end = start + bytes;

    span->FreeList = start;
    start += size;
    void* tail = span->FreeList;
    int i = 1;
    while (start < end) {
        i++;
        NextObj(tail) = start;
        tail = NextObj(tail);
        start += size;
    }
    NextObj(tail) = nullptr;
    list.m_Mtx.lock();
    list.PushFront(span);

    return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size) {
    size_t index = SizeClass::Index(size);
    m_SpanList[index].m_Mtx.lock();
    Span* span = GetOneSpan(m_SpanList[index], size);
    assert(span);
    assert(span->FreeList);

    start = span->FreeList;
    end = start;

    size_t i = 0;
    size_t actualNum = 1;
    // 获取到batchNum后的end
    while (i < batchNum - 1 && NextObj(end) != nullptr) {
        end = NextObj(end);
        i++;
        actualNum++;
    }
    span->FreeList = NextObj(end);
    NextObj(end) = nullptr;
    span->UseCount += actualNum;

    m_SpanList[index].m_Mtx.unlock();
    return actualNum;
}

void CentralCache::ReleaseListToSpan(void* start, size_t size) {
    size_t index = SizeClass::Index(size);
    m_SpanList[index].m_Mtx.lock();
    while (start) {
        void* next = NextObj(start);
        Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
        NextObj(start) = span->FreeList;
        span->FreeList = start;
        span->UseCount--;

        if (span->UseCount == 0) {
            m_SpanList[index].Erase(span);
            span->FreeList = nullptr;
            span->Next = nullptr;
            span->Prev = nullptr;
            m_SpanList[index].m_Mtx.unlock();

            PageCache::GetInstance()->m_PageMtx.lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->m_PageMtx.unlock();

            m_SpanList[index].m_Mtx.lock();
        }

        start = next;
    }

    m_SpanList[index].m_Mtx.unlock();
}
