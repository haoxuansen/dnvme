/**
 * @file test_metrics.c
 * @author yumeng (imyumeng@qq.com)
 * @brief  this is a sample c unittest framework.also realization some 
 *         sample function like memcmp/mem_disp for debug.
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#include "common.h"
#include "unittest.h"

/**
 * @brief sub_case_list_exe
 * 
 * @param SubCaseHeader 
 * @param SubCaseList 
 * @param SubCaseNum 
 * @return uint32_t means no failed sub case cnt
 */
uint32_t sub_case_list_exe(SubCaseHeader_t *SubCaseHeader, SubCase_t *SubCaseList, uint32_t SubCaseNum)
{
    uint32_t idx;
    TestReport_t test_report;

    memset((void *)&test_report, 0, sizeof(TestReport_t));
    test_report.tests = SubCaseNum;

    pr_info("\n********************\t Case:%s \t********************\n", SubCaseHeader->CaseHeaderName);
    pr_info("%s\r\n", SubCaseHeader->CaseHeaderDesc);
    if (SubCaseHeader->PreFunc != NULL)
    {
        SubCaseHeader->PreFunc();
    }
    for (idx = 0; idx < SubCaseNum; idx++)
    {
        pr_color(LOG_N_GREEN, "Case:%d,%s():%s\r\n", idx, SubCaseList[idx].FuncName, SubCaseList[idx].FuncDesc);
        test_report.executed++;
        SubCaseList[idx].Result = SubCaseList[idx].CaseFunc();
        if (SubCaseList[idx].Result == SUCCEED)
        {
            test_report.passed++;
        }
        else if(SubCaseList[idx].Result == SKIPED)
        {
            test_report.skiped++;
        }
        else
        {
            test_report.failed++;
        }
    }
    if (SubCaseHeader->EndFunc != NULL)
    {
        SubCaseHeader->EndFunc();
    }
    pr_info("------------------------------------------------------------------\n");
    pr_info("SubCaseNum|Result|SubCaseName\r\n");
    for (idx = 0; idx < SubCaseNum; idx++)
    {
        if (SubCaseList[idx].Result == SUCCEED)
        {
            pr_info("  %4d    | %s |%-26s\r\n", idx, "PASS", SubCaseList[idx].FuncDesc);
        }
        else if (SubCaseList[idx].Result == SKIPED)
        {
            pr_color(LOG_N_CYAN,"  %4d    | %s |%-26s\r\n", idx, "SKIP", SubCaseList[idx].FuncDesc);
        }
        else
        {
            pr_color(LOG_N_RED, "  %4d    | %s |%-26s\r\n", idx, "FAIL", SubCaseList[idx].FuncDesc);
        }
    }
    pr_info("SubCase Summary [%s]: %d Tests, %d Executed, %d Passed, %d Failed, %d Skiped, %d Warnings.\r\n",
            SubCaseHeader->CaseHeaderName,
            test_report.tests,
            test_report.executed,
            test_report.passed,
            test_report.failed,
            test_report.skiped,
            test_report.warnings);
    pr_info("------------------------------------------------------------------\n");
    return test_report.failed;
}

/**
 * @brief memery data display
 * 
 * @param mem_addr 
 * @param data_size display size
 */
void mem_disp(void *mem_addr, uint32_t data_size)
{
    uint32_t columns, rows;
    uint32_t column_cnt, row_cnt;
    uint32_t *ptr = NULL;
    column_cnt = 8;
    row_cnt = data_size / column_cnt / 4;
    if (column_cnt == 16)
        pr_info("<--addr---dword-->: 03----00 07----04 11----08 15----12 19----16 23----20 27----24 31----28 "
                 "35----32 39----36 43----40 47----44 51----48 55----52 59----56 63----60\n");
    else
        pr_info("<--addr---dword-->: 03----00 07----04 11----08 15----12 19----16 23----20 27----24 31----28\n");

    ptr = (uint32_t *)mem_addr;
    for (rows = 0; rows < row_cnt; rows++)
    {
        // pr_info("%p: ", mem_addr + rows * 4 * column_cnt);
        pr_info("0x%016lx: ", (uint64_t)(mem_addr + rows * 4 * column_cnt));
        // pr_info("0x%-16lx: ", (uint64_t)(+ rows * 4 * column_cnt));
        for (columns = 0; columns < column_cnt; columns++)
        {
            pr_info("%08x ", ptr[columns]);
        }
        pr_info("\n");
        ptr += column_cnt;
    }
}
/**
 * @brief memery data compare
 * 
 * @param mem_addr1 
 * @param mem_addr2 
 * @param data_size 
 * @return int 
 */
int mem_cmp(uint32_t *mem_addr1, uint32_t *mem_addr2, uint32_t data_size)
{
    int idx = 0;
    idx = memcmp(mem_addr1, mem_addr2, data_size);
    if (idx)
        pr_err("Compare ERROR!!! idx:%d\n", idx);
    else
        pr_div("Compare OK!!! \n");
    return idx;
}
/**
 * @brief set memery data as pattern data
 * 
 * @param mem_addr1 
 * @param pattern 
 * @param data_size 
 * @return int 
 */
int mem_set(uint32_t *mem_addr1, uint32_t pattern, uint32_t data_size)
{
    uint32_t idx = 0;
    if (mem_addr1 == NULL)
        return idx;
    for (idx = 0; idx < (data_size / 4); idx++)
    {
        mem_addr1[idx] = pattern;
    }
    return idx;
}
/**
 * @brief get dw memery data, compare with pattern data 
 * 
 * @param mem_addr1 
 * @param pattern 
 * @param data_size 
 * @return int 
 */
int pat_cmp(uint32_t *mem_addr1, uint32_t pattern, uint32_t data_size)
{
    uint32_t idx = 0;
    uint8_t flg = 0;
    if (mem_addr1 == NULL)
        return idx;
    for (idx = 0; idx < (data_size / 4); idx++)
    {
        if (mem_addr1[idx] != pattern)
            flg = 1;
    }
    if (flg)
        pr_err("Compare ERROR!!! idx:%d\n", idx);
    else
        pr_div("Compare OK!!! \n");
    return idx;
}

/**
 * @brief get dw memery data, compare with pattern data 
 * 
 * @param mem_addr1 
 * @param pattern 
 * @param data_size 
 * @return int 
 */
int dw_cmp(uint32_t *addr_buf1, uint32_t *addr_buf2, uint32_t buf_size)
{
    uint32_t idx = 0;
    int flg = SUCCEED;
    for (idx = 0; idx < (buf_size / 4); idx++)
    {
        if (addr_buf1[idx] != addr_buf2[idx])
        {
            flg = FAILED;
            break;
        }
    }
    if (flg == SUCCEED)
    {
        pr_div("Compare OK!!! \n");
    }
    else
    {
        pr_err("Compare ERROR!!! idx:%d\n", idx);
    }
    return flg;
}
