#include <vector>
#include <chrono>
#include "MemoryPool.h"
#include "TestAlloc.h"

namespace Test {
	struct TreeNode {
		int Val;
		TreeNode* Left;
		TreeNode* Right;

		TreeNode()
			: Val(0), Left(nullptr), Right(nullptr) {}
	};

	void TestMemoryPool() {
		MemoryPool<TreeNode> pool;
		const int Rounds = 5;
		const int N = 100000;

		std::vector<TreeNode*> v1;
		v1.reserve(N);

		auto v1Start = std::chrono::high_resolution_clock::now();
		for (uint32_t i = 0; i < Rounds; i++) {
			// alloc
			for (uint32_t j = 0; j < N; j++) {
				v1.push_back(new TreeNode());
			}
			// free
			for (uint32_t j = 0; j < N; j++) {
				delete v1[j];
			}
			v1.clear();
		}
		auto v1End = std::chrono::high_resolution_clock::now();
		float duration1 = 0.001f * 0.001f * std::chrono::duration_cast<std::chrono::nanoseconds>(v1End - v1Start).count();

		auto v2Start = std::chrono::high_resolution_clock::now();
		std::vector<TreeNode*> v2;
		v2.reserve(N);
		for (uint32_t i = 0; i < Rounds; i++) {
			// alloc
			for (uint32_t j = 0; j < N; j++) {
				v2.push_back(pool.New());
			}
			// free
			for (uint32_t j = 0; j < N; j++) {
				pool.Delete(v2[j]);
			}
		}
		auto v2End = std::chrono::high_resolution_clock::now();
		float duration2 = 0.001f * 0.001f * std::chrono::duration_cast<std::chrono::nanoseconds>(v2End - v2Start).count();

		printf("new/delete cost time: %fms \n", duration1);
		printf("MemoryPool New/Delete cost time: %fms \n", duration2);
	}
}

