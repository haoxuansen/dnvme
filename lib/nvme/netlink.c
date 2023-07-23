/**
 * @file netlink.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>

#include "compiler.h"
#include "dnvme.h"
#include "libbase.h"
#include "libnvme.h"

/**
 * @ingroup attr
 * Iterate over a stream of attributes
 * @arg pos	loop counter, set to current attribute
 * @arg head	head of attribute stream
 * @arg len	length of attribute stream
 * @arg rem	initialized to len, holds bytes currently remaining in stream
 */
#define nla_for_each_attr(pos, head, len, rem) \
	for (pos = head, rem = len; \
	     nla_ok(pos, rem); \
	     pos = nla_next(pos, &(rem)))

static size_t default_msg_size;

struct nl_data {
	size_t			d_size;
	void			*d_data;
};

struct nl_msg {
	struct sockaddr_nl	nm_src;
	struct sockaddr_nl	nm_dst;
	struct nlmsghdr		*nm_nlh;
	size_t			nm_size;
};

static void __init init_msg_size(void)
{
	default_msg_size = getpagesize();
}

static inline int nlmsg_msg_size(int payload)
{
	return NLMSG_HDRLEN + payload;
}

static int nlmsg_total_size(int payload)
{
	return NLMSG_ALIGN(nlmsg_msg_size(payload));
}

/**
 * @brief head of message payload
 *
 * @param nlh netlink message header
 */
static inline void *nlmsg_data(struct nlmsghdr *nlh)
{
	return (unsigned char *)nlh + NLMSG_HDRLEN;
}

static inline void *nlmsg_tail(struct nlmsghdr *nlh)
{
	return (unsigned char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len);
}

static int nlmsg_valid_hdr(struct nlmsghdr *nlh, int hdrlen)
{
	if (nlh->nlmsg_len < nlmsg_msg_size(hdrlen))
		return 0;

	return 1;
}

static struct nl_msg *__nlmsg_alloc(size_t len)
{
	struct nl_msg *nm;

	BUG_ON(len < nlmsg_total_size(0));

	nm = calloc(1, sizeof(*nm));
	if (!nm)
		return NULL;

	nm->nm_nlh = calloc(1, len);
	if (!nm->nm_nlh)
		goto out;

	nm->nm_size = len;
	nm->nm_nlh->nlmsg_len = nlmsg_total_size(0);

	return nm;
out:
	free(nm);
	return NULL;
}

static struct nl_msg *nlmsg_alloc(void)
{
	return __nlmsg_alloc(default_msg_size);
}

static void nlmsg_init(struct nl_msg *msg)
{
	msg->nm_src.nl_family = AF_NETLINK;
	msg->nm_src.nl_pid = getpid();
	msg->nm_src.nl_groups = 0;

	msg->nm_dst.nl_family = AF_NETLINK;
	msg->nm_dst.nl_pid = 0;
	msg->nm_dst.nl_groups = 0;
}

static void nlmsg_free(struct nl_msg *msg)
{
	if (!msg)
		return;

	free(msg->nm_nlh);
	free(msg);
}

/**
 * @brief Reserve room for additional data in a netlink message
 *
 * @param n		netlink message
 * @param len		length of additional data to reserve room for
 * @param pad		number of bytes to align data to
 *
 * Reserves room for additional data at the tail of the an
 * existing netlink message. Eventual padding required will
 * be zeroed out.
 *
 * @return Pointer to start of additional data tailroom or NULL.
 */
void *nlmsg_reserve(struct nl_msg *n, size_t len, int pad)
{
	void *buf = n->nm_nlh;
	size_t nlmsg_len = n->nm_nlh->nlmsg_len;
	size_t tlen;

	tlen = pad ? ((len + (pad - 1)) & ~(pad - 1)) : len;

	if ((tlen + nlmsg_len) > n->nm_size)
		return NULL;

	buf += nlmsg_len;
	n->nm_nlh->nlmsg_len += tlen;

	if (tlen > len)
		memset(buf + len, 0, tlen - len);

	return buf;
}

/**
 * @brief Append data to tail of a netlink message
 *
 * @param n	netlink message
 * @param data	data to add
 * @param len	length of data
 * @param pad	Number of bytes to align data to.
 *
 * Extends the netlink message as needed and appends the data of given
 * length to the message. 
 *
 * @return 0 on success or a negative error code
 */
int nlmsg_append(struct nl_msg *n, void *data, size_t len, int pad)
{
	void *tmp;

	tmp = nlmsg_reserve(n, len, pad);
	if (!tmp)
		return -ENOMEM;

	memcpy(tmp, data, len);
	return 0;
}

static void *nlmsg_put(struct nl_msg *n, uint32_t port, uint32_t seq, 
	uint16_t type, int payload, uint16_t flags)
{
	struct nlmsghdr *nlh;

	nlh = n->nm_nlh;
	nlh->nlmsg_type = type;
	nlh->nlmsg_flags = flags;
	nlh->nlmsg_seq = seq;
	nlh->nlmsg_pid = port;

	if (payload > 0 && 
		nlmsg_reserve(n, payload, NLMSG_ALIGNTO) == NULL)
		return NULL;

	return nlmsg_data(nlh);
}


/**
 * @brief Return size of attribute whithout padding.
 *
 * @param payload Payload length of attribute.
 *
 * @code
 *    <-------- nla_attr_size(payload) --------->
 *   +------------------+- - -+- - - - - - - - - +- - -+
 *   | Attribute Header | Pad |     Payload      | Pad |
 *   +------------------+- - -+- - - - - - - - - +- - -+
 * @endcode
 *
 * @return Size of attribute in bytes without padding.
 */
static int nla_attr_size(int payload)
{
	return NLA_HDRLEN + payload;
}

/**
 * @brief Return size of attribute including padding.
 *
 * @param payload Payload length of attribute.
 *
 * @code
 *    <----------- nla_total_size(payload) ----------->
 *   +------------------+- - -+- - - - - - - - - +- - -+
 *   | Attribute Header | Pad |     Payload      | Pad |
 *   +------------------+- - -+- - - - - - - - - +- - -+
 * @endcode
 *
 * @return Size of attribute in bytes.
 */
static int nla_total_size(int payload)
{
	return NLA_ALIGN(nla_attr_size(payload));
}

/**
 * @brief Return length of padding at the tail of the attribute.
 *
 * @param payload Payload length of attribute.
 *
 * @code
 *   +------------------+- - -+- - - - - - - - - +- - -+
 *   | Attribute Header | Pad |     Payload      | Pad |
 *   +------------------+- - -+- - - - - - - - - +- - -+
 *                                                <--->  
 * @endcode
 *
 * @return Length of padding in bytes.
 */
static int nla_padlen(int payload)
{
	return nla_total_size(payload) - nla_attr_size(payload);
}

/**
 * Return type of the attribute.
 * @arg nla		Attribute.
 *
 * @return Type of attribute.
 */
static int nla_type(const struct nlattr *nla)
{
	return nla->nla_type & NLA_TYPE_MASK;
}

/**
 * Return pointer to the payload section.
 * @arg nla		Attribute.
 *
 * @return Pointer to start of payload section.
 */
static void *nla_data(struct nlattr *nla)
{
	return (unsigned char *)nla + NLA_HDRLEN;
}

/**
 * @brief Return length of the payload .
 *
 * @param nla		Attribute
 *
 * @return Length of payload in bytes.
 */
static int nla_len(const struct nlattr *nla)
{
	return nla->nla_len - NLA_HDRLEN;
}

/**
 * Check if the attribute header and payload can be accessed safely.
 * @arg nla		Attribute of any kind.
 * @arg remaining	Number of bytes remaining in attribute stream.
 *
 * Verifies that the header and payload do not exceed the number of
 * bytes left in the attribute stream. This function must be called
 * before access the attribute header or payload when iterating over
 * the attribute stream using nla_next().
 *
 * @return True if the attribute can be accessed safely, false otherwise.
 */
static int nla_ok(const struct nlattr *nla, int remaining)
{
	return remaining >= sizeof(*nla) &&
	       nla->nla_len >= sizeof(*nla) &&
	       nla->nla_len <= remaining;
}

/**
 * Return next attribute in a stream of attributes.
 * @arg nla		Attribute of any kind.
 * @arg remaining	Variable to count remaining bytes in stream.
 *
 * Calculates the offset to the next attribute based on the attribute
 * given. The attribute provided is assumed to be accessible, the
 * caller is responsible to use nla_ok() beforehand. The offset (length
 * of specified attribute including padding) is then subtracted from
 * the remaining bytes variable and a pointer to the next attribute is
 * returned.
 *
 * nla_next() can be called as long as remainig is >0.
 *
 * @return Pointer to next attribute.
 */
struct nlattr *nla_next(const struct nlattr *nla, int *remaining)
{
	int totlen = NLA_ALIGN(nla->nla_len);

	*remaining -= totlen;
	return (struct nlattr *) ((char *) nla + totlen);
}

/**
 * @brief Reserve space for a attribute.
 *
 * @param msg		Netlink Message.
 * @param attrtype	Attribute Type.
 * @param attrlen	Length of payload.
 *
 * Reserves room for a attribute in the specified netlink message and
 * fills in the attribute header (type, length). Returns NULL if there
 * is unsuficient space for the attribute.
 *
 * Any padding between payload and the start of the next attribute is
 * zeroed out.
 *
 * @return Pointer to start of attribute or NULL on failure.
 */
static struct nlattr *nla_reserve(struct nl_msg *msg, int attrtype, int attrlen)
{
	struct nlattr *nla;
	int total;

	total = NLMSG_ALIGN(msg->nm_nlh->nlmsg_len) + nla_total_size(attrlen);
	if (total > msg->nm_size)
		return NULL;

	nla = (struct nlattr *)nlmsg_tail(msg->nm_nlh);
	nla->nla_type = attrtype;
	nla->nla_len = nla_attr_size(attrlen);

	if (attrlen && nla_padlen(attrlen))
		memset((unsigned char *) nla + nla->nla_len, 0, nla_padlen(attrlen));
	msg->nm_nlh->nlmsg_len = total;

	return nla;
}

static int nla_put(struct nl_msg *msg, int attrtype, int datalen, const void *data)
{
	struct nlattr *nla;

	nla = nla_reserve(msg, attrtype, datalen);
	if (!nla)
		return -ENOMEM;

	if (datalen > 0) {
		memcpy(nla_data(nla), data, datalen);
	}

	return 0;
}

int nla_put_data(struct nl_msg *msg, int attrtype, struct nl_data *data)
{
	return nla_put(msg, attrtype, data->d_size, data->d_data);
}

/**
 * @brief Add 8 bit integer attribute to netlink message.
 *
 * @param msg		Netlink message.
 * @param attrtype	Attribute type.
 * @param value		Numeric value to store as payload.
 *
 * @see nla_put
 * @return 0 on success or a negative error code.
 */
int nla_put_u8(struct nl_msg *msg, int attrtype, uint8_t value)
{
	return nla_put(msg, attrtype, sizeof(uint8_t), &value);
}

/**
 * @brief Return value of 8 bit integer attribute.
 *
 * @param nla		8 bit integer attribute
 *
 * @return Payload as 8 bit integer.
 */
uint8_t nla_get_u8(struct nlattr *nla)
{
	return *(uint8_t *)nla_data(nla);
}

/**
 * @brief Add 16 bit integer attribute to netlink message.
 *
 * @param msg		Netlink message.
 * @param attrtype	Attribute type.
 * @param value		Numeric value to store as payload.
 *
 * @see nla_put
 * @return 0 on success or a negative error code.
 */
int nla_put_u16(struct nl_msg *msg, int attrtype, uint16_t value)
{
	return nla_put(msg, attrtype, sizeof(uint16_t), &value);
}

/**
 * @brief Return payload of 16 bit integer attribute.
 *
 * @param nla		16 bit integer attribute
 *
 * @return Payload as 16 bit integer.
 */
uint16_t nla_get_u16(struct nlattr *nla)
{
	return *(uint16_t *) nla_data(nla);
}

/**
 * @brief Add 32 bit integer attribute to netlink message.
 *
 * @param msg		Netlink message.
 * @param attrtype	Attribute type.
 * @param value		Numeric value to store as payload.
 *
 * @see nla_put
 * @return 0 on success or a negative error code.
 */
int nla_put_u32(struct nl_msg *msg, int attrtype, uint32_t value)
{
	return nla_put(msg, attrtype, sizeof(uint32_t), &value);
}

/**
 * @brief Return payload of 32 bit integer attribute.
 *
 * @param nla		32 bit integer attribute.
 *
 * @return Payload as 32 bit integer.
 */
uint32_t nla_get_u32(struct nlattr *nla)
{
	return *(uint32_t *) nla_data(nla);
}

/**
 * @brief Add 64 bit integer attribute to netlink message.
 *
 * @param msg		Netlink message.
 * @param attrtype	Attribute type.
 * @param value		Numeric value to store as payload.
 *
 * @see nla_put
 * @return 0 on success or a negative error code.
 */
int nla_put_u64(struct nl_msg *msg, int attrtype, uint64_t value)
{
	return nla_put(msg, attrtype, sizeof(uint64_t), &value);
}

/**
 * @brief Return payload of u64 attribute
 *
 * @param nla		u64 netlink attribute
 *
 * @return Payload as 64 bit integer.
 */
uint64_t nla_get_u64(struct nlattr *nla)
{
	uint64_t tmp = 0;

	if (nla && nla_len(nla) >= sizeof(tmp))
		memcpy(&tmp, nla_data(nla), sizeof(tmp));

	return tmp;
}

int nla_put_s32(struct nl_msg *msg, int attrtype, int value)
{
	return nla_put(msg, attrtype, sizeof(int), &value);
}

int32_t nla_get_s32(struct nlattr *nla)
{
	return *(int32_t *) nla_data(nla);
}

int nla_put_string(struct nl_msg *msg, int attrtype, const char *str)
{
	return nla_put(msg, attrtype, strlen(str) + 1, str);
}

int nla_parse(struct nlattr **tb, int maxtype, struct nlattr *head, int len)
{
	struct nlattr *nla;
	int rem;

	memset(tb, 0, sizeof(struct nlattr *) * (maxtype + 1));

	nla_for_each_attr(nla, head, len, rem) {
		int type = nla_type(nla);

		if (type > maxtype)
			continue;

		if (tb[type])
			pr_warn("Attribute of type %#x found multiple times in message,"
				"previous attribute is being ignored.\n", type);

		tb[type] = nla;
	}

	if (rem > 0)
		pr_warn("%d bytes leftover after parsing attributes.\n", rem);

	return 0;
}

static inline void *genlmsg_data(struct genlmsghdr *hdr)
{
	return (unsigned char *)hdr + GENL_HDRLEN;
}

static int genlmsg_len(struct genlmsghdr *gnlh)
{
	struct nlmsghdr *nlh;

	nlh = (struct nlmsghdr *)((unsigned char *) gnlh - NLMSG_HDRLEN);
	return (nlh->nlmsg_len - GENL_HDRLEN - NLMSG_HDRLEN);
}

/**
 * Return pointer to user header
 * @arg gnlh		Generic Netlink message header
 *
 * Calculates the pointer to the user header based on the pointer to
 * the Generic Netlink message header.
 *
 * @return Pointer to the user header
 */
static void *genlmsg_user_hdr(struct genlmsghdr *gnlh)
{
	return genlmsg_data(gnlh);
}

/**
 * Return pointer to user data
 * @arg gnlh		Generic netlink message header
 * @arg hdrlen		Length of user header
 *
 * Calculates the pointer to the user data based on the pointer to
 * the Generic Netlink message header.
 *
 * @see genlmsg_user_datalen()
 *
 * @return Pointer to the user data
 */
static void *genlmsg_user_data(struct genlmsghdr *gnlh, int hdrlen)
{
	return genlmsg_user_hdr(gnlh) + NLMSG_ALIGN(hdrlen);
}

/**
 * Return length of user data
 * @arg gnlh		Generic Netlink message header
 * @arg hdrlen		Length of user header
 *
 * @see genlmsg_user_data()
 *
 * @return Length of user data in bytes
 */
int genlmsg_user_datalen(struct genlmsghdr *gnlh, int hdrlen)
{
	return genlmsg_len(gnlh) - NLMSG_ALIGN(hdrlen);
}

/**
 * Return pointer to message attributes
 * @arg gnlh		Generic Netlink message header
 * @arg hdrlen		Length of user header
 *
 * @see genlmsg_attrlen()
 *
 * @return Pointer to the start of the message's attributes section.
 */
static struct nlattr *genlmsg_attrdata(struct genlmsghdr *gnlh, int hdrlen)
{
	return genlmsg_user_data(gnlh, hdrlen);
}

/**
 * Return length of message attributes
 * @arg gnlh		Generic Netlink message header
 * @arg hdrlen		Length of user header
 *
 * @see genlmsg_attrdata()
 *
 * @return Length of the message section containing attributes in number
 *         of bytes.
 */
static int genlmsg_attrlen(struct genlmsghdr *gnlh, int hdrlen)
{
	return genlmsg_len(gnlh) - NLMSG_ALIGN(hdrlen);
}

static void *genlmsg_put(struct nl_msg *msg, uint32_t port, uint32_t seq, 
	uint16_t family, uint16_t flags, uint8_t cmd, uint8_t version)
{
	struct genlmsghdr *hdr;

	hdr = nlmsg_put(msg, port, seq, family, GENL_HDRLEN, flags);
	BUG_ON(!hdr);

	hdr->cmd = cmd;
	hdr->version = version;

	return genlmsg_data(hdr);
}

/**
 * @return true if the headers are valid or false if not.
 */
static int genlmsg_valid_hdr(struct nlmsghdr *nlh, int hdrlen)
{
	struct genlmsghdr *ghdr;

	if (!nlmsg_valid_hdr(nlh, GENL_HDRLEN))
		return 0;

	ghdr = nlmsg_data(nlh);
	if (genlmsg_len(ghdr) < NLMSG_ALIGN(hdrlen))
		return 0;

	return 1;
}

static int genlmsg_parse(struct nlmsghdr *nlh, int hdrlen, struct nlattr **tb,
	int maxtype)
{
	struct genlmsghdr *ghdr;

	if (!genlmsg_valid_hdr(nlh, hdrlen))
		return -EPERM;

	ghdr = nlmsg_data(nlh);

	return nla_parse(tb, maxtype, genlmsg_attrdata(ghdr, hdrlen),
			 genlmsg_attrlen(ghdr, hdrlen));
}

static int nl_send_iovec(int sockfd, struct nl_msg *msg, struct iovec *iov, 
	size_t iovlen)
{
	int ret;
	struct msghdr hdr = {
		.msg_name = &msg->nm_dst,
		.msg_namelen = sizeof(struct sockaddr_nl),
		.msg_iov = iov,
		.msg_iovlen = iovlen,
	};

	ret = sendmsg(sockfd, &hdr, 0);
	if (ret < 0) {
		pr_err("failed to send msg!(%d)\n", ret);
		return ret;
	}

	return 0;
}

static int nl_recv_iovec(int sockfd, struct nl_msg *msg, struct iovec *iov,
	size_t iovlen)
{
	int ret;
	struct msghdr hdr = {
		.msg_name = &msg->nm_dst,
		.msg_namelen = sizeof(struct sockaddr_nl),
		.msg_iov = iov,
		.msg_iovlen = iovlen,
	};

	ret = recvmsg(sockfd, &hdr, 0);
	if (ret < 0) {
		pr_err("failed to recv msg!(%d)\n", ret);
		return ret;
	}

	return 0;
}

static int nl_send(int sockfd, struct nl_msg *msg)
{
	struct iovec iov = {
		.iov_base = (void *)msg->nm_nlh,
		.iov_len = msg->nm_nlh->nlmsg_len,
	};

	return nl_send_iovec(sockfd, msg, &iov, 1);
}

static int nl_recv(int sockfd, struct nl_msg *msg)
{
	struct iovec iov = {
		.iov_base = (void *)msg->nm_nlh,
		.iov_len = msg->nm_size,
	};

	return nl_recv_iovec(sockfd, msg, &iov, 1);
}

int nvme_gnl_cmd_reap_cqe(struct nvme_dev_info *ndev, uint16_t cqid,
	uint32_t expect, void *buf, uint32_t size)
{
	struct nvme_dev_public *pub = &ndev->dev_pub;
	struct nl_msg *msg;
	struct nlattr *tb[NVME_GNL_ATTR_MAX + 1];
	struct genlmsghdr *ghdr;
	unsigned long ptr = (unsigned long)buf;
	int ret;

	msg = nlmsg_alloc();
	if (!msg) {
		pr_err("failed to alloc msg!\n");
		return -ENOMEM;
	}
	nlmsg_init(msg);

	genlmsg_put(msg, msg->nm_src.nl_pad, 0, pub->family, 
		NLM_F_REQUEST, NVME_GNL_CMD_REAP_CQE, 1);

	nla_put_s32(msg, NVME_GNL_ATTR_DEVNO, pub->devno);
	nla_put_s32(msg, NVME_GNL_ATTR_TIMEOUT, 5000);
	nla_put_u16(msg, NVME_GNL_ATTR_CQID, cqid);
	nla_put_u32(msg, NVME_GNL_ATTR_OPT_NUM, expect);
	nla_put_u64(msg, NVME_GNL_ATTR_OPT_BUF_PTR, ptr);
	nla_put_u32(msg, NVME_GNL_ATTR_OPT_BUF_SIZE, size);

	ret = nl_send(ndev->sock_fd, msg);
	if (ret < 0) {
		pr_err("failed to send msg!\n");
		goto out;
	}

	ret = nl_recv(ndev->sock_fd, msg);
	if (ret < 0) {
		pr_err("failed to recv msg!\n");
		goto out;
	}

	ret = genlmsg_parse(msg->nm_nlh, 0, tb, NVME_GNL_ATTR_MAX);
	if (ret < 0) {
		pr_err("failed to parse genlmsg!(%d)\n", ret);
		goto out;
	}

	ghdr = nlmsg_data(msg->nm_nlh);
	if (ghdr->cmd != NVME_GNL_CMD_REAP_CQE) {
		pr_err("cmd:%u err!\n", ghdr->cmd);
		ret = -EPERM;
		goto out;
	}

	if (!tb[NVME_GNL_ATTR_OPT_STATUS]) {
		pr_err("attr status not exist!\n");
		ret = -EPERM;
		goto out;
	}

	ret = nla_get_s32(tb[NVME_GNL_ATTR_OPT_STATUS]);
	if (ret < 0) {
		pr_err("failed to reap CQ entry!(%d)\n", ret);
		goto out;
	}

	if (!tb[NVME_GNL_ATTR_OPT_NUM]) {
		pr_err("attr opt num not exist!\n");
		ret = -EPERM;
		goto out;
	}
	ret = (int)nla_get_u32(tb[NVME_GNL_ATTR_OPT_NUM]);
	if (ret != expect)
		pr_warn("timeout! expect:%u, actual:%d\n", expect, ret);

out:
	nlmsg_free(msg);
	return ret;
}

int nvme_gnl_connect(void)
{
	struct sockaddr_nl addr;
	int sockfd = -1;
	int ret;

	sockfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (sockfd < 0) {
		pr_err("failed to create socket!(%d)\n", sockfd);
		return sockfd;
	}

	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();
	addr.nl_groups = 0;

	ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_nl));
	if (ret < 0)
		goto out;

	return sockfd;
out:
	close(sockfd);
	return ret;
}

void nvme_gnl_disconnect(int sockfd)
{
	close(sockfd);
}

