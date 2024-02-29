#include "Common.h"
#include "TestAlloc.h"
#include "ConcurrentAlloc.h"
#include <chrono>

// ntimes��ÿ���߳�ÿ�ִε��� malloc �����Ĵ���
// nworks���߳�����
// rounds��ÿ���߳�ִ�е��ִ���

void Malloc(size_t ntimes, size_t nworks, size_t rounds) {
	std::vector<std::thread> thread1(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t i = 0; i < nworks; i++) {
		thread1[i] = std::thread([&, i]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; j++) {
				auto begin1 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					v.push_back(malloc(16));
				}
				auto end1 = std::chrono::high_resolution_clock::now();
				
				auto begin2 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					free(v[k]);
				}
				auto end2 = std::chrono::high_resolution_clock::now();
				v.clear();

				malloc_costtime += 0.001f * 0.001f * std::chrono::duration_cast<std::chrono::nanoseconds>((end1 - begin1)).count();
				free_costtime += 0.001f * 0.001f * std::chrono::duration_cast<std::chrono::nanoseconds>((end2 - begin2)).count();
			}
		});
	}

	for (auto& t : thread1) {
		t.join();
	}

	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�malloc" << ntimes << "��: ���ѣ�" << malloc_costtime << "ms" << std::endl;
	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�free" << ntimes << "��: ���ѣ�" << free_costtime << "ms" << std::endl;
	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�malloc&free" << ntimes << "��: ���ѣ�" << malloc_costtime + free_costtime << "ms" << std::endl;
}

void ConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds) {
	std::vector<std::thread> thread2(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t i = 0; i < nworks; ++i) {
			thread2[i] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j) {
				auto begin1 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					v.push_back(ConcurrentAlloc(16));
				}
				auto end1 = std::chrono::high_resolution_clock::now();

				auto begin2 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					ConcurrentFree(v[k]);
				}
				auto end2 = std::chrono::high_resolution_clock::now();
				v.clear();

				malloc_costtime += 0.001f * 0.001f * std::chrono::duration_cast<std::chrono::nanoseconds>((end1 - begin1)).count();
				free_costtime += 0.001f * 0.001f * std::chrono::duration_cast<std::chrono::nanoseconds>((end2 - begin2)).count();
			}
		});
	}

	for (auto& t : thread2) {
		t.join();
	}
	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�concurrent alloc" << ntimes << "��: ���ѣ�" << malloc_costtime << "ms" << std::endl;
	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�concurrent dealloc" << ntimes << "��: ���ѣ�" << free_costtime << "ms" << std::endl;
	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�concurrent alloc&dealloc" << ntimes << "��: ���ѣ�" << malloc_costtime + free_costtime << "ms" << std::endl;
}

int main(int argc, char* argv[]) {
	size_t n = 1000;
	std::cout << "==========================================================" << std::endl;
	Malloc(n, 4, 10);
	std::cout << std::endl << std::endl;

	ConcurrentMalloc(n, 4, 10);
	std::cout << "==========================================================" << std::endl;
	return 0;
}