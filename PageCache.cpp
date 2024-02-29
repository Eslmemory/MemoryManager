#include "PageCache.h"

PageCache PageCache::s_Instance;

// ����kֱ�Ӻ�m_SpanListͰ������Ӧ������m_SpanList[0]���߼����ܹ���Ӧ
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	if (k > NPAGES - 1) {
		void* ptr = SystemAlloc(k);
		Span* span = m_SpanPool.New();
		span->PageID = (PAGE_ID)ptr >> PAGE_SHIFT; // ҳ��
		span->N = k; // ҳ��
		
		m_IdSpanMap.set(span->PageID, span);
		// m_IdSpanMap[span->PageID] = span;
		return span;
	}

	if (!m_SpanList[k].Empty()) {
		Span* kSpan = m_SpanList[k].PopFront();

		for (PAGE_ID i = 0; i < kSpan->N; i++) {
			m_IdSpanMap.set(kSpan->PageID + i, kSpan);
			// m_IdSpanMap[kSpan->PageID + i] = kSpan;
		}

		return kSpan;
	}

	// �鿴�����Ƿ��и����span
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!m_SpanList[i].Empty()) {
			Span* nSpan = m_SpanList[i].PopFront();
			Span* kSpan = m_SpanPool.New();
			kSpan->PageID = nSpan->PageID;
			kSpan->N = k;

			nSpan->PageID += k;
			nSpan->N -= k;

			m_SpanList[nSpan->N].PushFront(nSpan);

			m_IdSpanMap.set(nSpan->PageID, nSpan);
			m_IdSpanMap.set(nSpan->PageID + nSpan->N - 1, kSpan);
			// m_IdSpanMap[nSpan->PageID] = nSpan;
			// m_IdSpanMap[nSpan->PageID + nSpan->N - 1] = kSpan;

			for (PAGE_ID i = 0; i < kSpan->N; i++) {
				m_IdSpanMap.set(kSpan->PageID + i, kSpan);
				// m_IdSpanMap[kSpan->PageID + i] = kSpan;
			}

			return kSpan;
		}
	}

	// pagecache�Ѿ�û�к��ʵĿɷ�����ڴ棬ȥ��������
	Span* bigSpan = m_SpanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->PageID = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->N = NPAGES - 1;

	m_SpanList[bigSpan->N].PushFront(bigSpan);
	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;

	// std::unique_lock<std::mutex> lock(m_PageMtx); // ����ʱ�Զ�����������ʱ�Զ�����
	// auto ret = m_IdSpanMap.find(id);
	auto ret = (Span*)m_IdSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

void PageCache::ReleaseSpanToPageCache(Span* span) {
	if (span->N > NPAGES - 1) {
		void* ptr = (void*)(span->PageID << PAGE_SHIFT);
		SystemFree(ptr);
		m_SpanPool.Delete(span);
		return;
	}

	// ��ǰ�ϲ�
	while (true) {
		PAGE_ID prevID = span->PageID - 1;
		// auto ret = m_IdSpanMap.find(prevID);
		auto ret = (Span*)m_IdSpanMap.get(prevID);
		if (ret == nullptr)
			break;
		
		// ǰ������ҳ��span��ʹ�ã����ϲ�
		// Span* prevSpan = ret->second;
		Span* prevSpan = ret;
		if (prevSpan->IsUse)
			break;

		// ����128ҳ�����ϲ�
		if (prevSpan->N + span->N > NPAGES - 1)
			break;

		span->PageID = prevSpan->PageID;
		span->N += prevSpan->N;

		m_SpanList[prevSpan->N].Erase(prevSpan);
		m_SpanPool.Delete(prevSpan);
	}

	// ���ϲ�
	while (true) {
		PAGE_ID nextID = span->PageID + span->N;
		// auto ret = m_IdSpanMap.find(nextID);
		auto ret = (Span*)m_IdSpanMap.get(nextID);
		if (ret == nullptr)
			break;

		Span* nextSpan = ret;
		if (nextSpan->IsUse)
			break;

		if (nextSpan->N + span->N > NPAGES - 1)
			break;

		span->N += nextSpan->N;
		m_SpanList[nextSpan->N].Erase(nextSpan);
		m_SpanPool.Delete(nextSpan);
	}

	m_SpanList[span->N].PushFront(span);
	span->IsUse = false;

	m_IdSpanMap.set(span->PageID, span);
	m_IdSpanMap.set(span->PageID + span->N - 1, span);
	// m_IdSpanMap[span->PageID] = span;
	// m_IdSpanMap[span->PageID + span->N - 1] = span;
}
