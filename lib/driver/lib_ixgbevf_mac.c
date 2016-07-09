static int ufp_ixgbevf_negotiate_api(struct ufp_handle *ih)
{
        int32_t err, i = 0;
        uint32_t msg[3];
        struct ufp_ixgbevf_data *data;

        data = ih->ops.data;

        enum ufp_mbx_api_rev api[] = {
                ixgbe_mbox_api_12,
                ixgbe_mbox_api_11,
                ixgbe_mbox_api_10,
                ixgbe_mbox_api_unknown
        };

        while (api[i] != ixgbe_mbox_api_unknown) {

                /* Negotiate the mailbox API version */
                msg[0] = IXGBE_VF_API_NEGOTIATE;
                msg[1] = api[i];
                msg[2] = 0;

                err = mbx->ops.write_posted(hw, msg, 3);
                if(err) 
                        goto err_write;

                err = mbx->ops.read_posted(hw, msg, 3);
                if(err) 
                        goto err_read;

                msg[0] &= ~IXGBE_VT_MSGTYPE_CTS;

                if (msg[0] == (IXGBE_VF_API_NEGOTIATE | IXGBE_VT_MSGTYPE_ACK))
                        break;

                i++;
        }

        data->api_version = api[i];
        return 0;

err_read:
err_write:
        return -1;
}

static void ufp_mac_stop_adapter(struct ufp_handle *ih)
{
        uint32_t reg_val;
        uint16_t i;

	/* Clear interrupt mask to stop from interrupts being generated */
	ufp_write_reg(ih, IXGBE_VTEIMC, IXGBE_VF_IRQ_CLEAR_MASK);

	/* Clear any pending interrupts, flush previous writes */
	ufp_read_reg(ih, IXGBE_VTEICR);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < ih->num_queues; i++)
		ufp_write_reg(ih, IXGBE_VFTXDCTL(i), IXGBE_TXDCTL_SWFLSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < ih->num_queues; i++) {
		reg_val = ufp_read_reg(ih, IXGBE_VFRXDCTL(i));
		reg_val &= ~IXGBE_RXDCTL_ENABLE;
		ufp_write_reg(ih, IXGBE_VFRXDCTL(i), reg_val);
	}
	/* Clear packet split and pool config */
	ufp_write_reg(ih, IXGBE_VFPSRTYPE, 0);

	/* flush all queues disables */
	ufp_write_flush(ih);
	msleep(2);

	return 0;
}

static void ufp_ixgbevf_clr_reg(struct ufp_handle *ih)
{
	int i;
	uint32_t vfsrrctl;
	uint32_t ufpca_rxctrl;
	uint32_t ufpca_txctrl;

	/* VRSRRCTL default values (BSIZEPACKET = 2048, BSIZEHEADER = 256) */
	vfsrrctl = 0x100 << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT;
	vfsrrctl |= 0x800 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;

	/* DCA_RXCTRL default value */
	ufpca_rxctrl = IXGBE_DCA_RXCTRL_DESC_RRO_EN |
		       IXGBE_DCA_RXCTRL_DATA_WRO_EN |
		       IXGBE_DCA_RXCTRL_HEAD_WRO_EN;

	/* DCA_TXCTRL default value */
	ufpca_txctrl = IXGBE_DCA_TXCTRL_DESC_RRO_EN |
		       IXGBE_DCA_TXCTRL_DESC_WRO_EN |
		       IXGBE_DCA_TXCTRL_DATA_RRO_EN;

	ufp_write_reg(ih, IXGBE_VFPSRTYPE, 0);

	for (i = 0; i < 7; i++) {
		ufp_write_reg(ih, IXGBE_VFRDH(i), 0);
		ufp_write_reg(ih, IXGBE_VFRDT(i), 0);
		ufp_write_reg(ih, IXGBE_VFRXDCTL(i), 0);
		ufp_write_reg(ih, IXGBE_VFSRRCTL(i), vfsrrctl);
		ufp_write_reg(ih, IXGBE_VFTDH(i), 0);
		ufp_write_reg(ih, IXGBE_VFTDT(i), 0);
		ufp_write_reg(ih, IXGBE_VFTXDCTL(i), 0);
		ufp_write_reg(ih, IXGBE_VFTDWBAH(i), 0);
		ufp_write_reg(ih, IXGBE_VFTDWBAL(i), 0);
		ufp_write_reg(ih, IXGBE_VFDCA_RXCTRL(i), ufpca_rxctrl);
		ufp_write_reg(ih, IXGBE_VFDCA_TXCTRL(i), ufpca_txctrl);
	}

	ufp_write_flush(ih);
}

static void ufp_ixgbevf_set_eitr(struct ufp_handle *ih, int vector)
{
	uint32_t itr_reg;

	itr_reg = ih->num_interrupt_rate & IXGBE_MAX_EITR;

	/*
	 * set the WDIS bit to not clear the timer bits and cause an
	 * immediate assertion of the interrupt
	 */
	itr_reg |= IXGBE_EITR_CNT_WDIS;
	ufp_write_reg(ih, IXGBE_VTEITR(vector), itr_reg);
}

static void ufp_ixgbevf_set_ivar(struct ufp_handle *ih,
	int8_t direction, uint8_t queue, uint8_t msix_vector)
{
	uint32_t ivar, index;
	struct ufp_hw *hw = port->hw;

	/* tx or rx causes */
	msix_vector |= IXGBE_IVAR_ALLOC_VAL;
	index = ((16 * (queue & 1)) + (8 * direction));
	ivar = ufp_read_reg(hw, IXGBE_VTIVAR(queue >> 1));
	ivar &= ~(0xFF << index);
	ivar |= (msix_vector << index);
	ufp_write_reg(ih, IXGBE_VTIVAR(queue >> 1), ivar);
}

