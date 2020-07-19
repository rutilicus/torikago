#include "../include/message.h"

Message::Message(int time) : time(time), lastTime(0) {
    std::random_device rnd;
    mt = std::mt19937(rnd());
}

int Message::getTime() { return time; }

int Message::getLastTime() { return lastTime; }

void Message::setLastTime(int lastTime) { this->lastTime = lastTime; }

void Message::addMessage(std::string message) { messages.push_back(message); }

std::string Message::getMessage()
{
    return messages.at(mt() % messages.size());
}
