

void ufp_fm10k_configure_irq(struct ufp_handle *ih, uint32_t rate)
{
        unsigned int qmask = 0;
	uint32_t itr;
        int queue_idx;

	/* Enable auto-mask and clear the current mask */
	itr = FM10K_ITR_ENABLE;

	/* Store Tx itr in timer slot 0 */
	itr |= (rate & FM10K_ITR_MAX);

	/* Shift Rx itr to timer slot 1 */
	itr |= (rate & FM10K_ITR_MAX) << FM10K_ITR_INTERVAL1_SHIFT;

        for(queue_idx = 0; queue_idx < ih->num_qps; queue_idx++){
		/* Enable q_vector */
		ufp_write_reg(FM10K_ITR(queue_idx), itr);
	}

        return 0;
}


