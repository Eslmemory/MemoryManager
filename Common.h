#pragma once
#include <stdlib.h>
#include <Windows.h>
#include <assert.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>

const size_t MAX_SIZE = 256 * 1024; // 256kb
const size_t PAGE_SHIFT = 13;
const size_t NFREELIST = 208;
const size_t NPAGES = 128;

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#else
	// linux
#endif

inline void* SystemAlloc(size_t kpage) {
	// MEM_RESERVE预留，申请虚拟内存，但不提交物理内存   MEM_COMMIT提交物理内存
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// (linux下的brk/mmap)
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

inline void SystemFree(void* ptr) {
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// (linux下的sbrk/unmmap)
#endif
}

static void*& NextObj(void* obj) {
	return *((void**)obj);
}

// --------------FreeList--------------
class FreeList {
public:
	// 头插
	void Push(void* obj) {
		assert(obj);

		NextObj(obj) = m_FreeList;
		m_FreeList = obj;

		++m_Size;
	}

	void PushRange(void* start, void* end, size_t n) {
		NextObj(end) = m_FreeList;
		m_FreeList = start;
		m_Size += n;
	}

	void* Pop() {
		assert(m_FreeList);

		void* obj = m_FreeList;
		m_FreeList = NextObj(m_FreeList);

		--m_Size;
		return obj;
	}

	void PopRange(void*& start, void*& end, size_t n) {
		assert(n <= m_Size);

		start = m_FreeList;
		end = start;
		for (size_t i = 0; i < n - 1; i++) {
			end = NextObj(end);
		}

		m_FreeList = NextObj(end);

		NextObj(end) = nullptr;
		m_Size -= n;
	}

	bool Empty() {
		return m_FreeList == nullptr;
	}

	size_t& MaxSize() {
		return m_MaxSize;
	}

	size_t Size() {
		return m_Size;
	}

private:
	size_t m_Size = 0;
	size_t m_MaxSize = 1;
	void* m_FreeList = nullptr;
};

// --------------Span--------------
struct Span {
	PAGE_ID PageID = 0;        // 页号
	size_t N = 0;              // 页的数量

	Span* Next = nullptr;
	Span* Prev = nullptr;

	size_t ObjSize = 0;        // 切好的小对象大小
	size_t UseCount = 0;       // 已分配给ThreadCache的小块内存的数量
	void* FreeList = nullptr;

	bool IsUse = false;        // 当前span内存跨度是否在被使用
};

class SpanList {
public:
	SpanList() {
		m_Head = new Span;
		m_Head->Next = m_Head;
		m_Head->Prev = m_Head;
	}

	Span* Begin() {
		return m_Head->Next;
	}

	Span* End() {
		return m_Head;
	}

	bool Empty() {
		return m_Head->Next == m_Head;
	}

	void PushFront(Span* span) {
		Insert(Begin(), span);
	}

	Span* PopFront() {
		Span* front = m_Head->Next;
		Erase(front);
		return front;
	}

	void Insert(Span* pos, Span* newSpan) {
		assert(pos);
		assert(newSpan);

		Span* prev = pos->Prev;
		prev->Next = newSpan;
		newSpan->Prev = prev;
		newSpan->Next = pos;
		pos->Prev = newSpan;
	}

	void Erase(Span* pos) {
		assert(pos);
		assert(pos != m_Head);

		// Span* prev = pos->Prev;
		// prev->Next = pos->Next;
		// pos->Next->Prev = prev;

		Span* prev = pos->Prev;
		Span* next = pos->Next;

		prev->Next = next;
		next->Prev = prev;
	}

public:
	std::mutex m_Mtx;
private:
	Span* m_Head;
};

class SizeClass {
public:
	static inline size_t _RoundUp(size_t size, size_t alignNum) {
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}
	
	static inline size_t RoundUp(size_t size) {
		if (size <= 128) {
			return _RoundUp(size, 8);
		}
		else if (size <= 1024) {
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)  {
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024) {
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024) {
			return _RoundUp(size, 8 * 1024);
		}
		else {
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	static inline size_t _Index(size_t size, size_t align_shift) {
		return (size + (1 << align_shift) - 1) >> align_shift - 1;
	}

	static inline size_t Index(size_t size) {
		assert(size <= MAX_SIZE);

		static int groupArray[4] = { 16, 56, 56, 56 };
		if (size <= 128) {
			return _Index(size, 3);
		}
		else if (size <= 1024) {
			return _Index(size - 128, 4) + groupArray[0];
		}
		else if (size <= 8 * 1024) {
			return _Index(size - 1024, 7) + groupArray[1] + groupArray[0];
		}
		else if (size <= 64 * 1024) {
			return _Index(size - 8 * 1024, 10) + groupArray[2] + groupArray[1] + groupArray[0];
		}
		else if (size <= 256 * 1024) {
			return _Index(size - 64 * 1024, 13) + groupArray[3] + groupArray[2] + groupArray[1] + groupArray[0];
		}
		else {
			assert(false);
		}
		return -1;
	}

	static size_t NumMoveSize(size_t size) {
		assert(size > 0);
		int num = MAX_SIZE / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	static size_t NumMovePage(size_t size) {
		assert(size > 0);
		size_t num = NumMoveSize(size);
		size_t npage = num * size;

		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}

};