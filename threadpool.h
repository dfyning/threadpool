#pragma once

#include<mutex>
#include<vector>
#include<map>
#include<future>
#include<condition_variable>
#include<thread>
#include<functional>



class threadpool {
private:
	std::mutex mu;
	std::condition_variable stdcv;
	std::atomic_bool stop;
	std::vector<std::thread> workers;
	std::multimap<int, std::function<void()> > tasks;

public:
	threadpool(size_t numofthread);
	template<typename Fun, typename ... Args>
	auto pushonetask(int pro, Fun&& fun, Args&& ... args)
		->std::future<decltype(fun(args...))>;
	~threadpool();
};

threadpool::threadpool(size_t numofthread) : stop(false) {
	for (unsigned int i = 0; i < numofthread; i++) {
		workers.emplace_back([this]() {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(mu);
					this->stdcv.wait(lock, [this]() {return this->stop || !tasks.empty(); });
					if (this->stop && tasks.empty()) return;
					task = tasks.begin()->second;
					tasks.erase(tasks.begin());
				}
				task();
			}
			});
	}
}

template<typename Fun, typename ... Args>
auto threadpool::pushonetask(int pro, Fun&& fun, Args&& ... args)
->std::future<decltype(fun(args...))> {
	typedef decltype(fun(args...)) returntype;
	auto task = std::make_shared<std::packaged_task<returntype()>>
		(std::bind(std::forward<Fun>(fun), std::forward<Args>(args)...));
	std::future<returntype> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(mu);
		if (stop) {
			throw std::runtime_error("pushtask in a stopped threadpool");
		}
		tasks.insert(std::make_pair(pro, ([task]() { (*task)(); })));
	}
	stdcv.notify_one();
	return res;
}

threadpool::~threadpool() {
	stop = true;
	stdcv.notify_all();
	for (auto i = workers.begin(); i != workers.end(); i++) {
		i->join();
	}
}
