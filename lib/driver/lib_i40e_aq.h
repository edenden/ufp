/* general information */
#define I40E_AQ_LARGE_BUF       512
#define I40E_ASQ_CMD_TIMEOUT    250  /* msecs */

struct i40e_aq_ring {
	struct ufp_i40e_page *desc; /* descriptor ring memory */
	struct ufp_i40e_page **bufs;

	u16 count;		/* Number of descriptors */
	u16 rx_buf_len;		/* Admin Receive Queue buffer length */

	/* used for interrupt processing */
	u16 next_to_use;
	u16 next_to_clean;

	/* used for queue tracking */
	u32 head;
	u32 tail;
	u32 len;
	u32 bah;
	u32 bal;

	uint16_t num_entries;
	uint16_t buf_size;
};

#endif /* _I40E_ADMINQ_H_ */
