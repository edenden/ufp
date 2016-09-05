#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "lib_main.h"
#include "lib_rtx.h"

int ufp_fm10k_reset_hw(struct ufp_handle *ih)
{
	int err, i;
	uint32_t reg;

	/* Disable interrupts */
	ufp_write_reg(ih, FM10K_EIMR, FM10K_EIMR_DISABLE(ALL));

	/* Lock ITR2 reg 0 into itself and disable interrupt moderation */
	ufp_write_reg(ih, FM10K_ITR2(0), 0);
	ufp_write_reg(ih, FM10K_INT_CTRL, 0);

	/* We assume here Tx and Rx queue 0 are owned by the PF */

	/* Shut off VF access to their queues forcing them to queue 0 */
	for (i = 0; i < FM10K_TQMAP_TABLE_SIZE; i++) {
		ufp_write_reg(ih, FM10K_TQMAP(i), 0);
		ufp_write_reg(ih, FM10K_RQMAP(i), 0);
	}

	/* shut down all rings */
	err = ufp_fm10k_disable_queues(ih, FM10K_MAX_QUEUES);
	if(err < 0)
		goto err_disable_queues;

	/* Verify that DMA is no longer active */
	reg = ufp_read_reg(ih, FM10K_DMA_CTRL);
	if (reg & (FM10K_DMA_CTRL_TX_ACTIVE | FM10K_DMA_CTRL_RX_ACTIVE))
		return FM10K_ERR_DMA_PENDING;

	/* Inititate data path reset */
	reg = FM10K_DMA_CTRL_DATAPATH_RESET;
	ufp_write_reg(ih, FM10K_DMA_CTRL, reg);

	/* Flush write and allow 100us for reset to complete */
	fm10k_write_flush(ih);
	udelay(FM10K_RESET_TIMEOUT);

	/* Reset mailbox global interrupts */
	reg = FM10K_MBX_GLOBAL_REQ_INTERRUPT | FM10K_MBX_GLOBAL_ACK_INTERRUPT;
	ufp_write_reg(ih, FM10K_GMBX, reg);

	/* Verify we made it out of reset */
	reg = ufp_read_reg(ih, FM10K_IP);
	if (!(reg & FM10K_IP_NOTINRESET))
		return FM10K_ERR_RESET_FAILED;

	return 0;

err_disable_queues:
	return -1;
}

int ufp_fm10k_disable_queues(struct ufp_handle *ih, uint16_t q_cnt)
{
	int i, time;
	uint32_t reg;

	/* clear the enable bit for all rings */
	for (i = 0; i < q_cnt; i++) {
		reg = fm10k_read_reg(hw, FM10K_TXDCTL(i));
		fm10k_write_reg(hw, FM10K_TXDCTL(i),
				reg & ~FM10K_TXDCTL_ENABLE);
		reg = fm10k_read_reg(hw, FM10K_RXQCTL(i));
		fm10k_write_reg(hw, FM10K_RXQCTL(i),
				reg & ~FM10K_RXQCTL_ENABLE);
	}

	fm10k_write_flush(hw);
	udelay(1);

	/* loop through all queues to verify that they are all disabled */
	for (i = 0, time = FM10K_QUEUE_DISABLE_TIMEOUT; time;) {
		/* if we are at end of rings all rings are disabled */
		if (i == q_cnt)
			return 0;

		/* if queue enables cleared, then move to next ring pair */
		reg = fm10k_read_reg(hw, FM10K_TXDCTL(i));
		if (!~reg || !(reg & FM10K_TXDCTL_ENABLE)) {
			reg = fm10k_read_reg(hw, FM10K_RXQCTL(i));
			if (!~reg || !(reg & FM10K_RXQCTL_ENABLE)) {
				i++;
				continue;
			}
		}

		/* decrement time and wait 1 usec */
		time--;
		if (time)
			udelay(1);
	}

	return -1;
}

