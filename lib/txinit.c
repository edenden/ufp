#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <net/ethernet.h>
#include <time.h>

#include "ixmap.h"
#include "txinit.h"

static void ixmap_setup_mtqc(struct ixmap_handle *ih);
static void ixmap_configure_tx_ring(struct ixmap_handle *ih,
	uint8_t reg_idx, struct ixmap_ring *ring);

void ixmap_configure_tx(struct ixmap_handle *ih)
{
	uint32_t dmatxctl;
	int i;

	ixmap_setup_mtqc(ih);

	/* DMATXCTL.EN must be before Tx queues are enabled */
	dmatxctl = ixmap_read_reg(ih, IXGBE_DMATXCTL);
	dmatxctl |= IXGBE_DMATXCTL_TE;
	ixmap_write_reg(ih, IXGBE_DMATXCTL, dmatxctl);

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < ih->num_queues; i++)
		ixmap_configure_tx_ring(ih, i, &ih->tx_ring[i]);
	return;
}

static void ixmap_setup_mtqc(struct ixmap_handle *ih)
{
	uint32_t rttdcs, mtqc;

	/* disable the arbiter while setting MTQC */
	rttdcs = ixmap_read_reg(ih, IXGBE_RTTDCS);
	rttdcs |= IXGBE_RTTDCS_ARBDIS;
	ixmap_write_reg(ih, IXGBE_RTTDCS, rttdcs);

	/*
	 * set transmit pool layout:
	 * Though we don't support traffic class(TC)
	 */
	mtqc = IXGBE_MTQC_64Q_1PB;
	ixmap_write_reg(ih, IXGBE_MTQC, mtqc);

	/* re-enable the arbiter */
	rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
	ixmap_write_reg(ih, IXGBE_RTTDCS, rttdcs);
	return;
}

static void ixmap_configure_tx_ring(struct ixmap_handle *ih,
	uint8_t reg_idx, struct ixmap_ring *ring)
{
	int wait_loop = 10;
	uint32_t txdctl = IXGBE_TXDCTL_ENABLE;
	uint64_t addr_dma;
	struct timespec ts;

	addr_dma = (uint64_t)ring->addr_dma;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;

	/* disable queue to avoid issues while updating state */
	ixmap_write_reg(ih, IXGBE_TXDCTL(reg_idx), IXGBE_TXDCTL_SWFLSH);
	ixmap_write_flush(ih);

	ixmap_write_reg(ih, IXGBE_TDBAL(reg_idx),
			addr_dma & DMA_BIT_MASK(32));
	ixmap_write_reg(ih, IXGBE_TDBAH(reg_idx),
			addr_dma >> 32);
	ixmap_write_reg(ih, IXGBE_TDLEN(reg_idx),
			ih->num_tx_desc * sizeof(union ixmap_adv_tx_desc));

	/* disable head writeback */
	ixmap_write_reg(ih, IXGBE_TDWBAH(reg_idx), 0);
	ixmap_write_reg(ih, IXGBE_TDWBAL(reg_idx), 0);

	/* reset head and tail pointers */
	ixmap_write_reg(ih, IXGBE_TDH(reg_idx), 0);
	ixmap_write_reg(ih, IXGBE_TDT(reg_idx), 0);

	ring->tail = ih->bar + IXGBE_TDT(reg_idx);

	/*
	 * set WTHRESH to encourage burst writeback, it should not be set
	 * higher than 1 when ITR is 0 as it could cause false TX hangs.
	 *
	 * In order to avoid issues WTHRESH + PTHRESH should always be equal
	 * to or less than the number of on chip descriptors, which is
	 * currently 40.
	 */
	if(ih->num_interrupt_rate < 8)
		txdctl |= (1 << 16);    /* WTHRESH = 1 */
	else
		txdctl |= (8 << 16);    /* WTHRESH = 8 */

	/*
	 * Setting PTHRESH to 32 both improves performance
	 * and avoids a TX hang with DFP enabled
	 */
	txdctl |=	(1 << 8) |	/* HTHRESH = 1 */
			32;		/* PTHRESH = 32 */

	/* enable queue */
	ixmap_write_reg(ih, IXGBE_TXDCTL(reg_idx), txdctl);

	/* poll to verify queue is enabled */
	do {
		nanosleep(&ts, NULL);
		txdctl = ixmap_read_reg(ih, IXGBE_TXDCTL(reg_idx));
	} while (--wait_loop && !(txdctl & IXGBE_TXDCTL_ENABLE));
	if (!wait_loop)
		printf("Could not enable Tx Queue %d\n", reg_idx);
	return;
}
