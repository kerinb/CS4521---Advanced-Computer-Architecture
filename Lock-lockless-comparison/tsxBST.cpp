//
// tsxBST.cpp
//
// Copyright (C) 2013 - 2018 jones@scss.tcd.ie
//

#include "helper.h"                            

#include <iostream>                            
#include <sstream>                             
#include <iomanip>                             

using namespace std;                            

#define K                   1024LL              
#define M                   (K*K)               
define G                   (K*K*K)             

//
// METHOD 0: no lock single thread
// METHOD 1: testAndTestAndSet lock
//
// TODO TODO TODO TODO TODO TODO TODO TODO TODO
//
// METHOD 2: HLE testAndTestAndSet lock
// METHOD 3: RTM
//

#define METHOD              1		
#define PREFILL             0                   // pre-fill with odd integers 0 .. maxKey-1 => 0: perfect 1: right list 2: left list

#define MINKEY              (16)               
#define MAXKEY              (1*M)              
#define SCALEKEY            (4)                

#define MINNT               1                   

#define NSECONDS            2                  

#define STATS               0x17                 MASK 1:commit 2:abort 4:depth 8:tsxStatus 16:times

#define ALIGNED                              
#define RECYCLENODES                         


#define NOP                 1000                

#if CONTAINS < 0 || CONTAINS > 100
#error CONTAINS sould be in the range 0 to 100
#endif

#if STATS & 1                                   
#define STAT1(x)    x                           
#else
#define STAT1(x)                                
#endif

#if STATS & 2                                   
#define STAT2(x)    x                           
#else
#define STAT2(x)                                
#endif


#if STATS & 4                                   
#define STAT4(x)    x                           
#else
#define STAT4(x)                                
#endif

#if STATS & 8                                   
#define STAT8(x)    x                           
#else
#define STAT8(x)                                
#endif

#if STATS & 16                                  
#define STAT16(x)   x                           
#else
#define STAT16(x)                               
#endif

#define DSUM    pt->avgD += d;      \
                if (d > pt->maxD)   \
                    pt->maxD = d                

#define PT(bst, thread)     ((PerThreadData*) ((BYTE*) ((bst)->perThreadData) + (thread)*ptDataSz))

UINT ntMin;                                     
UINT nt;                                        
UINT maxThread;                                 
UINT lineSz;                                    
UINT ptDataSz;                                  
UINT64 tStart;                                  
UINT64 t0;                                      
INT64 maxKey;                                   
UINT64 totalOps = 0;                            

THREADH *threadH;                               

TLSINDEX tlsPtIndx;                             
0
typedef struct {
    INT64 maxKey;                               
    UINT nt;                                    
    UINT64 pft;                                 
    UINT64 rt;                                  
    UINT64 nop;                                 
    UINT64 nmalloc;                             
    UINT64 nfree;                               
    UINT64 avgD;                                
    UINT64 maxD;                                
    size_t vmUse;                               
    size_t memUse;                              
    UINT64 ntree;                               
    UINT64 tt;                                  // total time (ms) [fill time] + test run time + free memory time
} Result;

Result *r, *ravg;                               
UINT rindx;                                     
UINT64 errs = 0;                                

class Node;                                     

typedef struct {
    UINT thread;                                
    UINT64 nop;                                 
    UINT64 nmalloc;                             
    UINT64 nfree;                               
    UINT64 avgD;                                
    UINT64 maxD;                                
    Node *free;                                 )
} PerThreadData;

class ALIGNEDMA {

public:

    void* operator new(size_t);                     
    void operator delete(void*);                    

};

void* ALIGNEDMA::operator new(size_t sz) {
    return AMALLOC(sz, lineSz);                     
}

void ALIGNEDMA::operator delete(void *p) {
    AFREE(p);                                       
}

#if defined(ALIGNED)
class Node : public ALIGNEDMA {
#else
class Node {
#endif

public:

    INT64 volatile key;                                     
    Node* volatile left;                                    
    Node* volatile right;                                   

    Node() {key = 0; right = left = NULL;}                  
    Node(INT64 k) {key = k; right = left = NULL;}           

};

class BST : public ALIGNEDMA {

public:

    PerThreadData *perThreadData;                           
    Node* volatile root;                                    

    BST(UINT);                                              
    ~BST();                                                 

    int contains(INT64);                                     
    int add(INT64);                                         
    int remove(INT64);                                      

    INT64 checkBST(Node*, UINT64& errBST);                  

#ifdef PREFILL
    void preFill();                                         
#endif

private:                                                    

#if METHOD == 1
    ALIGN(64) volatile long lock;                           
#endif

    int addTSX(Node*);                                      
    Node* removeTSX(INT64);                                 

#if defined(RECYCLENODES)
    Node* alloc(INT64, PerThreadData*);                     
    void dealloc(Node*, PerThreadData*);                    
#endif

    INT64 checkHelper(Node*, INT64, INT64, UINT64&);        

};

BST *bst;                                                   

BST::BST(UINT nt)  {                                                    
    perThreadData = (PerThreadData*) AMALLOC(nt*ptDataSz, lineSz);      
    memset(perThreadData, 0, nt*ptDataSz);                              
    for (UINT thread = 0; thread < nt; thread++)                        
        PT(this, thread)->thread = thread;                              
    root = NULL;                                                        

#if METHOD == 1
    lock = 0;
#endif

}

#if defined(RECYCLENODES)

inline Node* BST::alloc(INT64 key, PerThreadData *pt) {
    Node *n;
    if (pt->free) {
        n = pt->free;
        pt->free = n->right;
        n->key = key;
        n->left = n->right = NULL;
    } else {
        n = new Node(key);
        pt->nmalloc++;
    }
    return n;
}

inline void BST::dealloc(Node *n, PerThreadData *pt) {
    n->right = pt->free;
    pt->free = n;
}

#endif

INT64 BST:: checkHelper(Node *n, INT64 min, INT64 max, UINT64& errBST) {
    if (n == NULL)
        return 0;
    if (n->key <= min || n->key >= max)
        errBST += 1;
    return checkHelper(n->left, min, n->key, errBST) + checkHelper(n->right, n->key, max, errBST) + 1;
}

INT64 BST::checkBST(Node* n, UINT64& errBST) {
    errBST = 0;
    return checkHelper(n, INT64MIN, INT64MAX, errBST);
}

BST::~BST() {
    AFREE(perThreadData);
}

int BST::contains(INT64 key) {
    
    PerThreadData *pt = (PerThreadData*)TLSGETVALUE(tlsPtIndx);

#if METHOD == 1
    while (_InterlockedExchange(&lock, 1)) {
        do {
            _mm_pause();
        } while (lock);
    }
#endif

    Node *p = root;
    STAT4(UINT64 d = 0);
    while (p) {
        STAT4(d++);
        if (key < p->key) {
            p = p->left;
        } else if (key > p->key) {
            p = p->right;
        } else {
#if METHOD == 1
            lock = 0;
#endif
            STAT4(DSUM);
            return 1;
        }
    }

#if METHOD == 1
    lock = 0;
#endif
    STAT4(DSUM);
    return 0;
}

int BST::addTSX(Node *n) {

    PerThreadData *pt = (PerThreadData*)TLSGETVALUE(tlsPtIndx);

#if METHOD == 1
    while (_InterlockedExchange(&lock, 1)) {
        do {
            _mm_pause();
        } while (lock);
    }
#endif

    Node* volatile *pp = &root;
    Node *p = root;
    STAT4(UINT64 d = 0);
    while (p) {
        STAT4(d++);
        if (n->key < p->key) {
            pp = &p->left;
        } else if (n->key > p->key) {
            pp = &p->right;
        } else {
#if METHOD == 1
            lock = 0;
#endif
            STAT4(DSUM);
            return 0;
        }
        p = *pp;
    }

    *pp = n;
#if METHOD == 1
    lock = 0;
#endif
    STAT4(DSUM);
    return 1;
}

Node* BST::removeTSX(INT64 key) {

    PerThreadData *pt = (PerThreadData*)TLSGETVALUE(tlsPtIndx);

#if METHOD == 1
    while (_InterlockedExchange(&lock, 1)) {
        do {
            _mm_pause();
        } while (lock);
    }
#endif

    Node* volatile *pp = &root;
    Node *p = root;
    STAT4(UINT64 d = 0);
    while (p) {
        STAT4(d++);
        if (key < p->key) {
            pp = &p->left;
        } else if (key > p->key) {
            pp = &p->right;
        } else {
            break;
        }
        p = *pp;
    }

    if (p == NULL) {
#if METHOD == 1
        lock = 0;
#endif
        STAT4(DSUM);
        return NULL;
    }

    Node *left = p->left;
    Node *right = p->right;
    if (left == NULL && right == NULL) {
        *pp = NULL;
    } else if (left == NULL) {
        *pp = right;
    } else if (right == NULL) {
        *pp = left;
    } else {
        Node* volatile *ppr = &p->right;
        Node *r = right;
        while (r->left) {
            ppr = &r->left;
            r = r->left;
        }
#ifdef MOVENODE
        *ppr = r->right;
        r->left = p->left;
        r->right = p->right;
        *pp = r;
#else
        p->key = r->key;
        p = r;
        *ppr = r->right;
#endif
    }

#if METHOD == 1
    lock = 0;
#endif
    STAT4(DSUM);
    return  p;

}

int BST::add(INT64 key) {

#if defined(RECYCLENODES)
    PerThreadData *pt = (PerThreadData*)TLSGETVALUE(tlsPtIndx); 
    Node *n = alloc(key, pt);
#else
    Node *n = new Node(key);
    pt->nmalloc++;
#endif
    if (addTSX(n) == 0) {
#if defined(RECYCLENODES)
        dealloc(n, pt);
#else
        delete n;
        pt->nfree++;
#endif
        return 0;
    }
    return 1;
}

int BST::remove(INT64 key) {

    if (Node volatile *n = removeTSX(key)) {
#if defined(RECYCLENODES)
        PerThreadData *pt = (PerThreadData*)TLSGETVALUE(tlsPtIndx); 
        dealloc((Node *) n, pt);
#else
        delete n;
        pt->nfree++;
#endif
        return 1;
    }
    return 0;
}

#if defined(PREFILL) && PREFILL == 0

void preFillHelper(Node* volatile &root, INT64 minKey, INT64 maxKey, INT64 diff, PerThreadData *pt) {
    if (maxKey - minKey <= diff)
        return;
    INT64 key = minKey + (maxKey - minKey) / 2;
    root = new Node(key);
    pt->nmalloc++;
    preFillHelper(root->left, minKey, key, diff, pt);
    preFillHelper(root->right, key, maxKey, diff, pt);
}

WORKER preFillWorker(void* vthread) {

    UINT64 thread = (UINT64) vthread;

    INT64 minK = 0;
    INT64 maxK = maxKey - 1;
    INT64 key = 0;
    for (UINT64 n = ncpu / 2; n; n /= 2) {
        key = minK + (maxK - minK) / 2;
        if (thread & n) {
            minK = key;
        } else {
            maxK = key;
        }
    }
    Node *np = bst->root;
    while (np) {
        if (key < np->key) {
            np = np->left;
        } else if (key > np->key) {
            np = np->right;
        } else {
            break;
        }
    }

    PerThreadData *pt = PT(bst, thread);
    preFillHelper((key == maxK) ? np->left : np->right, minK, maxK, 2, pt);

    return 0;
}

void BST::preFill() {

    if (maxKey <= 1*M) {
        preFillHelper(bst->root, 0, maxKey - 1, 2, PT(this, 0));
        return;
    }

    preFillHelper(bst->root, 0, maxKey - 1, maxKey / ncpu, PT(this, 0));

    for (UINT thread = 0; thread < ncpu; thread++)
        createThread(&threadH[thread], preFillWorker, (void*) (UINT64) thread);
    waitForThreadsToFinish(ncpu, threadH);

    for (UINT thread = 0; thread < ncpu; thread++)
        closeThread(threadH[thread]);

    for (UINT cpu = 1; cpu < ncpu; cpu++) {
        if (cpu >= nt) {
            PT(this, 0)->nmalloc += PT(this, cpu)->nmalloc;
            PT(this, cpu)->nmalloc = 0;
        }
    }

}

#elif defined(PREFILL) && (PREFILL == 1 || PREFILL == 2)

struct PARAMS {
    PerThreadData *pt;
    INT64 minKey;
    INT64 maxKey;
    Node *first;
    Node *last;
};

WORKER preFillWorker(void *_params) {

    PARAMS *params = (PARAMS*) _params;     
    INT64 key = params->minKey + 1;         

    while (key < params->maxKey) {
        Node *nn = new Node(key);
        params->pt->nmalloc++;
#if PREFILL == 1
        if (params->first == NULL) {        
            params->first = nn;
        } else {
            params->last->right = nn;
        }
        params->last = nn;
#else
        if (params->first == NULL) {        
            params->last = nn;
        } else {
            nn->left = params->first;
        }
        params->first = nn;
#endif
        key += 2;
    }

    return 0;
}

void BST::preFill() {

    if (maxKey <= 1*M) {
        PARAMS params;
        params.pt = PT(this, 0);                
        params.minKey = 0;
        params.maxKey = maxKey;
        params.first = NULL;
        params.last = NULL;
        preFillWorker(&params);
        root = params.first;
        return;
    }

    PARAMS *params = new PARAMS[ncpu];
    for (UINT cpu = 0; cpu < ncpu; cpu++) {
        params[cpu].pt = PT(this, cpu);
        params[cpu].minKey = maxKey / ncpu * cpu;
        params[cpu].maxKey = maxKey / ncpu * (cpu + 1);
        params[cpu].first = NULL;
        params[cpu].last = NULL;
    }

    for (UINT cpu = 0; cpu < ncpu; cpu++)
        createThread(&threadH[cpu], preFillWorker, (void*) &params[cpu]);
    waitForThreadsToFinish(ncpu, threadH);


    for (UINT cpu = 0; cpu < ncpu; cpu++) {
#if PREFILL == 1
        if (cpu == 0) {
            root = params[0].first;
        } else {
            params[cpu-1].last->right = params[cpu].first;
        }
#else
        if (cpu == ncpu - 1) {
            root = params[cpu].first;
        } else {
            params[cpu+1].last->left = params[cpu].first;
        }
#endif
    }

    for (UINT cpu = 0; cpu < ncpu; cpu++)
        closeThread(threadH[cpu]);

    delete params;

    for (UINT cpu = 1; cpu < ncpu; cpu++) {
        if (cpu >= nt) {
            PT(this, 0)->nmalloc += PT(this, cpu)->nmalloc;
            PT(this, cpu)->nmalloc = 0;
        }
    }

}

#endif

WORKER worker(void* vthread) {

    PerThreadData *pt = PT(bst, (UINT64)vthread);       
    TLSSETVALUE(tlsPtIndx, pt);                         

    UINT64 r;                                           
    while (_rdrand64_step(&r) == 0);                    

    while (1) {

        for (int i = 0; i < NOP; i++) {

            UINT64 k = rand(r);

#if CONTAINS == 0
            INT64 key = k & (maxKey - 1);
            if (k >> 63) {
                bst->add(key);
            } else {
                bst->remove(key);
            }
#else
            UINT op = k % 100;
            INT64 key = (k / 100) & (maxKey - 1);
            if (op < CONTAINS) {
                bst->contains(key);
            } else if (op < CONTAINS + (100 - CONTAINS) / 2) {
                bst->add(key);
            } else {
                bst->remove(key);
            }
#endif
        }
        pt->nop += NOP;

        if (getWallClockMS() - t0 > NSECONDS*1000)
            break;

    }

    return 0;

}

void header() {

    char date[256];

    getDateAndTime(date, sizeof(date));

    cout << getHostName() << " " << getOSName() << (is64bitExe() ? " 64" : " 32") << " bit exe";

#ifdef _DEBUG
    cout << " DEBUG";
#else
    cout << " RELEASE";
#endif
#if METHOD == 0
    cout << " BST [NO lock single thread ONLY]";
#elif METHOD == 1
    cout << " BST [testAndTestAndSet lock]";
#endif

    cout << " NCPUS=" << ncpu << " RAM=" << (getPhysicalMemSz() + G - 1) / G << "GB ";

    cout << endl;

    cout << "METHOD=" << METHOD << " ";
#ifdef ALIGNED
    cout << "ALIGNED ";
#endif
#ifdef CONTAINS
    cout << "CONTAINS=" << CONTAINS << "% ADD=" << (100 - CONTAINS) / 2 << "% REMOVE=" << (100 - CONTAINS) / 2 << "% ";
#endif
#ifdef MOVENODE
    cout << "MOVENODE ";
#endif
    cout << "NOP=" << NOP << " ";
    cout << "NSECONDS=" << NSECONDS << " ";
#ifdef PREFILL
    cout << "PREFILL=" << PREFILL << " ";
#endif
#if defined(RECYCLENODES)
    cout << "RECYCLENODES ";
#endif
#ifdef STATS
    cout << "STATS=0x" << setfill('0') << hex << setw(2) << STATS << dec << setfill(' ') << " ";
#endif

    cout << "sizeof(Node)=" << sizeof(Node) << endl;;

    cout << "Intel" << (cpu64bit() ? "64" : "32") << " family " << cpuFamily() << " model " << cpuModel() << " stepping " << cpuStepping() << " " << cpuBrandString() << endl;

}

int main(int argc, char* argv[]) {

    ncpu = getNumberOfCPUs();           
    if (ncpu > 32)                      
        ncpu = 32;                      
    maxThread = 2 * ncpu;               

    TLSALLOC(tlsPtIndx);                

    header();                           

    tStart = time(NULL);                

    lineSz = getCacheLineSz();          
    ptDataSz = (UINT) (sizeof(PerThreadData) + lineSz - 1) / lineSz * lineSz;          

    cout << endl;

#if METHOD == 2
   if (!hleSupported()) {
        cout << "HLE (hardware lock elision) NOT supported by this CPU" << endl;
        quit();
        return 1;
    }
#endif

#if METHOD == 3
   if (!rtmSupported()) {
        cout << "RTM (restricted transactional memory) NOT supported by this CPU" << endl;
        quit();
        return 1;
    }
#endif

#ifdef WIN32
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif

    INT64 keyMin = MINKEY;                                                  
    INT64 keyMax = MAXKEY;                                                  
#if METHOD == 0                                 
    ntMin = 1;
    UINT ntMax = 1;
#else
    ntMin = MINNT;
    UINT ntMax = maxThread;                                                 
#endif

    int c0 = 1, c1 = 1;                                                     
    for (maxKey = keyMin; maxKey < keyMax; maxKey *= SCALEKEY)              
        c0++;                                                               
    for (nt = ntMin; nt < ntMax; nt *= 2)                                   
        c1++;                                                               

    threadH = (THREADH*) AMALLOC(maxThread*sizeof(THREADH), lineSz);        
    r = (Result*) AMALLOC(c0*c1*sizeof(Result), lineSz);                    
    ravg = (Result*) AMALLOC(c0*c1*sizeof(Result), lineSz);                 
    memset(ravg, 0, c0*c1*sizeof(Result));                                  
        
        setCommaLocale();
        int keyw = (int) log10((double) keyMax) + (int) (log10((double) keyMax) / log10(1000)) + 2;
        keyw = (keyw < 7) ? 7 : keyw;
        cout << setw(keyw - 1) << "maxKey" << setw(3) << "nt";
#ifdef PREFILL
        STAT16(cout << setw(7) << "pft");
#endif
        cout << setw(7) << "rt";
        cout << setw(16) << "ops" << setw(12) << "ops/s";
#if METHOD > 0
        if (ntMin == 1)
            cout << setw(8) << "rel";
#endif
        cout << setw(14) << "nMalloc" << setw(14) << "nFree";
        cout << setw(keyw) << "ntree";
        cout << setw(11) << "vmUse" << setw(11) << "memUse";
        STAT4(cout << setw(keyw) << "avgD" << setw(keyw) << "maxD");
#if METHOD > 1
        STAT1(cout << setw(8) << "commit");
#endif
        STAT16(cout << setw(7) << "tt");
        cout << endl;

        cout << setw(keyw - 1) << "------" << setw(3) << "--";          
#ifdef PREFILL
        STAT16(cout << setw(7) << "---");                               
#endif
        cout << setw(7) << "--";                                        
        cout << setw(16) << "---" << setw(12) << "-----";               
#if METHOD > 0
        if (ntMin == 1)
            cout << setw(8) << "---";                                   
#endif
        cout << setw(14) << "-------" << setw(14) << "-----";           
        cout << setw(keyw) << "-----";                                  
        cout << setw(11) << "-----" << setw(11) << "------";            
        STAT4(cout << setw(keyw) << "----" << setw(keyw) << "----");    

        STAT16(cout << setw(7) << "--");                                
        cout << endl;

        rindx = 0;                                                      

        for (maxKey = keyMin; maxKey <= keyMax; maxKey *= SCALEKEY) {

#if METHOD > 0
            double noppersec1 = 1;
#endif

            for (nt = ntMin; nt <= ntMax; nt *= 2) {

                bst = new BST(maxThread);                               

                t0 = getWallClockMS();                                  

#ifdef PREFILL
                bst->preFill();
                UINT64 pft = getWallClockMS() - t0;
                t0 = getWallClockMS();                                  
#else
                UINT64 pft = 0;                                         
#endif

                for (UINT thread = 0; thread < nt; thread++)
                    createThread(&threadH[thread], worker, (void*) (UINT64) thread);

                waitForThreadsToFinish(nt, threadH);
                UINT64 rt = getWallClockMS() - t0;

                UINT64 nop = 0, nmalloc = 0, nfree = 0, ntree;
                UINT64 avgD = 0, maxD = 0;
                size_t vmUse, memUse;

                for (UINT thread = 0; thread < nt; thread++) {
                    PerThreadData *pt = PT(bst, thread);
                    nop += pt->nop;
                    nmalloc += pt->nmalloc;

#if defined(RECYCLENODES)
                    while (pt->free) {
                        Node *tmp = pt->free->right;
                        delete pt->free;
                        pt->free = tmp;
                        pt->nfree++;
                    }
#endif
                    nfree += pt->nfree;
                    avgD += pt->avgD;
                    maxD = (pt->maxD > maxD) ? pt->maxD : maxD;
                }

#if METHOD > 0
                if (nt == 1)
                    noppersec1 = (double) nop / (double) rt / 1000;     
#endif

                totalOps += nop;

                r[rindx].maxKey = maxKey;
                r[rindx].nt = nt;
                r[rindx].pft = pft;
                r[rindx].rt = rt;
                r[rindx].nop = nop;
                r[rindx].nmalloc = nmalloc;
                r[rindx].nfree = nfree;
                r[rindx].avgD = avgD;
                r[rindx].maxD = maxD;

                vmUse = r[rindx].vmUse = getVMUse();
                memUse = r[rindx].memUse = getMemUse();

                cout << setw(keyw - 1) << maxKey << setw(3) << nt;
#ifdef PREFILL
                STAT16(cout << setw(7) << fixed << setprecision(pft < 100*1000 ? 2 : 0) << (double) pft / 1000);
#endif
                cout << setw(7) << fixed << setprecision(rt < 100*1000 ? 2 : 0) << (double) rt / 1000;
                cout << setw(16) << nop << setw(12) << nop * 1000 / rt;
#if METHOD > 0
                if (ntMin == 1)
                    cout << " [" << fixed << setprecision(2) << setw(5) << (double) nop / (double) rt / 1000 / noppersec1 << "]";   
#endif
                cout << setw(14) << nmalloc << setw(14) << nfree;

                cout << flush;  

                UINT64 errBST;
                ntree = bst->checkBST(bst->root, errBST);
                r[rindx].ntree = ntree;
                cout << setw(keyw) << ntree;
                delete bst;

                if (vmUse / G) {
                    cout << fixed << setprecision(2) << setw(9) << (double) vmUse / G << "GB";
                } else {
                    cout << fixed << setprecision(2) << setw(9) << (double) vmUse / M << "MB";
                }

                if (memUse / G) {
                    cout << fixed << setprecision(2) << setw(9) << (double) memUse / G << "GB";
                } else {
                    cout << fixed << setprecision(2) << setw(9) << (double) memUse / M << "MB";
                }

                STAT4(double davgD = (double) avgD / (double) nop); 
                STAT4(cout << fixed << setprecision(2) << setw(keyw) << setprecision(davgD < 1000 ? 2 : 0) << davgD << setw(keyw) << maxD);

                UINT64 tt = getWallClockMS() - t0;
#ifdef PREFILL
                tt += pft;
#endif
                STAT16(cout << setw(7) << fixed << setprecision(tt < 100*1000 ? 2 : 0) << (double) tt / 1000);

                for (UINT thread = 0; thread < nt; thread++)
                    closeThread(threadH[thread]);

                if (errBST || (nmalloc != ntree + nfree)) {

                    cout << " ERROR:";

                    if (errBST) {
                        errs++;
                        cout << " BST inccorrect";
                    }

                    if (nmalloc != ntree + nfree) {
                        errs++;
                        cout << " diff= " << nmalloc - ntree - nfree;
                    }

                    cout << " [errs=" << errs << "]";

                }

                ravg[rindx].pft += r[rindx].pft;
                ravg[rindx].rt += r[rindx].rt;
                ravg[rindx].nop += r[rindx].nop;
                ravg[rindx].nmalloc += r[rindx].nmalloc;
                ravg[rindx].nfree += r[rindx].nfree;
                ravg[rindx].avgD += r[rindx].avgD;
                ravg[rindx].maxD += r[rindx].maxD;
                ravg[rindx].vmUse += r[rindx].vmUse;
                ravg[rindx].memUse += r[rindx].memUse;
                ravg[rindx].ntree += r[rindx].ntree;
                ravg[rindx].tt += tt;

                rindx++;
                cout << endl;

        } 

    } 

    pressKeyToContinue();

    return 0;

}
