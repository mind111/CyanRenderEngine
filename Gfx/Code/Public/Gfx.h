#pragma once

#ifdef GFX_EXPORTS
    #define GFX_API __declspec(dllexport)
#else
    #define GFX_API __declspec(dllimport)
#endif
