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

#include "lib_i40e.h"
#include "lib_i40e_main.h"

i40e_status i40e_pf_reset(struct i40e_hw *hw)
{
        u32 cnt = 0;
        u32 cnt1 = 0;
        u32 reg = 0;
        u32 grst_del;

        /* Poll for Global Reset steady state in case of recent GRST.
         * The grst delay value is in 100ms units, and we'll wait a
         * couple counts longer to be sure we don't just miss the end.
         */
        grst_del = (rd32(hw, I40E_GLGEN_RSTCTL) &
                        I40E_GLGEN_RSTCTL_GRSTDEL_MASK) >>
                        I40E_GLGEN_RSTCTL_GRSTDEL_SHIFT;

        grst_del = grst_del * 20;

        for (cnt = 0; cnt < grst_del; cnt++) {
                reg = rd32(hw, I40E_GLGEN_RSTAT);
                if (!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
                        break;
                msleep(100);
        }
        if (reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK) {
                hw_dbg(hw, "Global reset polling failed to complete.\n");
                return I40E_ERR_RESET_FAILED;
        }

        /* Now Wait for the FW to be ready */
        for (cnt1 = 0; cnt1 < I40E_PF_RESET_WAIT_COUNT; cnt1++) {
                reg = rd32(hw, I40E_GLNVM_ULD);
                reg &= (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
                        I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK);
                if (reg == (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
                            I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK)) {
                        hw_dbg(hw, "Core and Global modules ready %d\n", cnt1);
                        break;
                }
                usleep_range(10000, 20000);
        }
        if (!(reg & (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
                     I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))) {
                hw_dbg(hw, "wait for FW Reset complete timedout\n");
                hw_dbg(hw, "I40E_GLNVM_ULD = 0x%x\n", reg);
                return I40E_ERR_RESET_FAILED;
        }

        /* If there was a Global Reset in progress when we got here,
         * we don't need to do the PF Reset
         */
        if (!cnt) {
                reg = rd32(hw, I40E_PFGEN_CTRL);
                wr32(hw, I40E_PFGEN_CTRL,
                     (reg | I40E_PFGEN_CTRL_PFSWR_MASK));
                for (cnt = 0; cnt < I40E_PF_RESET_WAIT_COUNT; cnt++) {
                        reg = rd32(hw, I40E_PFGEN_CTRL);
                        if (!(reg & I40E_PFGEN_CTRL_PFSWR_MASK))
                                break;
                        usleep_range(1000, 2000);
                }
                if (reg & I40E_PFGEN_CTRL_PFSWR_MASK) {
                        hw_dbg(hw, "PF reset polling failed to complete.\n");
                        return I40E_ERR_RESET_FAILED;
                }
        }

        i40e_clear_pxe_mode(hw);

        return I40E_SUCCESS;
}

void i40e_clear_hw(struct i40e_hw *hw)
{
	u32 num_queues, base_queue;
	u32 num_pf_int;
	u32 num_vf_int;
	u32 num_vfs;
	u32 i, j;
	u32 val;
	u32 eol = 0x7ff;

	/* get number of interrupts, queues, and vfs */
	val = rd32(hw, I40E_GLPCI_CNF2);
	num_pf_int = (val & I40E_GLPCI_CNF2_MSI_X_PF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_PF_N_SHIFT;
	num_vf_int = (val & I40E_GLPCI_CNF2_MSI_X_VF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_VF_N_SHIFT;

	val = rd32(hw, I40E_PFLAN_QALLOC);
	base_queue = (val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
			I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	j = (val & I40E_PFLAN_QALLOC_LASTQ_MASK) >>
			I40E_PFLAN_QALLOC_LASTQ_SHIFT;
	if (val & I40E_PFLAN_QALLOC_VALID_MASK)
		num_queues = (j - base_queue) + 1;
	else
		num_queues = 0;

	val = rd32(hw, I40E_PF_VT_PFALLOC);
	i = (val & I40E_PF_VT_PFALLOC_FIRSTVF_MASK) >>
			I40E_PF_VT_PFALLOC_FIRSTVF_SHIFT;
	j = (val & I40E_PF_VT_PFALLOC_LASTVF_MASK) >>
			I40E_PF_VT_PFALLOC_LASTVF_SHIFT;
	if (val & I40E_PF_VT_PFALLOC_VALID_MASK)
		num_vfs = (j - i) + 1;
	else
		num_vfs = 0;

	/* stop all the interrupts */
	wr32(hw, I40E_PFINT_ICR0_ENA, 0);
	val = 0x3 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	for (i = 0; i < num_pf_int - 2; i++)
		wr32(hw, I40E_PFINT_DYN_CTLN(i), val);

	/* Set the FIRSTQ_INDX field to 0x7FF in PFINT_LNKLSTx */
	val = eol << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	wr32(hw, I40E_PFINT_LNKLST0, val);
	for (i = 0; i < num_pf_int - 2; i++)
		wr32(hw, I40E_PFINT_LNKLSTN(i), val);
	val = eol << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	for (i = 0; i < num_vfs; i++)
		wr32(hw, I40E_VPINT_LNKLST0(i), val);
	for (i = 0; i < num_vf_int - 2; i++)
		wr32(hw, I40E_VPINT_LNKLSTN(i), val);

	/* warn the HW of the coming Tx disables */
	for (i = 0; i < num_queues; i++) {
		u32 abs_queue_idx = base_queue + i;
		u32 reg_block = 0;

		if (abs_queue_idx >= 128) {
			reg_block = abs_queue_idx / 128;
			abs_queue_idx %= 128;
		}

		val = rd32(hw, I40E_GLLAN_TXPRE_QDIS(reg_block));
		val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
		val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
		val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

		wr32(hw, I40E_GLLAN_TXPRE_QDIS(reg_block), val);
	}
	udelay(400);

	/* stop all the queues */
	for (i = 0; i < num_queues; i++) {
		wr32(hw, I40E_QINT_TQCTL(i), 0);
		wr32(hw, I40E_QTX_ENA(i), 0);
		wr32(hw, I40E_QINT_RQCTL(i), 0);
		wr32(hw, I40E_QRX_ENA(i), 0);
	}

	/* short wait for all queue disables to settle */
	udelay(50);
}

