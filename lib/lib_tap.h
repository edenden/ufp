#ifndef _UFPD_TUN_H
#define _UFPD_TUN_H

int tun_open(struct ufp_dev *dev, struct ufp_iface *iface);
void tun_close(struct ufp_dev *dev, struct ufp_iface *iface);

#endif /* _UFPD_TUN_H */
