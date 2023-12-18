/**
 * @file check.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-12-18
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIB_BASE_CHECK_H_
#define _UAPI_LIB_BASE_CHECK_H_

#include "base/log.h"

#define CHK_MATCH_MAGIC			0x43484b6D ///< 'CHKM'

/**
 * @return If 'expr' retval and 'val' match the 'cond'(condition), then 
 *  return magic. Otherwise return 'expr' retval
 */
#define CHK_EXPR_VAL(expr, val, cond, ret) \
	({							\
		ret = expr;					\
		BUG_ON(ret == (typeof(ret))CHK_MATCH_MAGIC);	\
		if (ret cond val) {				\
			pr_err("check '%s' => '%d %s %d' err!\n", \
				#expr, val, #cond, ret);	\
			dump_stack(0);				\
			ret = (typeof(ret))CHK_MATCH_MAGIC;	\
		}						\
	})

/**
 * @brief Execute 'expr' and check its retval
 * 
 * @return If 'expr' retval and 'val' match the 'cond'(condition), then 
 *  return 'errno' and back to the previous function level. Otherwise 
 *  return 'expr' retval.
 */
#define CHK_EXPR_NUM_RTN(expr, val, errno, cond) \
	({							\
		int _ret;					\
		CHK_EXPR_VAL(expr, val, cond, _ret);		\
		if (_ret == CHK_MATCH_MAGIC)			\
			return errno;				\
		_ret;						\
	})

/**
 * @brief Execute 'expr' and check its retval
 * 
 * @return If 'expr' retval and 'val' match the 'cond'(condition), then 
 *  set 'errno' and jump to 'label'. Otherwise save 'expr' retval to 
 *  'ret'.
 */
#define CHK_EXPR_NUM_GOTO(expr, val, ret, errno, label, cond) \
	({							\
		int _ret;					\
		CHK_EXPR_VAL(expr, val, cond, _ret);		\
		if (_ret == CHK_MATCH_MAGIC) {			\
			ret = errno;				\
			goto label;				\
		} else {					\
			ret = _ret;				\
		}						\
	})

#define CHK_EXPR_PTR_RTN(expr, val, errno, cond) \
	({							\
		void *_ret;					\
		CHK_EXPR_VAL(expr, val, cond, _ret);		\
		if (_ret == (void *)CHK_MATCH_MAGIC)		\
			return errno;				\
		_ret;						\
	})

#define CHK_EXPR_PTR_GOTO(expr, val, ret, errno, label, cond) \
	({							\
		void *_ret;					\
		CHK_EXPR_VAL(expr, val, cond, _ret);		\
		if (_ret == (void *)CHK_MATCH_MAGIC) {		\
			ret = errno;				\
			goto label;				\
		} else {					\
			ret = _ret;				\
		}						\
	})

#define CHK_EXPR_NUM_LT_RTN(expr, val, errno) \
	CHK_EXPR_NUM_RTN(expr, val, errno, <)
#define CHK_EXPR_NUM_LE_RTN(expr, val, errno) \
	CHK_EXPR_NUM_RTN(expr, val, errno, <=)
#define CHK_EXPR_NUM_EQ_RTN(expr, val, errno) \
	CHK_EXPR_NUM_RTN(expr, val, errno, ==)
#define CHK_EXPR_NUM_GE_RTN(expr, val, errno) \
	CHK_EXPR_NUM_RTN(expr, val, errno, >=)
#define CHK_EXPR_NUM_GT_RTN(expr, val, errno) \
	CHK_EXPR_NUM_RTN(expr, val, errno, >)

#define CHK_EXPR_NUM_LT_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_NUM_GOTO(expr, val, ret, errno, label, <)
#define CHK_EXPR_NUM_LE_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_NUM_GOTO(expr, val, ret, errno, label, <=)
#define CHK_EXPR_NUM_EQ_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_NUM_GOTO(expr, val, ret, errno, label, ==)
#define CHK_EXPR_NUM_GE_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_NUM_GOTO(expr, val, ret, errno, label, >=)
#define CHK_EXPR_NUM_GT_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_NUM_GOTO(expr, val, ret, errno, label, >)

#define CHK_EXPR_PTR_LT_RTN(expr, val, errno) \
	CHK_EXPR_PTR_RTN(expr, val, errno, <)
#define CHK_EXPR_PTR_LE_RTN(expr, val, errno) \
	CHK_EXPR_PTR_RTN(expr, val, errno, <=)
#define CHK_EXPR_PTR_EQ_RTN(expr, val, errno) \
	CHK_EXPR_PTR_RTN(expr, val, errno, ==)
#define CHK_EXPR_PTR_GE_RTN(expr, val, errno) \
	CHK_EXPR_PTR_RTN(expr, val, errno, >=)
#define CHK_EXPR_PTR_GT_RTN(expr, val, errno) \
	CHK_EXPR_PTR_RTN(expr, val, errno, >)

#define CHK_EXPR_PTR_LT_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_PTR_GOTO(expr, val, ret, errno, label, <)
#define CHK_EXPR_PTR_LE_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_PTR_GOTO(expr, val, ret, errno, label, <=)
#define CHK_EXPR_PTR_EQ_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_PTR_GOTO(expr, val, ret, errno, label, ==)
#define CHK_EXPR_PTR_GE_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_PTR_GOTO(expr, val, ret, errno, label, >=)
#define CHK_EXPR_PTR_GT_GOTO(expr, val, ret, errno, label) \
	CHK_EXPR_PTR_GOTO(expr, val, ret, errno, label, >)

#define CHK_EXPR_NUM_LT0_RTN(expr, errno) \
	CHK_EXPR_NUM_LT_RTN(expr, 0, errno)
#define CHK_EXPR_NUM_LT0_GOTO(expr, ret, errno, label) \
	CHK_EXPR_NUM_LT_GOTO(expr, 0, ret, errno, label)

#define CHK_EXPR_PTR_EQ0_RTN(expr, errno) \
	CHK_EXPR_PTR_EQ_RTN(expr, NULL, errno)
#define CHK_EXPR_PTR_EQ0_GOTO(expr, ret, errno, label) \
	CHK_EXPR_PTR_EQ_GOTO(expr, NULL, ret, errno, label)


#define CHK_M_EXPR_VAL(expr, val, cond, ret, msg, ...) \
	({							\
		ret = expr;					\
		BUG_ON(ret == (typeof(ret))CHK_MATCH_MAGIC);	\
		if (ret cond val) {				\
			pr_err(msg, ##__VA_ARGS__);		\
			dump_stack(0);				\
			ret = (typeof(ret))CHK_MATCH_MAGIC;	\
		}						\
	})

#define CHK_M_EXPR_NUM_RTN(expr, val, errno, cond, msg, ...) \
	({							\
		int _ret;					\
		CHK_M_EXPR_VAL(expr, val, cond, _ret, msg, 	\
			##__VA_ARGS__);				\
		if (_ret == CHK_MATCH_MAGIC)			\
			return errno;				\
		_ret;						\
	})

#define CHK_M_EXPR_NUM_GOTO(expr, val, ret, errno, label, cond, msg, ...) \
	({							\
		int _ret;					\
		CHK_M_EXPR_VAL(expr, val, cond, _ret, msg,	\
			##__VA_ARGS__);				\
		if (_ret == CHK_MATCH_MAGIC) {			\
			ret = errno;				\
			goto label;				\
		} else {					\
			ret = _ret;				\
		}						\
	})

#define CHK_M_EXPR_PTR_RTN(expr, val, errno, cond, msg, ...) \
	({							\
		void *_ret;					\
		CHK_M_EXPR_VAL(expr, val, cond, _ret, msg,	\
			##__VA_ARGS__);				\
		if (_ret == (void *)CHK_MATCH_MAGIC)		\
			return errno;				\
		_ret;						\
	})

#define CHK_M_EXPR_PTR_GOTO(expr, val, ret, errno, label, cond, msg, ...) \
	({							\
		void *_ret;					\
		CHK_M_EXPR_VAL(expr, val, cond, _ret, msg, 	\
			##__VA_ARGS__);				\
		if (_ret == (void *)CHK_MATCH_MAGIC) {		\
			ret = errno;				\
			goto label;				\
		} else {					\
			ret = _ret;				\
		}						\
	})

#define CHK_M_EXPR_NUM_LT_RTN(expr, val, errno, msg, ...) \
	CHK_M_EXPR_NUM_RTN(expr, val, errno, <, msg, ##__VA_ARGS__)

#define CHK_M_EXPR_NUM_LT_GOTO(expr, val, ret, errno, label, msg, ...) \
	CHK_M_EXPR_NUM_GOTO(expr, val, ret, errno, label, <, msg, ##__VA_ARGS__)

#define CHK_M_EXPR_PTR_EQ_RTN(expr, val, errno, msg, ...) \
	CHK_M_EXPR_PTR_RTN(expr, val, errno, ==, msg, ##__VA_ARGS__)

#define CHK_M_EXPR_PTR_EQ_GOTO(expr, val, ret, errno, label, msg, ...) \
	CHK_M_EXPR_PTR_GOTO(expr, val, ret, errno, label, ==, msg, ##__VA_ARGS__)


#define CHK_M_EXPR_NUM_LT0_RTN(expr, errno, msg, ...) \
	CHK_M_EXPR_NUM_LT_RTN(expr, 0, errno, msg, ##__VA_ARGS__)
#define CHK_M_EXPR_NUM_LT0_GOTO(expr, ret, errno, label, msg, ...) \
	CHK_M_EXPR_NUM_LT_GOTO(expr, 0, ret, errno, label, msg, ##__VA_ARGS__)

#define CHK_M_EXPR_PTR_EQ0_RTN(expr, errno, msg, ...) \
	CHK_M_EXPR_PTR_EQ_RTN(expr, NULL, errno, msg, ##__VA_ARGS__)
#define CHK_M_EXPR_PTR_EQ0_GOTO(expr, ret, errno, label, msg, ...) \
	CHK_M_EXPR_PTR_EQ_GOTO(expr, NULL, ret, errno, label, msg, ##__VA_ARGS__)

#endif /* !_UAPI_LIB_BASE_CHECK_H_ */
