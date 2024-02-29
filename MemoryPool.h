#pragma once
#include "Common.h"

#define   MIN_ALLOC_SIZE   128 * 1024;

template<typename T>
class MemoryPool {
public:
	T* New() {
		T* obj = nullptr;
		if (m_FreeList) {
			void* next = NextObj(m_FreeList);
			obj = (T*)m_FreeList;
			m_FreeList = next;
		}
		else {
			if (m_RemainBytes < sizeof(T)) {
				m_RemainBytes = MIN_ALLOC_SIZE;
				m_MemoryPool = (char*)SystemAlloc(m_RemainBytes >> PAGE_SHIFT);

				if (m_MemoryPool == nullptr)
					std::bad_alloc();
			}

			obj = (T*)m_MemoryPool;
			size_t objSize = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
			m_MemoryPool += objSize;
			m_RemainBytes -= objSize;
		}
		new(obj) T;
		return obj;
	}

	void Delete(T* obj) {
		obj->~T();
		NextObj(obj) = m_FreeList;
		m_FreeList = obj;
	}

private:
	void* m_FreeList = nullptr;
	char* m_MemoryPool = nullptr;
	size_t m_RemainBytes = 0;
};