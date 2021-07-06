#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#include "Frame.h"
#include "GlobalEvent.h"

class ResultSaver
{
public:
	ResultSaver(stationParam* stp, dataParam* mdp);
	~ResultSaver(void);
	void Terminate(void);
	void AddResult(std::shared_ptr<GlobalEvent> ge);
	void Run();

private:
    void saveDetectionInfos(GlobalEvent* ge, string path);
    bool saveEventData(GlobalEvent* ge, std::string EventPath);
    std::string buildEventDataDirectory(TimeDate::Date eventDate);

private:
    std::thread* thd;
	std::mutex mtx;
	std::condition_variable cv;
	std::queue<std::shared_ptr<GlobalEvent>> queue;
	std::atomic_bool running;
    stationParam* mstp;
    dataParam* mdp;
};

