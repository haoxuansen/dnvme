#ifndef _LINUX_UUID_H_
#define _LINUX_UUID_H_

#include <linux/types.h>

#define UUID_SIZE			16

typedef struct {
	__u8	b[UUID_SIZE];
} uuid_t;

#endif /* !_LINUX_UUID_H_ */
