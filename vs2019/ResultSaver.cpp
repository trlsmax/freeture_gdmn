#include "ResultSaver.h"

ResultSaver::ResultSaver(void)
{
	running = true;
	thd = new std::thread(&ResultSaver::Run, *this);
	thd->detach();
}

ResultSaver::~ResultSaver(void)
{
	Terminate();
}

void ResultSaver::Terminate(void)
{
	running = false;
	if (thd && thd->joinable()) {
		thd->join();
	}
	delete thd;
	thd = nullptr;
}

void ResultSaver::AddResult(std::vector<std::shared_ptr<Frame>> frames, std::shared_ptr<GlobalEvent> ge)
{
	std::unique_lock<std::mutex> lock(mtx);
	queue.emplace(frames, ge);
	cv.notify_one();
}

void ResultSaver::Run(void)
{
	try {
		do {
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock);
			lock.unlock();

			while (!queue.empty()) {
				lock.lock();
				auto result = queue.back();
				queue.pop();
				lock.unlock();

			}
		} while (running);
	}
	catch (std::exception& e) {

	}
}
