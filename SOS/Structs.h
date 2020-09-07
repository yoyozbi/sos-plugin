#pragma once


struct LastTouchInfo
{
    std::string playerID;
    float speed = 0;
};

struct DummyStatEvent {
    char pad[144];
    wchar_t* Label;
};

struct DummyStatEventContainer
{
    uintptr_t Receiver;
    uintptr_t Victim;
    DummyStatEvent* StatEvent;
};
