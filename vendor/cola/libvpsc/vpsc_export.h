#ifndef VPSC_EXPORT_H
#define VPSC_EXPORT_H

#if defined(_MSC_VER)
    #ifdef LIBVPSC_EXPORTS
        #define VPSC_EXPORT __declspec(dllexport)
    #else
        #define VPSC_EXPORT __declspec(dllimport)
    #endif
#else
    #define VPSC_EXPORT
#endif

#endif
