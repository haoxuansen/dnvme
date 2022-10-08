/**
 * @file common_logs.h
 * @author meng_yu (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2020-06-06
 * 
 * @copyright imyumeng@qq.com Copyright (c) 2020
 * 
 */
#ifndef _COMMON_LOGS_H_
#define _COMMON_LOGS_H_

//--------------------------------------------------------------------------------
// #define DISP_LOG_DBUG
#define DISP_LOG_INFO
#define DISP_LOG_WARN
#define DISP_LOG_ERROR
#define DISP_LOG_COLOR

#define BLACK_LOG "30"  //黑色
#define RED_LOG "31"    //红色
#define GREEN_LOG "32"  //绿色
#define YELLOW_LOG "33" //黄色
#define BLUE_LOG "34"   //蓝色
#define PURPLE_LOG "35" //紫色
#define SKBLU_LOG "36"  //天蓝
#define WHITE_LOG "37"  //白色

//--------------------------------------------------------------------------------
#ifdef DISP_LOG_DBUG
#define LOG_DBUG(fmt, ...) PRINT_IF(fmt, ##__VA_ARGS__)
#else
#define LOG_DBUG(fmt, ...) do{} while (0)
#endif
#ifdef DISP_LOG_INFO
#define LOG_INFO(fmt, ...) PRINT_IF(fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) do{} while (0)
#endif
#ifdef DISP_LOG_WARN
#define LOG_WARN(fmt, ...) PRINT_IF("\033[33mWARNING! %s:%d %s()\033[0m: \r\n\t" fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...) do{} while (0)
#endif
#ifdef DISP_LOG_ERROR
#define LOG_ERROR(fmt, ...) PRINT_IF("\033[31mERROR!!! %s:%d %s()\033[0m: \r\n\t" fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) do{} while (0)
#endif
#ifdef DISP_LOG_COLOR
#define LOG_COLOR(color, fmt, ...) PRINT_IF("\033[" color "m" fmt "\033[0m", ##__VA_ARGS__)
#else
#define LOG_COLOR(color, fmt, ...) do{} while (0)
#endif
#define COLOR_STR(color, str) ("\033[" color "m" str "\033[0m")


//--------------------------------------------------------------------------------
// #define LOG_DBUG 0x10000000
// #define LOG_INFO 0x20000000
// #define LOG_WARN 0x40000000
// #define LOG_ERROR 0x80000000
// #define ALL_LOG 0xFFFFFFFF
// #define LOG_FILTER (LOG_INFO | LOG_WARN | LOG_ERROR) //|LOG_DBUG)
// // #define LOG_FILTER (ALL_LOG)
// #define LOG_PRINT_IF printf
// #define LOG(filter, fmt, ...)                                                                                              
//     do                                                                                                                     
//     {                                                                                                                      
//         if (filter & LOG_FILTER)                                                                                           
//         {                                                                                                                  
//             if ((filter & LOG_ERROR) == LOG_ERROR)                                                                         
//             {                                                                                                              
//                 LOG_PRINT_IF("\033[31mERROR!!! %s:%d %s()\033[0m: " fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); 
//             }                                                                                                              
//             else if ((filter & LOG_WARN) == LOG_WARN)                                                                      
//             {                                                                                                              
//                 LOG_PRINT_IF("\033[33mWARNING! %s:%d %s()\033[0m: " fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); 
//             }                                                                                                              
//             else                                                                                                           
//                 LOG_PRINT_IF(fmt, ##__VA_ARGS__);                                                                          
//         }                                                                                                                  
//     } while (0)

/* Assertions */
#define ASSERT(test)                                                             \
    do                                                                           \
    {                                                                            \
        if (!(test))                                                             \
        {                                                                        \
            PRINT_IF("[E]Assert:" #test " " __FILE__ ":" _STR(__LINE__) "\r\n"); \
            while (1);                                                           \
        }                                                                        \
    } while (0)

#endif
