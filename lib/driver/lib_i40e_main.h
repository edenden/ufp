#ifndef _I40E_MAIN_H__
#define _I40E_MAIN_H__

struct switch_elem {
	uint8_t type;
#define I40E_AQ_SW_ELEM_TYPE_MAC        1
#define I40E_AQ_SW_ELEM_TYPE_PF         2
#define I40E_AQ_SW_ELEM_TYPE_VF         3
#define I40E_AQ_SW_ELEM_TYPE_EMP        4
#define I40E_AQ_SW_ELEM_TYPE_BMC        5
#define I40E_AQ_SW_ELEM_TYPE_PV         16
#define I40E_AQ_SW_ELEM_TYPE_VEB        17
#define I40E_AQ_SW_ELEM_TYPE_PA         18
#define I40E_AQ_SW_ELEM_TYPE_VSI        19
	uint16_t seid;
	uint16_t seid_uplink;
	uint16_t seid_downlink;
};

#endif /* _I40E_MAIN_H__ */
