#ifndef _I40E_MAIN_H__
#define _I40E_MAIN_H__

#include "i40e_aq.h"
#include "i40e_hmc.h"

#define I40E_MAX_NUM_DESCRIPTORS	4096
#define I40E_PF_RESET_WAIT_COUNT	200
#define I40E_MAX_TRAFFIC_CLASS		8
#define I40E_DRV_VERSION_MAJOR		1
#define I40E_DRV_VERSION_MINOR		5
#define I40E_DRV_VERSION_BUILD		16
#define I40E_NUM_MISC_IRQS		1

/* Filter context base size is 1K */
#define I40E_HASH_FILTER_BASE_SIZE	1024
/* Supported Hash filter values */
enum i40e_hash_filter_size {
	I40E_HASH_FILTER_SIZE_1K	= 0,
	I40E_HASH_FILTER_SIZE_2K	= 1,
	I40E_HASH_FILTER_SIZE_4K	= 2,
	I40E_HASH_FILTER_SIZE_8K	= 3,
	I40E_HASH_FILTER_SIZE_16K	= 4,
	I40E_HASH_FILTER_SIZE_32K	= 5,
	I40E_HASH_FILTER_SIZE_64K	= 6,
	I40E_HASH_FILTER_SIZE_128K	= 7,
	I40E_HASH_FILTER_SIZE_256K	= 8,
	I40E_HASH_FILTER_SIZE_512K	= 9,
	I40E_HASH_FILTER_SIZE_1M	= 10,
};

/* DMA context base size is 0.5K */
#define I40E_DMA_CNTX_BASE_SIZE		512
/* Supported DMA context values */
enum i40e_dma_cntx_size {
	I40E_DMA_CNTX_SIZE_512		= 0,
	I40E_DMA_CNTX_SIZE_1K		= 1,
	I40E_DMA_CNTX_SIZE_2K		= 2,
	I40E_DMA_CNTX_SIZE_4K		= 3,
	I40E_DMA_CNTX_SIZE_8K		= 4,
	I40E_DMA_CNTX_SIZE_16K		= 5,
	I40E_DMA_CNTX_SIZE_32K		= 6,
	I40E_DMA_CNTX_SIZE_64K		= 7,
	I40E_DMA_CNTX_SIZE_128K		= 8,
	I40E_DMA_CNTX_SIZE_256K		= 9,
};

/* Supported Hash look up table (LUT) sizes */
enum i40e_hash_lut_size {
	I40E_HASH_LUT_SIZE_128		= 0,
	I40E_HASH_LUT_SIZE_512		= 1,
};

/* Packet Classifier Types for filters */
enum i40e_filter_pctype {
	/* Note: Values 0-28 are reserved for future use.
	 * Value 29, 30, 32 are not supported on XL710 and X710.
	 */
	I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP	= 29,
	I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP	= 30,
	I40E_FILTER_PCTYPE_NONF_IPV4_UDP		= 31,
	I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK	= 32,
	I40E_FILTER_PCTYPE_NONF_IPV4_TCP		= 33,
	I40E_FILTER_PCTYPE_NONF_IPV4_SCTP		= 34,
	I40E_FILTER_PCTYPE_NONF_IPV4_OTHER		= 35,
	I40E_FILTER_PCTYPE_FRAG_IPV4			= 36,
	/* Note: Values 37-38 are reserved for future use.
	 * Value 39, 40, 42 are not supported on XL710 and X710.
	 */
	I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP	= 39,
	I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP	= 40,
	I40E_FILTER_PCTYPE_NONF_IPV6_UDP		= 41,
	I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK	= 42,
	I40E_FILTER_PCTYPE_NONF_IPV6_TCP		= 43,
	I40E_FILTER_PCTYPE_NONF_IPV6_SCTP		= 44,
	I40E_FILTER_PCTYPE_NONF_IPV6_OTHER		= 45,
	I40E_FILTER_PCTYPE_FRAG_IPV6			= 46,
	/* Note: Value 47 is reserved for future use */
	I40E_FILTER_PCTYPE_FCOE_OX			= 48,
	I40E_FILTER_PCTYPE_FCOE_RX			= 49,
	I40E_FILTER_PCTYPE_FCOE_OTHER			= 50,
	/* Note: Values 51-62 are reserved for future use */
	I40E_FILTER_PCTYPE_L2_PAYLOAD			= 63,
};

/* RSS Hash Table Size */
#define I40E_PFQF_CTL_0_HASHLUTSIZE_512	0x00010000

#define I40E_PFQF_HKEY_MAX_INDEX 12
#define I40E_PFQF_HLUT_MAX_INDEX 127
#define I40E_HKEY_ARRAY_SIZE ((I40E_PFQF_HKEY_MAX_INDEX + 1) * 4)
#define I40E_HLUT_ARRAY_SIZE ((I40E_PFQF_HLUT_MAX_INDEX + 1) * 4)

struct i40e_elem {
	uint8_t			type;
	uint16_t		seid;
	uint16_t		seid_uplink;
	uint16_t		seid_downlink;
	uint16_t		element_info;
	struct list_node	list;
};

struct i40e_version {
	uint16_t		fw_major;
	uint16_t		fw_minor;
	uint32_t		fw_build;
	uint16_t		api_major;
	uint16_t		api_minor;
};

struct i40e_dev {
	uint8_t			pf_id;
	uint8_t			pf_lan_mac[6];
	struct i40e_aq		aq;
	struct i40e_hmc		hmc;
	struct i40e_version	version;
	struct list_head	elem;
};

enum i40e_vsi_type {
	I40E_VSI_MAIN	= 0,
	I40E_VSI_VMDQ1	= 1,
	I40E_VSI_VMDQ2	= 2,
	I40E_VSI_CTRL	= 3,
	I40E_VSI_FCOE	= 4,
	I40E_VSI_MIRROR	= 5,
	I40E_VSI_SRIOV	= 6,
	I40E_VSI_FDIR	= 7,
	I40E_VSI_TYPE_UNKNOWN
};

struct i40e_iface {
	enum i40e_vsi_type	type;
	uint16_t		seid;
	uint16_t		id;
	uint16_t		base_qp;
	uint16_t		qs_handles[8];
};

#define i40e_flush(dev) \
	UFP_READ32((dev), I40E_GLGEN_STAT)

int i40e_open(struct ufp_dev *dev);
void i40e_close(struct ufp_dev *dev);
int i40e_up(struct ufp_dev *dev, struct ufp_iface *iface);
int i40e_down(struct ufp_dev *dev, struct ufp_iface *iface);
void i40e_setup_misc_irq(struct ufp_dev *dev);
void i40e_shutdown_misc_irq(struct ufp_dev *dev);
void i40e_start_misc_irq(struct ufp_dev *dev);
void i40e_stop_misc_irq(struct ufp_dev *dev);

#endif /* _I40E_MAIN_H__ */
