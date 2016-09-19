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

int i40e_reset_hw(struct ufp_handle *ih)
{
	struct timespec ts;
	int err;
	uint32_t cnt = 0;
	uint32_t cnt1 = 0;
	uint32_t reg = 0;
	uint32_t grst_del;

	/* Poll for Global Reset steady state in case of recent GRST.
	 * The grst delay value is in 100ms units, and we'll wait a
	 * couple counts longer to be sure we don't just miss the end.
	 */
	grst_del = (ufp_read_reg(ih, I40E_GLGEN_RSTCTL) &
			I40E_GLGEN_RSTCTL_GRSTDEL_MASK) >>
			I40E_GLGEN_RSTCTL_GRSTDEL_SHIFT;

	grst_del = grst_del * 20;

	for (cnt = 0; cnt < grst_del; cnt++) {
		reg = ufp_read_reg(ih, I40E_GLGEN_RSTAT);
		if (!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
			break;
		msleep(&ts, 100);
	}
	if (reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK) {
		return I40E_ERR_RESET_FAILED;
	}

	/* Now Wait for the FW to be ready */
	for (cnt1 = 0; cnt1 < I40E_PF_RESET_WAIT_COUNT; cnt1++) {
		reg = ufp_read_reg(ih, I40E_GLNVM_ULD);
		reg &= (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
			I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK);
		if (reg == (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
			    I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK)) {
			break;
		}
		usleep(&ts, 10000);
	}
	if (!(reg & (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
		     I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))) {
		return I40E_ERR_RESET_FAILED;
	}

	/* If there was a Global Reset in progress when we got here,
	 * we don't need to do the PF Reset
	 */
	if (!cnt) {
		reg = ufp_read_reg(ih, I40E_PFGEN_CTRL);
		ufp_write_reg(ih, I40E_PFGEN_CTRL,
		     (reg | I40E_PFGEN_CTRL_PFSWR_MASK));
		for (cnt = 0; cnt < I40E_PF_RESET_WAIT_COUNT; cnt++) {
			reg = ufp_read_reg(ih, I40E_PFGEN_CTRL);
			if (!(reg & I40E_PFGEN_CTRL_PFSWR_MASK))
				break;
			usleep_range(1000, 2000);
		}
		if (reg & I40E_PFGEN_CTRL_PFSWR_MASK) {
			return I40E_ERR_RESET_FAILED;
		}
	}

	return 0;
}

void i40e_clear_hw(struct ufp_handle *ih)
{
	struct timespec ts;
	uint32_t num_queues, base_queue;
	uint32_t num_pf_int;
	uint32_t num_vf_int;
	uint32_t num_vfs;
	uint32_t i, j;
	uint32_t val;
	uint32_t eol = 0x7ff;

	/* get number of interrupts, queues, and vfs */
	val = ufp_read_reg(ih, I40E_GLPCI_CNF2);
	num_pf_int = (val & I40E_GLPCI_CNF2_MSI_X_PF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_PF_N_SHIFT;
	num_vf_int = (val & I40E_GLPCI_CNF2_MSI_X_VF_N_MASK) >>
			I40E_GLPCI_CNF2_MSI_X_VF_N_SHIFT;

	val = ufp_read_reg(ih, I40E_PFLAN_QALLOC);
	base_queue = (val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
			I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	j = (val & I40E_PFLAN_QALLOC_LASTQ_MASK) >>
			I40E_PFLAN_QALLOC_LASTQ_SHIFT;
	if (val & I40E_PFLAN_QALLOC_VALID_MASK)
		num_queues = (j - base_queue) + 1;
	else
		num_queues = 0;

	val = ufp_read_reg(ih, I40E_PF_VT_PFALLOC);
	i = (val & I40E_PF_VT_PFALLOC_FIRSTVF_MASK) >>
			I40E_PF_VT_PFALLOC_FIRSTVF_SHIFT;
	j = (val & I40E_PF_VT_PFALLOC_LASTVF_MASK) >>
			I40E_PF_VT_PFALLOC_LASTVF_SHIFT;
	if (val & I40E_PF_VT_PFALLOC_VALID_MASK)
		num_vfs = (j - i) + 1;
	else
		num_vfs = 0;

	/* stop all the interrupts */
	ufp_write_reg(ih, I40E_PFINT_ICR0_ENA, 0);
	val = 0x3 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	for (i = 0; i < num_pf_int - 2; i++)
		ufp_write_reg(ih, I40E_PFINT_DYN_CTLN(i), val);

	/* Set the FIRSTQ_INDX field to 0x7FF in PFINT_LNKLSTx */
	val = eol << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	ufp_write_reg(ih, I40E_PFINT_LNKLST0, val);
	for (i = 0; i < num_pf_int - 2; i++)
		ufp_write_reg(ih, I40E_PFINT_LNKLSTN(i), val);
	val = eol << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	for (i = 0; i < num_vfs; i++)
		ufp_write_reg(ih, I40E_VPINT_LNKLST0(i), val);
	for (i = 0; i < num_vf_int - 2; i++)
		ufp_write_reg(ih, I40E_VPINT_LNKLSTN(i), val);

	/* warn the HW of the coming Tx disables */
	for (i = 0; i < num_queues; i++) {
		uint32_t abs_queue_idx = base_queue + i;
		uint32_t reg_block = 0;

		if (abs_queue_idx >= 128) {
			reg_block = abs_queue_idx / 128;
			abs_queue_idx %= 128;
		}

		val = ufp_read_reg(ih, I40E_GLLAN_TXPRE_QDIS(reg_block));
		val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
		val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
		val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

		ufp_write_reg(ih, I40E_GLLAN_TXPRE_QDIS(reg_block), val);
	}
	usleep(&ts, 400);

	/* stop all the queues */
	for (i = 0; i < num_queues; i++) {
		ufp_write_reg(ih, I40E_QINT_TQCTL(i), 0);
		ufp_write_reg(ih, I40E_QTX_ENA(i), 0);
		ufp_write_reg(ih, I40E_QINT_RQCTL(i), 0);
		ufp_write_reg(ih, I40E_QRX_ENA(i), 0);
	}

	/* short wait for all queue disables to settle */
	usleep(&ts, 50);

	return;
}

void i40e_set_mac_type(struct ufp_handle *ih)
{
	struct ufp_i40e_data *data = ih->ops->data;

	switch(ih->ops->device_id){
	case I40E_DEV_ID_SFP_XL710:
	case I40E_DEV_ID_QEMU:
	case I40E_DEV_ID_KX_B:
	case I40E_DEV_ID_KX_C:
	case I40E_DEV_ID_QSFP_A:
	case I40E_DEV_ID_QSFP_B:
	case I40E_DEV_ID_QSFP_C:
	case I40E_DEV_ID_10G_BASE_T:
	case I40E_DEV_ID_10G_BASE_T4:
	case I40E_DEV_ID_20G_KR2:
	case I40E_DEV_ID_20G_KR2_A:
	case I40E_DEV_ID_25G_B:
	case I40E_DEV_ID_25G_SFP28:
		data->mac_type = I40E_MAC_XL710;
		break;
	case I40E_DEV_ID_KX_X722:
	case I40E_DEV_ID_QSFP_X722:
	case I40E_DEV_ID_SFP_X722:
	case I40E_DEV_ID_1G_BASE_T_X722:
	case I40E_DEV_ID_10G_BASE_T_X722:
	case I40E_DEV_ID_SFP_I_X722:
	case I40E_DEV_ID_QSFP_I_X722:
		data->mac_type = I40E_MAC_X722;
		break;
	default:
		data->mac_type = I40E_MAC_GENERIC;
		break;
	}

	return;
}

int i40e_diag_eeprom_test(struct i40e_hw *hw)
{
	int err;
	uint16_t reg_val;

	/* read NVM control word and if NVM valid, validate EEPROM checksum*/
	err = i40e_read_nvm_word(hw, I40E_SR_NVM_CONTROL_WORD, &reg_val);
	if ((err == I40E_SUCCESS) &&
	((reg_val & I40E_SR_CONTROL_WORD_1_MASK) == BIT(I40E_SR_CONTROL_WORD_1_SHIFT)))
		return i40e_validate_nvm_checksum(hw, NULL);
	else
		return I40E_ERR_DIAG_TEST_FAILED;
}

int i40e_aq_clear_pxe_mode(struct i40e_hw *hw,
			struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_clear_pxe *cmd =
		(struct i40e_aqc_clear_pxe *)&desc.params.raw;
	int err;

	i40e_fill_default_direct_cmd_desc(&desc,
		i40e_aqc_opc_clear_pxe_mode);

	cmd->rx_cnt = 0x2;

	err = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	wr32(hw, I40E_GLLAN_RCTL_0, 0x1);

	return err;
}

i40e_status i40e_aq_send_driver_version(struct i40e_hw *hw,
				struct i40e_driver_version *dv,
				struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_driver_version *cmd =
		(struct i40e_aqc_driver_version *)&desc.params.raw;
	i40e_status status;
	u16 len;

	if (dv == NULL)
		return I40E_ERR_PARAM;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_driver_version);

	desc.flags |= CPU_TO_LE16(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD);
	cmd->driver_major_ver = dv->major_version;
	cmd->driver_minor_ver = dv->minor_version;
	cmd->driver_build_ver = dv->build_version;
	cmd->driver_subbuild_ver = dv->subbuild_version;

	len = 0;
	while (len < sizeof(dv->driver_string) &&
	       (dv->driver_string[len] < 0x80) &&
	       dv->driver_string[len])
		len++;
	status = i40e_asq_send_command(hw, &desc, dv->driver_string,
				       len, cmd_details);

	return status;
}
