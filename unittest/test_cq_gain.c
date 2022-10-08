/**
 * @file test_cq_gain.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <stdint.h>
#include <unistd.h>

#include "../dnvme_interface.h"
#include "../dnvme_ioctls.h"

#include "common.h"
#include "test_metrics.h"
#include "test_irq.h"
#include "test_cq_gain.h"

void *buffer_cq_entry;
char *tmpfile_dump = "/tmp/dump_cq.txt";

int disp_cq_data(unsigned char *cq_buffer, int reap_num)
{
	int ret_val = SUCCEED;
	struct cq_completion *cq_entry = NULL;
	while (reap_num)
	{
		cq_entry = (struct cq_completion *)cq_buffer;
		if (cq_entry->status_field) // status != 0 !!!!! force display
		{
			LOG_WARN("Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
				cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
				cq_entry->sq_identifier, cq_entry->status_field);
			ret_val = FAILED;
			/*for debug*/
			// ioctl_dump(file_desc, tmpfile_dump);
			// exit(-1);
			/*for debug*/
		}
		else
		{
			LOG_DBUG("Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
				cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
				cq_entry->sq_identifier, cq_entry->status_field);
		}
		reap_num--;
		cq_buffer += sizeof(struct cq_completion);
	}
	return ret_val;
}

int cq_gain(uint16_t cq_id, uint32_t expect_num, uint32_t *reaped_num)
{
	int ret_val = SUCCEED;
	struct nvme_reap rp_cq  = {0};
	uint64_t cq_to_cnt = 0;

	*reaped_num = 0;

	// check cq id
	if (cq_id > g_nvme_dev.max_cq_num)
	{
		LOG_ERROR("cq_id is exceed!!!\n");
		return FAILED;
	}

	rp_cq.q_id = cq_id;
	rp_cq.elements = expect_num;
	rp_cq.size = (uint32_t)BUFFER_CQ_ENTRY_SIZE;
	rp_cq.buffer = (uint8_t *)buffer_cq_entry;

	while (*reaped_num < expect_num)
	{
		ret_val = ioctl(file_desc, NVME_IOCTL_REAP, &rp_cq);
		if (ret_val)
		{
			LOG_ERROR("Call cq_gain ioctl failed!!! cq_id: %d, expect_num: %d\n", cq_id, expect_num);
			return FAILED;
		}

		if (rp_cq.num_reaped)
		{
			*reaped_num += rp_cq.num_reaped;
			//full parameter for next
			rp_cq.elements = expect_num - (*reaped_num);
			rp_cq.size = (uint32_t)(rp_cq.size - rp_cq.num_reaped * CQ_ENTRY_SIZE);
			rp_cq.buffer = (uint8_t *)(rp_cq.buffer + rp_cq.num_reaped * CQ_ENTRY_SIZE);
			cq_to_cnt = 0; //if reaped, timeout cnt clear to 0;
			// LOG_INFO("num_reaped:%d\n",rp_cq.num_reaped); //XXXX for test cq int coalescing , pls open this
		}

		usleep(10);
		cq_to_cnt++;
		if (cq_to_cnt >= CQ_REAP_TO_US) //timeout
		{
			LOG_ERROR("cq_gain timeout, cq_id: %d, expect_num: %d, reaped_num:%d, %lds!!!\n",
				cq_id,
				expect_num,
				*reaped_num,
				CQ_REAP_TO_US/(1000*100));
			ret_val = FAILED;
			break;
		}
	}
	if (disp_cq_data(buffer_cq_entry, *reaped_num))
	{
		ret_val = FAILED;
	}
	return ret_val;
}

int cq_gain_disp_cq(uint16_t cq_id, uint32_t expect_num, uint32_t *reaped_num , uint32_t disp_cq)
{
	int ret_val = SUCCEED;
	struct nvme_reap rp_cq  = {0};
	uint64_t cq_to_cnt = 0;

	*reaped_num = 0;

	if (cq_id > g_nvme_dev.max_cq_num)
	{
		LOG_ERROR("cq_id is exceed!!!\n");
		return FAILED;
	}

	rp_cq.q_id = cq_id;
	rp_cq.elements = expect_num;
	rp_cq.size = (uint32_t)BUFFER_CQ_ENTRY_SIZE;
	rp_cq.buffer = (uint8_t *)buffer_cq_entry;

	while (*reaped_num < expect_num)
	{
		ret_val = ioctl(file_desc, NVME_IOCTL_REAP, &rp_cq);
		if (ret_val)
		{
			LOG_ERROR("call cq_gain ioctl failed!!! cq_id: %d, expect_num: %d\n", cq_id, expect_num);
			return FAILED;
		}

		if (rp_cq.num_reaped)
		{
			*reaped_num += rp_cq.num_reaped;
			//full parameter for next
			rp_cq.elements = expect_num - (*reaped_num);
			rp_cq.size = (uint32_t)(rp_cq.size - rp_cq.num_reaped * sizeof(struct cq_completion));
			rp_cq.buffer = (uint8_t *)(rp_cq.buffer + rp_cq.num_reaped * sizeof(struct cq_completion));
			cq_to_cnt = 0; //if reaped, timeout cnt clear to 0;
		}

		usleep(10);
		cq_to_cnt++;
		if (cq_to_cnt >= CQ_REAP_TO_US) //timeout
		{
			LOG_ERROR("cq_gain timeout, cq_id: %d, expect_num: %d, reaped_num:%d, %lds!!!\n",
				cq_id,
				expect_num,
				*reaped_num,
				CQ_REAP_TO_US/(1000*100));
			ret_val = FAILED;
			break;
		}
	}
	if(disp_cq)
	{
		ret_val = disp_cq_data(buffer_cq_entry, *reaped_num);
	}
	return ret_val;
}


struct cq_completion *get_cq_entry(void)
{
	return (struct cq_completion *)buffer_cq_entry;
}

/***********************/
//for command_arbitration
int arb_reap_all_cq(struct arbitration_parameter *arb_parameter)
{
	int ret_val = -1;
	struct nvme_reap rp_cq = {0};
	uint64_t cq_to_cnt = 0;

	uint16_t i = 0;
	uint16_t cq_id = 0;
	uint32_t reaped_num = 0;

	struct cq_completion *cq_entry = NULL;
	unsigned char *cq_buffer = NULL;

	uint32_t loop = 0;
	uint32_t cq_cmd_cnt = 0;

	uint32_t urgent_err_cnt = 0;
	uint32_t high_err_cnt = 0;
	uint32_t medium_err_cnt = 0;
	uint32_t low_err_cnt = 0;

	uint32_t urgent_cnt = 0;
	uint32_t high_cnt = 0;
	uint32_t medium_cnt = 0;
	uint32_t low_cnt = 0;

	// uint32_t loop_data = 0;

	uint32_t left_data_high = 0;
	uint32_t left_data_medium = 0;
	uint32_t left_data_low = 0;

	uint32_t right_data_high = 0;
	uint32_t right_data_medium = 0;
	uint32_t right_data_low = 0;

	// uint32_t cmd_sum_num = 0;

	rp_cq.elements = 0; //
	rp_cq.size = (uint32_t)BUFFER_CQ_ENTRY_SIZE;
	rp_cq.buffer = (uint8_t *)buffer_cq_entry;
	while (reaped_num < arb_parameter->expect_num)
	{
		for (i = 1; i <= 8; i++)
		{
			cq_id = i;
			rp_cq.q_id = cq_id;
			ret_val = ioctl(file_desc, NVME_IOCTL_REAP, &rp_cq);
			if (ret_val)
			{
				LOG_ERROR("call cq_gain ioctl failed!!! cq_id: %d, expect_num: %d\n", cq_id, arb_parameter->expect_num);
				exit(-1);
				return ret_val;
			}
			if (rp_cq.num_reaped)
			{
				reaped_num += rp_cq.num_reaped;
				//full parameter for next
				rp_cq.size = (uint32_t)(rp_cq.size - rp_cq.num_reaped * CQ_ENTRY_SIZE);
				rp_cq.buffer = (uint8_t *)(rp_cq.buffer + rp_cq.num_reaped * CQ_ENTRY_SIZE);
				cq_to_cnt = 0; //if reaped, timeout cnt clear to 0;
			}
		}
		usleep(1000);
		cq_to_cnt++;
		if (cq_to_cnt >= CQ_REAP_TO_US) //timeout
		{
			LOG_ERROR("cq_gain timeout, cq_id: %d, expect_num: %d, reaped_num:%d, %ldms!!!\n",
				cq_id,
				arb_parameter->expect_num,
				reaped_num,
				CQ_REAP_TO_US);
			ret_val = -1;
			break;
		}
	}

	// loop_data = arb_parameter->Arbit_HPW + 1 + arb_parameter->Arbit_MPW + 1 + arb_parameter->Arbit_LPW + 1;
	// cmd_sum_num = arb_parameter->hight_prio_cmd_num + arb_parameter->medium_prio_cmd_num + arb_parameter->low_prio_cmd_num;

	loop = 0;
	cq_buffer = buffer_cq_entry;
	for (cq_cmd_cnt = 1; cq_cmd_cnt <= reaped_num; cq_cmd_cnt++)
	{
		cq_entry = (struct cq_completion *)cq_buffer;
		if (cq_entry->status_field) // status != 0 !!!!! force display
		{
			LOG_WARN("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
				cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
				cq_entry->sq_identifier, cq_entry->status_field);
			ret_val = -1;
		}
		else
		{
			LOG_INFO("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
				cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
				cq_entry->sq_identifier, cq_entry->status_field);
		}
		cq_buffer += sizeof(struct cq_completion);

		if ((cq_cmd_cnt >= 1) && (cq_cmd_cnt <= arb_parameter->urgent_prio_cmd_num))
		{
			if (URGENT_PRIO == get_q_prio(cq_entry->sq_identifier))
			{
				urgent_cnt++;
			}
			else
			{
				urgent_err_cnt++;
				LOG_ERROR("cq_cmd_cnt: %d, prio_urgent_err sq_id:%d, prio:%d\n",
					cq_cmd_cnt,
					cq_entry->sq_identifier,
					get_q_prio(cq_entry->sq_identifier));
			}
			if (cq_cmd_cnt == arb_parameter->urgent_prio_cmd_num) //在urgent最后一个cmd时，为high准备比较数据
			{
				left_data_high = arb_parameter->urgent_prio_cmd_num + 1;
				switch (get_prio_order(1))
				{
				case HIGH_PRIO:
					right_data_high = (left_data_high + (arb_parameter->Arbit_HPW + 1) - 1);
					break;
				case MEDIUM_PRIO:
					right_data_high = (left_data_high + (arb_parameter->Arbit_MPW + 1) - 1);
					break;
				case LOW_PRIO:
					right_data_high = (left_data_high + (arb_parameter->Arbit_LPW + 1) - 1);
					break;
				default:
					break;
				}
			}
		}
		else
		{
			if ((left_data_high <= cq_cmd_cnt) && (cq_cmd_cnt <= right_data_high))
			{
				if (get_prio_order(1) == get_q_prio(cq_entry->sq_identifier))
				{
					high_cnt++;
				}
				else
				{
					high_err_cnt++;
					LOG_ERROR("high_err L: %d--->R: %d, cq_cmd_cnt: %d, sq_id:%d, prio:%d\n",
						left_data_high,
						right_data_high,
						cq_cmd_cnt,
						cq_entry->sq_identifier,
						get_q_prio(cq_entry->sq_identifier));
				}
				if (cq_cmd_cnt == right_data_high) //在High最后一个cmd时，为medium准备比较数据
				{
					left_data_medium = right_data_high + 1;
					switch (get_prio_order(2))
					{
					case HIGH_PRIO:
						right_data_medium = (left_data_medium + (arb_parameter->Arbit_HPW + 1) - 1);
						break;
					case MEDIUM_PRIO:
						right_data_medium = (left_data_medium + (arb_parameter->Arbit_MPW + 1) - 1);
						break;
					case LOW_PRIO:
						right_data_medium = (left_data_medium + (arb_parameter->Arbit_LPW + 1) - 1);
						break;
					default:
						break;
					}
				}
			}
			else if ((left_data_medium <= cq_cmd_cnt) && (cq_cmd_cnt <= right_data_medium))
			{
				if (get_prio_order(2) == get_q_prio(cq_entry->sq_identifier))
				{
					medium_cnt++;
				}
				else
				{
					medium_err_cnt++;
					LOG_ERROR("medium_err L: %d--->R: %d, cq_cmd_cnt: %d, sq_id:%d, prio:%d\n",
						left_data_medium,
						right_data_medium,
						cq_cmd_cnt,
						cq_entry->sq_identifier,
						get_q_prio(cq_entry->sq_identifier));
				}
				if (cq_cmd_cnt == right_data_medium) //在medium最后一个cmd时，为low准备比较数据
				{
					left_data_low = right_data_medium + 1;
					switch (get_prio_order(3))
					{
					case HIGH_PRIO:
						right_data_low = (left_data_low + (arb_parameter->Arbit_HPW + 1) - 1);
						break;
					case MEDIUM_PRIO:
						right_data_low = (left_data_low + (arb_parameter->Arbit_MPW + 1) - 1);
						break;
					case LOW_PRIO:
						right_data_low = (left_data_low + (arb_parameter->Arbit_LPW + 1) - 1);
						break;
					default:
						break;
					}
				}
			}
			else if ((left_data_low <= cq_cmd_cnt) && (cq_cmd_cnt <= right_data_low))
			{
				if (get_prio_order(3) == get_q_prio(cq_entry->sq_identifier))
				{
					low_cnt++;
				}
				else
				{
					low_err_cnt++;
					LOG_ERROR("low_err L: %d--->R: %d, cq_cmd_cnt: %d, sq_id:%d, prio:%d\n",
						left_data_low,
						right_data_low,
						cq_cmd_cnt,
						cq_entry->sq_identifier,
						get_q_prio(cq_entry->sq_identifier));
				}
				if (cq_cmd_cnt == right_data_low) //在low最后一个cmd时，为high准备比较数据
				{
					left_data_high = right_data_low + 1;
					switch (get_prio_order(1))
					{
					case HIGH_PRIO:
						right_data_high = (left_data_high + (arb_parameter->Arbit_HPW + 1) - 1);
						break;
					case MEDIUM_PRIO:
						right_data_high = (left_data_high + (arb_parameter->Arbit_MPW + 1) - 1);
						break;
					case LOW_PRIO:
						right_data_high = (left_data_high + (arb_parameter->Arbit_LPW + 1) - 1);
						break;
					default:
						break;
					}

					loop++;
					switch (get_prio_order(1)) //最新回来的算出来循环次数
					{
					case HIGH_PRIO:
						if (loop == (get_higt_weight_q_min_num() / arb_parameter->Arbit_HPW))
						{
							LOG_COLOR(PURPLE_LOG, "loop is end :%d\n", loop);
							goto GOOUT;
						}
						break;
					case MEDIUM_PRIO:
						if (loop == (get_higt_weight_q_min_num() / arb_parameter->Arbit_MPW))
						{
							LOG_COLOR(PURPLE_LOG, "loop is end :%d\n", loop);
							goto GOOUT;
						}
						break;
					case LOW_PRIO:
						if (loop == (get_higt_weight_q_min_num() / arb_parameter->Arbit_LPW))
						{
							LOG_COLOR(PURPLE_LOG, "loop is end :%d\n", loop);
							goto GOOUT;
						}
						break;
					default:
						break;
					}
				}
			}
		}
	}
GOOUT:
	LOG_INFO("\nreaped_num: %d\n", reaped_num);
	LOG_INFO("Arbit_HPW: %d\n", arb_parameter->Arbit_HPW);
	LOG_INFO("Arbit_MPW: %d\n", arb_parameter->Arbit_MPW);
	LOG_INFO("Arbit_LPW: %d\n", arb_parameter->Arbit_LPW);
	LOG_INFO("Arbit_AB: %d\n", arb_parameter->Arbit_AB);

	LOG_INFO("urgent_cnt: %d, urgent_err_cnt: %d,\n"
				  "high_cnt: %d, high_err_cnt: %d,\n"
				  "medium_cnt: %d, medium_err_cnt: %d,\n"
				  "low_cnt: %d, low_err_cnt: %d \n",
		urgent_cnt, urgent_err_cnt,
		high_cnt, high_err_cnt,
		medium_cnt, medium_err_cnt,
		low_cnt, low_err_cnt);

	return ret_val;
}

int arb_reap_all_cq_2(uint8_t qnum, struct arbitration_parameter *arb_parameter)
{
	int ret_val = -1;
	struct nvme_reap rp_cq = {0};
	uint64_t cq_to_cnt = 0;

	uint16_t i = 0;
	uint16_t cq_id = 0;
	uint32_t reaped_num = 0;

	struct cq_completion *cq_entry = NULL;
	unsigned char *cq_buffer = NULL;

	uint32_t cq_cmd_cnt = 0;

	uint32_t urgent_err_cnt = 0;
	uint32_t high_err_cnt = 0;
	uint32_t medium_err_cnt = 0;
	uint32_t low_err_cnt = 0;

	uint32_t urgent_cnt = 0;
	uint32_t high_cnt = 0;
	uint32_t medium_cnt = 0;
	uint32_t low_cnt = 0;

	rp_cq.elements = 0; //
	rp_cq.size = (uint32_t)BUFFER_CQ_ENTRY_SIZE;
	rp_cq.buffer = (uint8_t *)buffer_cq_entry;
	while (reaped_num < arb_parameter->expect_num)
	{
		for (i = 1; i <= qnum; i++)
		{
			cq_id = i;
			rp_cq.q_id = cq_id;
			ret_val = ioctl(file_desc, NVME_IOCTL_REAP, &rp_cq);
			if (ret_val)
			{
				LOG_ERROR("call cq_gain ioctl failed!!! cq_id: %d, expect_num: %d\n", cq_id, arb_parameter->expect_num);
				exit(-1);
				return ret_val;
			}
			if (rp_cq.num_reaped)
			{
				reaped_num += rp_cq.num_reaped;
				//full parameter for next
				rp_cq.size = (uint32_t)(rp_cq.size - rp_cq.num_reaped * CQ_ENTRY_SIZE);
				rp_cq.buffer = (uint8_t *)(rp_cq.buffer + rp_cq.num_reaped * CQ_ENTRY_SIZE);
				cq_to_cnt = 0; //if reaped, timeout cnt clear to 0;
			}
		}
		usleep(10);
		cq_to_cnt++;
		if (cq_to_cnt >= CQ_REAP_TO_US) //timeout
		{
			LOG_ERROR("cq_gain timeout, cq_id: %d, expect_num: %d, reaped_num:%d, %ldms!!!\n",
				cq_id,
				arb_parameter->expect_num,
				reaped_num,
				CQ_REAP_TO_US);
			ret_val = -1;
			break;
		}
	}

	// loop_data = arb_parameter->Arbit_HPW + 1 + arb_parameter->Arbit_MPW + 1 + arb_parameter->Arbit_LPW + 1;
	// cmd_sum_num = arb_parameter->hight_prio_cmd_num + arb_parameter->medium_prio_cmd_num + arb_parameter->low_prio_cmd_num;

	cq_buffer = buffer_cq_entry;
	for (cq_cmd_cnt = 1; cq_cmd_cnt <= reaped_num; cq_cmd_cnt++)
	{
		cq_entry = (struct cq_completion *)cq_buffer;
		if (cq_entry->status_field) // status != 0 !!!!! force display
		{
			LOG_WARN("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
				cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
				cq_entry->sq_identifier, cq_entry->status_field);
			ret_val = -1;
		}
		else
		{
			LOG_INFO("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
				cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
				cq_entry->sq_identifier, cq_entry->status_field);
		}
		cq_buffer += sizeof(struct cq_completion);
		if ((cq_cmd_cnt >= 1) && (cq_cmd_cnt <= arb_parameter->urgent_prio_cmd_num))
		{
			if (URGENT_PRIO == get_q_prio(cq_entry->sq_identifier))
			{
				urgent_cnt++;
			}
			else
			{
				urgent_err_cnt++;
				LOG_ERROR("cq_cmd_cnt: %d, prio_urgent_err sq_id:%d, prio:%d\n",
					cq_cmd_cnt,
					cq_entry->sq_identifier,
					get_q_prio(cq_entry->sq_identifier));
			}
		}
		else
		{
			if (HIGH_PRIO == get_q_prio(cq_entry->sq_identifier))
			{
				high_cnt++;
			}
			else if (MEDIUM_PRIO == get_q_prio(cq_entry->sq_identifier))
			{
				medium_cnt++;
			}
			else if (LOW_PRIO == get_q_prio(cq_entry->sq_identifier))
			{
				low_cnt++;
			}
			else
			{
				LOG_ERROR("q prio error! id: %d, prio:%d\n", cq_entry->sq_identifier, get_q_prio(cq_entry->sq_identifier));
			}
		}
	}
	LOG_INFO("\nreaped_num: %d\n", reaped_num);
	LOG_INFO("Arbit_HPW: %d\n", arb_parameter->Arbit_HPW);
	LOG_INFO("Arbit_MPW: %d\n", arb_parameter->Arbit_MPW);
	LOG_INFO("Arbit_LPW: %d\n", arb_parameter->Arbit_LPW);
	LOG_INFO("Arbit_AB: %d\n", arb_parameter->Arbit_AB);

	LOG_INFO("urgent_cnt: %d, urgent_err_cnt: %d,\n"
				  "high_cnt: %d, high_err_cnt: %d,\n"
				  "medium_cnt: %d, medium_err_cnt: %d,\n"
				  "low_cnt: %d, low_err_cnt: %d \n",
		urgent_cnt, urgent_err_cnt,
		high_cnt, high_err_cnt,
		medium_cnt, medium_err_cnt,
		low_cnt, low_err_cnt);

	return ret_val;
}
