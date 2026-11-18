#pragma once

#include <string>
#include <thread>

#ifdef _MSC_VER
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#elif defined(__linux__)
#include <pthread.h>
#endif

inline void set_thread_name(const std::string& name)
{
#ifdef _MSC_VER
    std::wstring wide_name(name.begin(), name.end());
    SetThreadDescription(
        GetCurrentThread(),
        wide_name.c_str()
    );

#elif defined(__APPLE__)
    pthread_setname_np(name.substr(0, 63).c_str());

#elif defined(__linux__)
    pthread_setname_np(
        pthread_self(),
        name.substr(0, 15).c_str()
    );
#endif
}