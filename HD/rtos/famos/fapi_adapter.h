#ifndef FAPI_ADAPTER_H_
#define FAPI_ADAPTER_H_

struct fapi_driver;

struct Struct_21f19580
{
#if 0
   void (*lock)(void); //0
   void (*unlock)(void); //4
   void (*sleep)(void); //8
#endif
   int (*disableIrq)(void); //12
   void (*enableIrq)(int); //16
   void* (*createSemaphore)(unsigned); //20
   int (*deleteSemaphore)(void*); //24
   int (*requestSemaphore)(void*, int); //28
   int (*releaseSemaphore)(void*, int); //32
#if 0
   void* (*createMailqueue)(unsigned, int); //36
   void (*destroyMailqueue)(void); //40
   void (*getMailqueue)(void); //44
   void (*setMailqueue)(void); //48
   int (*getPhysAddr)(int); //52
   void* (*getVirtAddr)(void*); //56
#endif
   void* (*mallocFunc)(size_t size); //60??
#if 0
   void* (*mallocUncached)(int); //60
   void (*Data_64)(void); //64
   void* (*mallocCached)(int); //68
   void (*mallocSegment)(void); //72
#endif
   void (*freeFunc)(void* ptr); //76
   int (*printf)(const char*, ...); //80
   int (*initDrivers)(struct fapi_driver* a[]); //84
   //88
};

//extern struct Struct_21f19580 FAPI_SYS_Services; //21f19580


extern void func_21c0a434(int, int);
extern void func_21c0a480(void*, int);
//extern int FAPI_SYS_IsIrqEnabled(void);
//extern int FAPI_SYS_IsIsrActive(void);
extern int func_21c0a504(void);
extern void FAPI_SYS_SetMasterCpu(void);
//extern int FAPI_SYS_IsMasterCpu(void);
//extern void FAPI_SYS_SetServices(struct Struct_21f19580* a);



#define PRINTF(str) \
   if (FAPI_SYS_Services.printf != 0)\
   {\
      (FAPI_SYS_Services.printf)(str);\
   }

#define PRINTF1(str, p1) \
   if (FAPI_SYS_Services.printf != 0)\
   {\
      (FAPI_SYS_Services.printf)(str, p1);\
   }

#define PRINTF2(str, p1, p2) \
   if (FAPI_SYS_Services.printf != 0)\
   {\
      (FAPI_SYS_Services.printf)(str, p1, p2);\
   }

#define PRINTF3(str, p1, p2, p3) \
   if (FAPI_SYS_Services.printf != 0)\
   {\
      (FAPI_SYS_Services.printf)(str, p1, p2, p3);\
   }

#define PRINTF4(str, p1, p2, p3, p4) \
   if (FAPI_SYS_Services.printf != 0)\
   {\
      (FAPI_SYS_Services.printf)(str, p1, p2, p3, p4);\
   }

#define PRINTF6(str, p1, p2, p3, p4, p5, p6) \
   if (FAPI_SYS_Services.printf != 0)\
   {\
      (FAPI_SYS_Services.printf)(str, p1, p2, p3, p4, p5, p6);\
   }


#define LOCK(lock) \
   if (FAPI_SYS_Services.requestSemaphore != 0)\
   {\
      (FAPI_SYS_Services.requestSemaphore)(lock, -1);\
   }

#define UNLOCK(lock) \
   if (FAPI_SYS_Services.releaseSemaphore != 0)\
   {\
      (FAPI_SYS_Services.releaseSemaphore)(lock, 0);\
   }

#define DI(cpu_sr) \
   cpu_sr = (FAPI_SYS_Services.disableIrq != 0)?\
      (FAPI_SYS_Services.disableIrq)(): 0;\
      
#define EI(cpu_sr) \
   if (FAPI_SYS_Services.enableIrq != 0)\
   {\
      (FAPI_SYS_Services.enableIrq)(cpu_sr);\
   }

#define GET_VIRT_ADDR(v, p) \
   v = (FAPI_SYS_Services.getVirtAddr != 0)?\
      (FAPI_SYS_Services.getVirtAddr)(p): p;


#endif /*FAPI_ADAPTER_H_*/
