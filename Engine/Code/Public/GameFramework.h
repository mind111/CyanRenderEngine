#pragma once

#ifdef GAMEFRAMEWORK_EXPORTS
    #define GAMEFRAMEWORK_API __declspec(dllexport)
#else
    #define GAMEFRAMEWORK_API __declspec(dllimport)
#endif
