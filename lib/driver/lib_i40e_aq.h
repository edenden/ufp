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

/* ARQ event information */
struct i40e_arq_event_info {
	struct i40e_aq_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};

/**
 * i40e_aq_rc_to_posix - convert errors to user-land codes
 * aq_ret: AdminQ handler error code can override aq_rc
 * aq_rc: AdminQ firmware error code to convert
 **/
static INLINE int i40e_aq_rc_to_posix(int aq_ret, int aq_rc)
{
	int aq_to_posix[] = {
		0,           /* I40E_AQ_RC_OK */
		-EPERM,      /* I40E_AQ_RC_EPERM */
		-ENOENT,     /* I40E_AQ_RC_ENOENT */
		-ESRCH,      /* I40E_AQ_RC_ESRCH */
		-EINTR,      /* I40E_AQ_RC_EINTR */
		-EIO,        /* I40E_AQ_RC_EIO */
		-ENXIO,      /* I40E_AQ_RC_ENXIO */
		-E2BIG,      /* I40E_AQ_RC_E2BIG */
		-EAGAIN,     /* I40E_AQ_RC_EAGAIN */
		-ENOMEM,     /* I40E_AQ_RC_ENOMEM */
		-EACCES,     /* I40E_AQ_RC_EACCES */
		-EFAULT,     /* I40E_AQ_RC_EFAULT */
		-EBUSY,      /* I40E_AQ_RC_EBUSY */
		-EEXIST,     /* I40E_AQ_RC_EEXIST */
		-EINVAL,     /* I40E_AQ_RC_EINVAL */
		-ENOTTY,     /* I40E_AQ_RC_ENOTTY */
		-ENOSPC,     /* I40E_AQ_RC_ENOSPC */
		-ENOSYS,     /* I40E_AQ_RC_ENOSYS */
		-ERANGE,     /* I40E_AQ_RC_ERANGE */
		-EPIPE,      /* I40E_AQ_RC_EFLUSHED */
		-ESPIPE,     /* I40E_AQ_RC_BAD_ADDR */
		-EROFS,      /* I40E_AQ_RC_EMODE */
		-EFBIG,      /* I40E_AQ_RC_EFBIG */
	};

	/* aq_rc is invalid if AQ timed out */
	if (aq_ret == I40E_ERR_ADMIN_QUEUE_TIMEOUT)
		return -EAGAIN;

	if (!((u32)aq_rc < (sizeof(aq_to_posix) / sizeof((aq_to_posix)[0]))))
		return -ERANGE;

	return aq_to_posix[aq_rc];
}

/* general information */
#define I40E_AQ_LARGE_BUF	512
#define I40E_ASQ_CMD_TIMEOUT	250  /* msecs */

void i40e_fill_default_direct_cmd_desc(struct i40e_aq_desc *desc,
				       u16 opcode);

#endif /* _I40E_ADMINQ_H_ */
