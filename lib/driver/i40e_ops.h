#ifndef _I40E_OPS_H__
#define _I40E_OPS_H__

int i40e_ops_init(struct ufp_dev *dev, struct ufp_ops *ops);
void i40e_ops_destroy(struct ufp_dev *dev, struct ufp_ops *ops);

#endif /* _I40E_OPS_H__ */
