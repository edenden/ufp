#ifndef _UFP_TUN_H
#define _UFP_TUN_H

int ufp_tun_open(struct ufp_iface *iface);
void ufp_tun_close(struct ufp_iface *iface);
int ufp_tun_up(struct ufp_iface *iface);
void ufp_tun_down(struct ufp_iface *iface);

#endif /* _UFP_TUN_H */
