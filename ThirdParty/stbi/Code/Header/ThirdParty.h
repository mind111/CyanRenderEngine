#pragma once

#ifdef THIRDPARTY_EXPORTS
    #define THIRDPARTY_API __declspec(dllexport)
#else
    #define THIRDPARTY_API __declspec(dllimport)
#endif
