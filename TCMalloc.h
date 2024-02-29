#pragma once
#include "Common.h"
#include "MemoryPool.h"
#include <stdint.h>

template<int BITS>
class TCMalloc_One {
public:
	explicit TCMalloc_One() {
		size_t size = sizeof(void*) << BITS;
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		m_Array = (void**)SystemAlloc(alignSize >> PAGE_SHIFT);
		memset(m_Array, 0, size);
	}

	void set(uint32_t key, void* v) {
		m_Array[key] = v;
	}

	void* get(uint32_t key) const {
		if ((key >> BITS) > 0) {
			return nullptr;
		}
		return m_Array[key];
	}

private:
	static const int LENGTH = 1 << BITS;
	void** m_Array;
};

template<int BITS>
class TCMalloc_Two {
public:
	explicit TCMalloc_Two() {
		memset(m_Root, 0, sizeof(m_Root));

		PreallocateMoreMemory();
	}

	void* get(uint32_t k) const {
		const uint32_t i1 = k >> LEAF_BITS;        // 获取高5位
		const uint32_t i2 = k & (LEAF_LENGTH - 1); // 获取低14位
		if ((k >> BITS) > 0 || m_Root[i1] == NULL)
			return NULL;
		return m_Root[i1]->values[i2];
	}

	void set(uint32_t k, void* v) {
		const uint32_t i1 = k >> LEAF_BITS;
		const uint32_t i2 = k & (LEAF_LENGTH - 1);
		ASSERT(i1 < ROOT_LENGTH);
		m_Root[i1]->values[i2] = v;
	}

	bool Ensure(uint32_t start, size_t n) {
		for (uint32_t key = start; key <= start + n - 1;) {
			const uint32_t i1 = key >> LEAF_BITS;

			if (i1 >= ROOT_LENGTH)
				return false;

			if (m_Root[i1] == NULL) {
				static MemoryPool memoryPool;
				Leaf* leaf = MemoryPool<Leaf*>.New();
				memset(leaf, 0, sizeof(*leaf));
				m_Root[i1] = leaf;
			}

			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void PreallocateMoreMemory() {
		Ensure(0, 1 << BITS);
	}

private:
	static const int ROOT_BITS = 5;
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	struct Leaf {
		void* Values[LEAF_LENGTH];
	};

	Leaf* m_Root[ROOT_LENGTH];
};