#pragma once
#ifndef LOG_H
#define LOG_H
#include <ntifs.h>
#include <cstdint>


namespace log
{
    template<class... Args>
    void dbg_print( const char* format, Args... args )
    {
        DbgPrint( format, args... );
    }

}
#endif LOG_H