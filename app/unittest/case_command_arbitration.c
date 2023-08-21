#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"
static int test_flag = 0;

static char *disp_this_case = "this case will tests command_arbitration\n"
                              "this case must use sq1 assoc cq1 and so on\n"
                              "default parameter: write/read slba=0, nlb = wr_nlb\n";

uint8_t Arbit_HPW = 0;
uint8_t Arbit_MPW = 0;
uint8_t Arbit_LPW = 0;
uint8_t Arbit_AB = 0;

uint8_t HPW_QID = 4;
uint8_t MPW_QID = 2;
uint8_t LPW_QID = 1;
uint8_t UPW_QID = 3;

uint32_t sq1_cmd_num = 500;
uint32_t sq2_cmd_num = 500;
uint32_t sq3_cmd_num = 500;
uint32_t sq4_cmd_num = 500;
// uint32_t sq5_cmd_num = 500;
// uint32_t sq6_cmd_num = 500;
// uint32_t sq7_cmd_num = 500;
// uint32_t sq8_cmd_num = 500;

//获取sq的优先级类型
enum nvme_sq_prio get_q_prio(uint16_t id)
{
    if ((id == LPW_QID))
    {
        return LOW_PRIO;
    }
    else if ((id == HPW_QID))
    {
        return HIGH_PRIO;
    }
    else if ((id == MPW_QID))
    {
        return MEDIUM_PRIO;
    }
    else if ((id == UPW_QID))
    {
        return URGENT_PRIO;
    }
    else
    {
        pr_err("get_q_prio q_id %d error!", id);
        exit(-1);
    }
}

//获取sq的cmd的个数
uint32_t get_q_cmd_num(uint8_t id)
{
    uint32_t ret = 0;
    switch (id)
    {
    case 1:
        ret = sq1_cmd_num;
        break;
    case 2:
        ret = sq2_cmd_num;
        break;
    case 3:
        ret = sq3_cmd_num;
        break;
    case 4:
        ret = sq4_cmd_num;
        break;
    // case 5:
    //     ret = sq5_cmd_num;
    //     break;
    // case 6:
    //     ret = sq6_cmd_num;
    //     break;
    // case 7:
    //     ret = sq7_cmd_num;
    //     break;
    // case 8:
    //     ret = sq8_cmd_num;
    //     break;
    default:
        pr_err("get_q_cmd_num q_id %d error!", id);
        exit(-1);
        break;
    }
    return ret;
}

//获取sq的优先级顺序
enum nvme_sq_prio get_prio_order(uint8_t i)
{
    enum nvme_sq_prio prio_order = LOW_PRIO;
    if (Arbit_HPW >= Arbit_MPW)
    {
        if (Arbit_HPW >= Arbit_LPW) //high is up!
        {
            if (i == 1)
            {
                prio_order = HIGH_PRIO;
            }
            if (Arbit_MPW >= Arbit_LPW) //medium is --- ; low is down
            {
                if (i == 2)
                {
                    prio_order = MEDIUM_PRIO;
                }
                else if (i == 3)
                {
                    prio_order = LOW_PRIO;
                }
                else
                {
                    /* code */
                }
            }
            else //low is --- ; medium is down
            {
                if (i == 2)
                {
                    prio_order = LOW_PRIO;
                }
                else if (i == 3)
                {
                    prio_order = MEDIUM_PRIO;
                }
                else
                {
                    /* code */
                }
            }
        }
        else //low is up ; high is --- ; medium is down
        {
            if (i == 1)
            {
                prio_order = LOW_PRIO;
            }
            else if (i == 2)
            {
                prio_order = HIGH_PRIO;
            }
            else if (i == 3)
            {
                prio_order = MEDIUM_PRIO;
            }
            else
            {
                /* code */
            }
        }
    }
    else
    {
        if (Arbit_MPW >= Arbit_LPW) //medium is up!
        {
            if (i == 1)
            {
                prio_order = MEDIUM_PRIO;
            }
            if (Arbit_HPW >= Arbit_LPW) //high is ---;low is down
            {
                if (i == 2)
                {
                    prio_order = HIGH_PRIO;
                }
                else if (i == 3)
                {
                    prio_order = LOW_PRIO;
                }
                else
                {
                    /* code */
                }
            }
            else //low is ---;high is down
            {
                if (i == 2)
                {
                    prio_order = LOW_PRIO;
                }
                else if (i == 3)
                {
                    prio_order = HIGH_PRIO;
                }
                else
                {
                    /* code */
                }
            }
        }
        else //low is up;medium is ---;high is down
        {
            if (i == 1)
            {
                prio_order = LOW_PRIO;
            }
            else if (i == 2)
            {
                prio_order = MEDIUM_PRIO;
            }
            else if (i == 3)
            {
                prio_order = HIGH_PRIO;
            }
            else
            {
                /* code */
            }
        }
    }
    return prio_order;
}

//找出高权重中的sq里面，较小cmd个数
uint32_t get_higt_weight_q_min_num(void)
{
    uint8_t i = 0;
    uint8_t flg = 0;
    uint32_t ret = 0;
    for (i = 1; i <= 4; i++)
    {
        if (get_prio_order(1) == get_q_prio(i))
        {
            // 找出来最小的cmd个数
            if (flg == 0)
            {
                flg = 1;
                ret = get_q_cmd_num(i);
            }
            if (ret > get_q_cmd_num(i))
            {
                ret = get_q_cmd_num(i);
            }
            //pr_info("i:%d, get_q_cmd_num(i):%d\n",i,get_q_cmd_num(i));
        }
    }
    return ret;
}

static void test_sub(void)
{
    uint32_t q_index = 0;
    uint32_t index = 0;
    uint32_t cmd_cnt = 0;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    struct nvme_tool *tool = g_nvme_tool;
    struct nvme_dev_info *ndev = tool->ndev;
    struct nvme_ctrl_property *prop = ndev->ctrl->prop;
    uint32_t cq_size = NVME_CAP_MQES(prop->cap);
    uint32_t sq_size = NVME_CAP_MQES(prop->cap);
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 0;
    uint32_t wr_nsid = 1;
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    uint32_t reap_num = 0;

    struct arbitration_parameter arb_parameter = {0};

    pr_color(LOG_N_PURPLE, "this case will tests command_arbitration\n");
    /*******************************************************************************************************************************/
    /**********************************************************************/
    pr_info("create SQ1 prio = LOW_PRIO\n");
    /**********************************************************************/
    for (q_index = 1; q_index <= 1; q_index++)
    {
        io_cq_id = q_index;
        io_sq_id = q_index;
        cq_parameter.cq_id = io_cq_id;
        cq_parameter.cq_size = cq_size;
        cq_parameter.contig = 1;
        cq_parameter.irq_en = 1;
        cq_parameter.irq_no = io_cq_id;
        test_flag |= create_iocq(ndev->fd, &cq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

        sq_parameter.cq_id = io_cq_id;
        sq_parameter.sq_id = io_sq_id;
        sq_parameter.sq_size = sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = LOW_PRIO;
        create_iosq(ndev->fd, &sq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    }
    /**********************************************************************/
    pr_info("create SQ2 prio = MEDIUM_PRIO\n");
    /**********************************************************************/
    for (q_index = 2; q_index <= 2; q_index++)
    {
        io_cq_id = q_index;
        io_sq_id = q_index;
        cq_parameter.cq_id = io_cq_id;
        cq_parameter.cq_size = cq_size;
        cq_parameter.contig = 1;
        cq_parameter.irq_en = 1;
        cq_parameter.irq_no = io_cq_id;
        create_iocq(ndev->fd, &cq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

        sq_parameter.cq_id = io_cq_id;
        sq_parameter.sq_id = io_sq_id;
        sq_parameter.sq_size = sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = MEDIUM_PRIO;
        create_iosq(ndev->fd, &sq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    }
    /**********************************************************************/
    pr_info("create SQ3 prio = URGENT_PRIO\n");
    /**********************************************************************/
    for (q_index = 3; q_index <= 3; q_index++)
    {
        io_cq_id = q_index;
        io_sq_id = q_index;
        cq_parameter.cq_id = io_cq_id;
        cq_parameter.cq_size = cq_size;
        cq_parameter.contig = 1;
        cq_parameter.irq_en = 1;
        cq_parameter.irq_no = io_cq_id;
        create_iocq(ndev->fd, &cq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

        sq_parameter.cq_id = io_cq_id;
        sq_parameter.sq_id = io_sq_id;
        sq_parameter.sq_size = sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = URGENT_PRIO;
        create_iosq(ndev->fd, &sq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    }
    /**********************************************************************/
    pr_info("create SQ4 prio = HIGH_PRIO\n");
    /**********************************************************************/
    for (q_index = 4; q_index <= 4; q_index++)
    {
        io_cq_id = q_index;
        io_sq_id = q_index;
        cq_parameter.cq_id = io_cq_id;
        cq_parameter.cq_size = cq_size;
        cq_parameter.contig = 1;
        cq_parameter.irq_en = 1;
        cq_parameter.irq_no = io_cq_id;
        create_iocq(ndev->fd, &cq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);

        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

        sq_parameter.cq_id = io_cq_id;
        sq_parameter.sq_id = io_sq_id;
        sq_parameter.sq_size = sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = HIGH_PRIO;
        create_iosq(ndev->fd, &sq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    }
    /**********************************************************************/

    cmd_cnt = 0;
    wr_slba = 0;
    wr_nlb = 8;
    /**********************************************************************/
    for (index = 0; index < sq1_cmd_num; index++)
    {
        io_sq_id = 1;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
    }
    for (index = 0; index < sq2_cmd_num; index++)
    {
        io_sq_id = 2;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
    }

    /**********************************************************************/
    for (index = 0; index < sq3_cmd_num; index++)
    {
        io_sq_id = 3;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
    }
    for (index = 0; index < sq4_cmd_num; index++)
    {
        io_sq_id = 4;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
    }

    // /**********************************************************************/
    // for (index = 0; index < sq5_cmd_num; index++)
    // {
    //     io_sq_id = 5;
    //     test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    //     cmd_cnt++;
    // }
    // for (index = 0; index < sq6_cmd_num; index++)
    // {
    //     io_sq_id = 6;
    //     test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    //     cmd_cnt++;
    // }

    // /**********************************************************************/
    // for (index = 0; index < sq7_cmd_num; index++)
    // {
    //     io_sq_id = 7;
    //     test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    //     cmd_cnt++;
    // }
    // for (index = 0; index < sq8_cmd_num; index++)
    // {
    //     io_sq_id = 8;
    //     test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    //     cmd_cnt++;
    // }

    /**********************************************************************/

    // pr_info("!!!!!!!!!!!!!!!!!!!!!before dbl sleep(20)\n");
    // sleep(20);
    io_sq_id = 3;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);

    io_sq_id = 1;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);

    io_sq_id = 2;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);

    io_sq_id = 4;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);

    // io_sq_id = 5;
    // test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);

    // io_sq_id = 6;
    // test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);

    // io_sq_id = 7;
    // test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);

    // io_sq_id = 8;
    // test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);

    // pr_info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@after dbl sleep(30)\n");
    // sleep(30);

    arb_parameter.urgent_prio_cmd_num = sq3_cmd_num;
    arb_parameter.hight_prio_cmd_num = sq4_cmd_num;
    arb_parameter.medium_prio_cmd_num = sq2_cmd_num;
    arb_parameter.low_prio_cmd_num = sq1_cmd_num;

    arb_parameter.Arbit_AB = Arbit_AB;
    arb_parameter.Arbit_HPW = Arbit_HPW;
    arb_parameter.Arbit_LPW = Arbit_LPW;
    arb_parameter.Arbit_MPW = Arbit_MPW;

    arb_parameter.expect_num = cmd_cnt;
    test_flag |= arb_reap_all_cq_2(4, &arb_parameter);

    /*******************************************************************************************************************************/
    pr_info("delete all sq\n");
    for (q_index = 1; q_index <= 4; q_index++)
    {
        io_cq_id = q_index;
        io_sq_id = q_index;
        /**********************************************************************/
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);

        pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
        /**********************************************************************/
    }
    /****************************************************************************
     * ***************************************************/
}

static int case_command_arbitration(struct nvme_tool *tool)
{
    struct nvme_dev_info *ndev = tool->ndev;
    int test_round = 0;
    uint32_t u32_tmp_data = 0;
    uint32_t reap_num = 0;
    int ret;

    struct nvme_completion *cq_entry = get_cq_entry();

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s", disp_this_case);

    ret = nvme_read_ctrl_property(ndev->fd, 0x0, 4, &u32_tmp_data);
    if (ret < 0)
    	return ret;

    //open W R-R Urgent Prio Class
    ret = nvme_read_ctrl_property(ndev->fd, 0x14, 4, &u32_tmp_data);
    if (ret < 0)
    	return ret;

    u32_tmp_data |= (1 << 11); // bit 13:11, 000:R-R, 001:W R-R Urgent prio
    nvme_write_ctrl_property(ndev->fd, NVME_REG_CC, 4, (uint8_t *)&u32_tmp_data);

    ret = nvme_read_ctrl_property(ndev->fd, 0x14, 4, &u32_tmp_data);
    if (ret < 0)
    	return ret;

    /**********************************************************************/

    cq_entry = send_get_feature(ndev->fd, NVME_FEAT_ARBITRATION);
    // must be admin queue
    if (0 == cq_entry->sq_id)
    {
        Arbit_HPW = (uint8_t)((cq_entry->result.u32 & 0xFF000000) >> 24);
        Arbit_MPW = (uint8_t)((cq_entry->result.u32 & 0xFF0000) >> 16);
        Arbit_LPW = (uint8_t)((cq_entry->result.u32 & 0xFF00) >> 8);
        Arbit_AB = (uint8_t)(cq_entry->result.u32 & 0x7);
    }
    else
    {
        pr_err("acq_num or sq_id is wrong!!!\n");
    }
    pr_info("Arbit_HPW: %d\n", Arbit_HPW);
    pr_info("Arbit_MPW: %d\n", Arbit_MPW);
    pr_info("Arbit_LPW: %d\n", Arbit_LPW);
    pr_info("Arbit_AB: %d\n", Arbit_AB);

    /**********************************************************************/
    //set Arbitration (Feature Identifier 01h)
    Arbit_HPW = 250; //u8 This is a 0’s based value.
    Arbit_MPW = 150; //u8 This is a 0’s based value.
    Arbit_LPW = 10;  //u8 This is a 0’s based value.
    Arbit_AB = 4;    //2^n Arbitration Burst (AB):
    test_flag |= nvme_set_feature_cmd(ndev->fd, 1, NVME_FEAT_ARBITRATION, ((Arbit_LPW << 8) | (Arbit_AB & 0x07)), ((Arbit_HPW << 8) | Arbit_MPW));
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, 0);
    test_flag |= cq_gain(0, 1, &reap_num);

    /**********************************************************************/
    cq_entry = send_get_feature(ndev->fd, NVME_FEAT_ARBITRATION);
    // must be admin queue
    if (0 == cq_entry->sq_id)
    {
        Arbit_HPW = (uint8_t)((cq_entry->result.u32 & 0xFF000000) >> 24);
        Arbit_MPW = (uint8_t)((cq_entry->result.u32 & 0xFF0000) >> 16);
        Arbit_LPW = (uint8_t)((cq_entry->result.u32 & 0xFF00) >> 8);
        Arbit_AB = (uint8_t)(cq_entry->result.u32 & 0x7);
    }
    else
    {
        pr_err("acq_num or sq_id is wrong!!!\n");
    }
    pr_info("Arbit_HPW: %d\n", Arbit_HPW);
    pr_info("Arbit_MPW: %d\n", Arbit_MPW);
    pr_info("Arbit_LPW: %d\n", Arbit_LPW);
    pr_info("Arbit_AB: %d\n", Arbit_AB);
    /**********************************************************************/
    pr_info("get_higt_weight_q_min_num(): %d\n", get_higt_weight_q_min_num());
    pr_info("get_prio_order(1): %d\n", get_prio_order(1));
    pr_info("get_prio_order(2): %d\n", get_prio_order(2));
    pr_info("get_prio_order(3): %d\n", get_prio_order(3));

    /**********************************************************************/

    for (test_round = 1; test_round <= 1; test_round++)
    {
        pr_info("\ntest_round: %d\n", test_round);
        test_sub();
    }

    return test_flag != 0 ? -EPERM : 0;
}
NVME_CASE_SYMBOL(case_command_arbitration, "?");
NVME_AUTOCASE_SYMBOL(case_command_arbitration);

