#pragma once

//
// helper.h
//
// Copyright (C) 2011 - 2018 jones@scss.tcd.ie
//

#ifdef WIN32
#include <tchar.h>          
#include <Windows.h>        
#include <intrin.h>         
#endif

#if __linux__
#include <math.h>           
#include <cpuid.h>          
#include <string.h>         
#include <pthread.h>        
#include <x86intrin.h>      
#include <limits.h>         
#endif

#include <iomanip>          
#include <locale>           

typedef long long INT64;
typedef unsigned long long UINT64;
#define INT64MIN LLONG_MIN
#define INT64MAX LLONG_MAX
#define UINT64MAX ULLONG_MAX

#define STRTOVINT(s, c, r) _strtoi64(s, c, r)
#define STRTOVUINT(s, c, r) _strtoui64(s, c, r)

#define AMALLOC(sz, align)  _aligned_malloc(((sz) + (align)-1) / (align) * (align), align)
#define AFREE(p)            _aligned_free(p)

#ifdef WIN32

#define CPUID(cd, v) __cpuid((int*) &cd, v);
#define CPUIDEX(cd, v0, v1) __cpuidex((int*) &cd, v0, v1)

#define THREADH HANDLE

#define WORKERF DWORD (WINAPI *worker) (void*)
#define WORKER DWORD WINAPI

#define ALIGN(n) __declspec(align(n))

#define TLSINDEX DWORD
#define TLSALLOC(key) key = TlsAlloc()
#define TLSSETVALUE(tlsIndex, v) TlsSetValue(tlsIndex, v)               
#define TLSGETVALUE(tlsIndex) TlsGetValue(tlsIndex)                     

#define thread_local __declspec(thread)

#elif __linux__

#define BYTE    unsigned char
#define UINT    unsigned int
#define INT64   long long
#define UINT64  unsigned long long
#define LONG64  signed long long
#define PVOID   void*
#define MAXINT  INT_MAX
#define MAXUINT UINT_MAX

#define MAXUINT64   ((UINT64)~((UINT64)0))
#define MAXINT64    ((INT64)(MAXUINT64 >> 1))
#define MININT64    ((INT64)~MAXINT64)

#define CPUID(cd, v) __cpuid(v, cd.eax, cd.ebx, cd.ecx, cd.edx);
#define CPUIDEX(cd, v0, v1) __cpuid_count(v0, v1, cd.eax, cd.ebx, cd.ecx, cd.edx)

#define THREADH pthread_t
#define GetCurrentProcessorNumber() sched_getcpu()

#define WORKER void*
#define WORKERF void* (*worker) (void*)

#define ALIGN(n) __attribute__ ((aligned (n)))
#define _aligned_malloc(sz, align)  aligned_alloc(align, ((sz)+(align)-1)/(align)*(align))
#define _aligned_free(p) free(p)
#define _alloca alloca

#define strcpy_s(dst, sz, src) strcpy(dst, src)
#define _strtoi64(str, end, base)  strtoll(str, end, base)
#define _strtoui64(str, end, base)  strtoull(str, end, base)

#define _InterlockedIncrement(addr)                                 __sync_fetch_and_add(addr, 1)
#define _InterlockedIncrement64(addr)                               __sync_fetch_and_add(addr, 1)
#define _InterlockedExchange(addr, v)                               __sync_lock_test_and_set(addr, v)
#define _InterlockedExchangePointer(addr, v)                        __sync_lock_test_and_set(addr, v)
#define _InterlockedExchangeAdd(addr, v)                            __sync_fetch_and_add(addr, v)
#define _InterlockedExchangeAdd64(addr, v)                          __sync_fetch_and_add(addr, v)
#define _InterlockedCompareExchange(addr, newv, oldv)               __sync_val_compare_and_swap(addr, oldv, newv)
#define _InterlockedCompareExchange64(addr, newv, oldv)             __sync_val_compare_and_swap(addr, oldv, newv)
#define _InterlockedCompareExchange64_HLERelease(addr, newv, oldv)  __sync_val_compare_and_swap(addr, oldv, newv, __ATOMIC_ACQUIRE | __ATOMIC_HLE_ACQUIRE)      
#define _InterlockedCompareExchangePointer(addr, newv, oldv)        __sync_val_compare_and_swap(addr, oldv, newv)
#define _InterlockedExchange_HLEAcquire(addr, val)                  __atomic_exchange_n(addr, val, __ATOMIC_ACQUIRE | __ATOMIC_HLE_ACQUIRE)
#define _InterlockedExchangeAdd64_HLEAcquire(addr, val)             __atomic_exchange_n(addr, val, __ATOMIC_ACQUIRE | __ATOMIC_HLE_ACQUIRE)
#define _Store_HLERelease(addr, v)                                  __atomic_store_n(addr, v, __ATOMIC_RELEASE | __ATOMIC_HLE_RELEASE)
#define _Store64_HLERelease(addr, v)                                __atomic_store_n(addr, v, __ATOMIC_RELEASE | __ATOMIC_HLE_RELEASE)

#define _mm_pause() __builtin_ia32_pause()
#define _mm_mfence() __builtin_ia32_mfence()

#define TLSINDEX pthread_key_t
#define TLSALLOC(key) pthread_key_create(&key, NULL)
#define TLSSETVALUE(key, v) pthread_setspecific(key, v)
#define TLSGETVALUE(key) (size_t) pthread_getspecific(key)

#define thread_local __thread                                       

#define Sleep(ms) usleep((ms)*1000)                                 

#endif

extern UINT ncpu;                                                   

extern void getDateAndTime(char*, int, time_t = 0);                 
extern char* getHostName();                                         
extern char* getOSName();                                           
extern int getNumberOfCPUs();                                       
extern UINT64 getPhysicalMemSz();                                   
extern int is64bitExe();                                            
extern size_t getMemUse();                                          
extern size_t getVMUse();                                           

extern UINT64 getWallClockMS();                                     
extern void createThread(THREADH*, WORKERF, void*);                 
extern void runThreadOnCPU(UINT);                                   
extern void waitForThreadsToFinish(UINT, THREADH*);                 
extern void closeThread(THREADH);                                   

extern UINT64 rand(UINT64&);                                        
#define RDRANDSTEP(r)   _rdrand64_step(r)                           

extern int cpu64bit();                                              
extern int cpuFamily();                                             
extern int cpuModel();                                              
extern int cpuStepping();                                           
extern char *cpuBrandString();                                      

extern int rtmSupported();                                          
extern int hleSupported();                                          

extern int getCacheInfo(int, int, int &, int &, int&);              
extern int getCacheLineSz();                                        
extern UINT getPageSz();                                            

extern void pauseIfKeyPressed();                                    
extern void pressKeyToContinue();                                   
extern void quit(int = 0);                                          

class CommaLocale : public std::numpunct<char> {
protected:
    virtual char do_thousands_sep() const { return ','; }
    virtual std::string do_grouping() const {return "\03"; }
};

extern void setCommaLocale();
extern void setLocale();
