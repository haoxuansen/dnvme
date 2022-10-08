
#ifndef _UNITTEST_H_
#define _UNITTEST_H_

/* Test case definition macro */
// #define FAILED (0)
#define PASSED (1)
// #define SUCCEED (1)
#define WARNING (0x1<<1)
#define SKIPED (0x1<<2)

// TestCaseDefine
#define TCD(tf) {tf, #tf, FAILED}

typedef int (*CaseFunc_t)(void);
typedef void (*VoidFunc_t)(void);


/* Test case ctrl structure */
typedef struct _case_ctrl_t
{
    byte_t enable;
    const char *case_detail;
} case_ctrl_t;


#define SUB_CASE(cf, cn) {cf, #cf, cn, FAILED}
typedef dword_t (*SubCaseFunc_t)(void);
typedef struct _SubCase_t
{
    SubCaseFunc_t CaseFunc; 
    const char *FuncName;
    const char *FuncDesc;
    dword_t Result;
} SubCase_t;

typedef struct _SubCaseHeader_t
{
    const char *CaseHeaderName;    
    const char *CaseHeaderDesc;    
    SubCaseFunc_t PreFunc;
    SubCaseFunc_t EndFunc;
}SubCaseHeader_t;

/* Test case description structure */
typedef struct _TestCase_t
{
    CaseFunc_t CaseFunc;  /* Case function                        */
    const char *FuncName; /* Case function name string            */
    // dword_t En;           /* Case function enabled                */
    // VoidFunc_t PreFunc;
    // VoidFunc_t EndFunc;
    dword_t Result;
} TestCase_t;

/* Test suite description structure */
typedef struct _TestSuite_t
{
    const char *FileName;       /* Test module file name                */
    const char *Date;           /* Compilation date                     */
    const char *Time;           /* Compilation time                     */
    const char *TestSuiteTitle; /* Title or name of module under test   */
    TestCase_t *TestCase;       /* Array of test cases                  */
    dword_t NumOfTestCase;      /* Number of test cases (sz of TC array)*/
} TestSuite_t;

/* Assertion statistics */
typedef struct _AssertStat_t
{
    dword_t passed;   /* Total assertions passed              */
    dword_t failed;   /* Total assertions failed              */
    dword_t warnings; /* Total assertions warnings            */
} AssertStat_t;

/* Test global statistics */
typedef struct _TestReport_t
{
    dword_t tests;           /* Total test cases count               */
    dword_t executed;        /* Total test cases executed            */
    dword_t passed;          /* Total test cases passed              */
    dword_t failed;          /* Total test cases failed              */
    dword_t skiped;          /* Total test cases skiped              */
    dword_t warnings;        /* Total test cases warnings            */
    AssertStat_t assertions; /* Total assertions statistics          */
} TestReport_t;

void random_list(TestCase_t * arr, dword_t cnt);
dword_t test_list_exe(TestCase_t *TestList, dword_t NumOfCase);
dword_t sub_case_list_exe(SubCaseHeader_t *SubCaseHeader, SubCase_t *SubCaseList, dword_t SubCaseNum);

void mem_disp(void *mem_addr, dword_t data_size);
int_t mem_set(dword_t *mem_addr1, dword_t pattern, dword_t data_size);
int_t mem_cmp(dword_t *mem_addr1, dword_t *mem_addr2, dword_t data_size);
int_t dw_cmp(uint32_t *addr_buf1, uint32_t *addr_buf2, uint32_t buf_size);
int_t pat_cmp(dword_t *mem_addr1, dword_t pattern, dword_t data_size);


#define STR_PASS "\n"                                 \
                  "PPPPPP   AAAA   SSSSSS  SSSSSS \n" \
                  "PP  PP  AA  AA  SS      SS     \n" \
                  "PPPPPP  AAAAAA  SSSSSS  SSSSSS \n" \
                  "PP      AA  AA      SS      SS \n" \
                  "PP      AA  AA  SSSSSS  SSSSSS \n\n"
#define STR_FAIL "\n"                                 \
                  "FFFFFF   AAAA   IIIIII  LL     \n" \
                  "FF      AA  AA    II    LL     \n" \
                  "FFFFFF  AAAAAA    II    LL     \n" \
                  "FF      AA  AA    II    LL     \n" \
                  "FF      AA  AA  IIIIII  LLLLLL \n\n"
#define STR_WARN "\n"                                 \
                  "W    W   AAAA   RRRRRR  NN  NN \n" \
                  "W WW W  AA  AA  RR  RR  NNN NN \n" \
                  " W  W   AAAAAA  RRRRRR  NN NNN \n" \
                  " W  W   AA  AA  RR RR   NN  NN \n" \
                  " W  W   AA  AA  RR  RR  NN  NN \n\n"
#define STR_SKIP "\n"                                 \
                  "SSSSSS  KK  KK  IIIIII  PPPPPP \n" \
                  "SS      KK KK     II    PP  PP \n" \
                  "SSSSSS  KKK       II    PPPPPP \n" \
                  "    SS  KK KK     II    PP     \n" \
                  "SSSSSS  KK  KK  IIIIII  PP     \n\n"


#endif
