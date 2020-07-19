#pragma once

#include <vector>
#include <string>
#include <random>

class Message
{
public:
    Message(int time);
    int getTime();
    int getLastTime();
    void setLastTime(int lastTime);
    void addMessage(std::string message);
    std::string getMessage();
private:
    int time;
    int lastTime;
    std::mt19937 mt;
    std::vector<std::string> messages;
};
