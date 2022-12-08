/**
 * @file common_define.h
 * @author meng_yu (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2020-06-06
 * 
 * @copyright imyumeng@qq.com Copyright (c) 2020
 * 
 */
#ifndef _COMMON_DEFINE_H_
#define _COMMON_DEFINE_H_

#define FAILED (-1)
#define SUCCEED (0)

#define DISABLE (0)
#define ENABLE (1)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(x,y) ((x)>(y)?(y):(x))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define MEM32_GET(n) (*(volatile uint32_t *)(n))
#define MEM16_GET(n) (*(volatile uint16_t *)(n))
#define MEM8_GET(n) (*(volatile uint8_t *)(n))

#define RAND_INIT() (srand(time(NULL)))
#define BYTE_RAND() ((uint8_t)rand())
#define WORD_RAND() ((uint16_t)rand())
#define DWORD_RAND() ((uint32_t)rand())
#define RAND_RANGE(s, e) (rand() % ((e) - (s) + 1) + (s))

#define BYTE_MASK (0xFF)
#define WORD_MASK (0xFFFF)
#define DWORD_MASK (0xFFFFFFFF)
#define QWORD_MASK (0xFFFFFFFFFFFFFFFF)

#define _VAL(x) #x
#define _STR(x) _VAL(x)

#endif
