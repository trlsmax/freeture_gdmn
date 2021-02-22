#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#include "Frame.h"
#include "GlobalEvent.h"

class ResultSaver
{
	ResultSaver(void);
	~ResultSaver(void);
	void Terminate(void);
	void AddResult(std::vector<std::shared_ptr<Frame>> frames, std::shared_ptr<GlobalEvent> ge);
	void Run(void);
private:
	std::thread* thd;
	std::mutex mtx;
	std::condition_variable cv;
	std::queue<std::pair<std::vector<std::shared_ptr<Frame>>, std::shared_ptr<GlobalEvent>>> queue;
	std::atomic_bool running;
};

