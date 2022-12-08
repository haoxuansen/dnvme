#ifndef _TEST_CQ_GAIN_H_
#define _TEST_CQ_GAIN_H_

#define BUFFER_CQ_ENTRY_SIZE (64 * 1024 * 16) // q_size 64K

#define CQ_ENTRY_SIZE 16
//                                          s
#define CQ_REAP_TO_US (uint64_t)(100*1000* 10) // max timeout time

struct arbitration_parameter
{
    uint8_t Arbit_HPW;
    uint8_t Arbit_MPW;
    uint8_t Arbit_LPW;
    uint8_t Arbit_AB;

    uint32_t expect_num; // all q expect reap cq num

    uint32_t urgent_prio_cmd_num;
    uint32_t hight_prio_cmd_num;
    uint32_t medium_prio_cmd_num;
    uint32_t low_prio_cmd_num;
};

int cq_gain(uint16_t cq_id, uint32_t expect_num, uint32_t *reap_num);
int cq_gain_disp_cq(uint16_t cq_id, uint32_t expect_num, uint32_t *reaped_num , uint32_t disp_cq);
struct nvme_completion *get_cq_entry(void);

int arb_reap_all_cq(struct arbitration_parameter *arb_parameter); //for command_arbitration
enum nvme_sq_prio get_q_prio(uint16_t x);                        //for command_arbitration in case_command_arbitration.c
uint32_t get_higt_weight_q_min_num(void);
enum nvme_sq_prio get_prio_order(uint8_t i);
int arb_reap_all_cq_2(uint8_t qnum, struct arbitration_parameter *arb_parameter);

int disp_cq_data(unsigned char *cq_buffer, int reap_num);

#endif
