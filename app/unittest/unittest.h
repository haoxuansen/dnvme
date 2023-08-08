
#ifndef _UNITTEST_H_
#define _UNITTEST_H_

#include "test.h"

/* Test case definition macro */
#define SKIPED (0x1<<2)

typedef int (*CaseFunc_t)(struct nvme_tool *tool);
typedef void (*VoidFunc_t)(void);

#define SUB_CASE(cf, cn) {cf, #cf, cn, FAILED}
typedef int (*SubCaseFunc_t)(void);
typedef struct _SubCase_t
{
    SubCaseFunc_t CaseFunc; 
    const char *FuncName;
    const char *FuncDesc;
    uint32_t Result;
} SubCase_t;

typedef struct _SubCaseHeader_t
{
    const char *CaseHeaderName;    
    const char *CaseHeaderDesc;    
    SubCaseFunc_t PreFunc;
    SubCaseFunc_t EndFunc;
}SubCaseHeader_t;

/* Test global statistics */
typedef struct _TestReport_t
{
    uint32_t tests;           /* Total test cases count               */
    uint32_t executed;        /* Total test cases executed            */
    uint32_t passed;          /* Total test cases passed              */
    uint32_t failed;          /* Total test cases failed              */
    uint32_t skiped;          /* Total test cases skiped              */
    uint32_t warnings;        /* Total test cases warnings            */
} TestReport_t;

uint32_t sub_case_list_exe(SubCaseHeader_t *SubCaseHeader, SubCase_t *SubCaseList, uint32_t SubCaseNum);

void mem_disp(void *mem_addr, uint32_t data_size);
int mem_set(uint32_t *mem_addr1, uint32_t pattern, uint32_t data_size);
int dw_cmp(uint32_t *addr_buf1, uint32_t *addr_buf2, uint32_t buf_size);

#endif
