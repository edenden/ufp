# UFP
User-Space Forwarding Plane

UFP is a library to provide direct NIC access with VFIO.
It is similar to something like Intel DPDK or other kernel-bypass packet I/O
libraries but simplicity and fastness(in terms of processable pps per core),
and affinity with Linux kernel are more prioritized than others.

# Build and Install
UFP is tested under Debian jessie.
You should install some packages before build from source code.
Installing target is `/usr/local/` by default.

```
$ sudo apt-get install gcc make autoconf automake libtool
$ cd ufp
$ ./autogen.sh
$ ./configure && make && sudo make install
```

# Setup
It is assumed that applications on top of UFP use `1GB hugepages`
because it significantly improve performance.
Also, `IOMMU` must be enabled for VFIO.

```
$ sudo vi /etc/default/grub
GRUB_CMDLINE_LINUX="default_hugepagesz=1G hugepagesz=1G
hugepages=[Number of hugepages] intel_iommu=on"
$ sudo update-grub
$ sudo reboot
```

Load VFIO kernel module.
```
$ sudo modprobe vfio-pci
```

Bind NIC(s) to VFIO.
Note: PCI device is described as `0000:04:00.0`.
```
$ echo "[PCI device]" > /sys/bus/pci/devices/[PCI device]/driver/unbind
$ echo "vfio-pci" > /sys/bus/pci/devices/[PCI device]/driver_override
$ echo "[PCI device]" > /sys/bus/pci/drivers/vfio-pci/bind
```

(Optional) You can automatically bind target NIC(s) by appropriate udev script
and loading VFIO module before the script is executed.
```
$ sudo vi /etc/initramfs-tools/modules
vfio
vfio_iommu_type1
vfio-pci

$ sudo update-initramfs -u
```

```
$ sudo vi /etc/udev/rules.d/10-ufp.rules
KERNEL=="[PCI device]", SUBSYSTEMS=="pci", \
ATTR{driver_override}="vfio-pci", \
ATTR{driver/unbind}="$kernel", \
ATTR{subsystem/drivers_probe}="$kernel"
```

UFP automatically setup interrupt affinity,
therefore `irqbalanced` must be stopped.
```
$ sudo systemctl stop irqbalance
```
(Optional)
```
$ sudo systemctl disable irqbalance
```

# L3 forward app
UFP includes L3 forward app to measure and demonstrate its performance.
The app supports parallel processing of packets using threads.
Resources used by thread are completely isolated from others,
to get rid of any overhead.
For example, each thread is bound to designated cpu core,
and hold a queue pair in each NIC(s), and maintains its own FIB.

The app requires kernel forwarding is enabled,
because first packet which has no corresponding ARP/ND entry is
forwarded in kernel.
```
$ sudo vi /etc/sysctl.conf
net.ipv4.ip_forward=1
net.ipv6.conf.all.forwarding=1
$ sysctl -p
```

Run L3 forward app. You may choose CPU cores to bind.
The app allows both `1,2,3` and `1-3` as CPU description.
```
$ sudo ufp -c 1-4,8,11-13 -p 0000:04:00.0,0000:05:00.0
```

L3 forward app creates kernel side TAP interface(s) automatically.
You can configure IP addresses and routes through the interface(s).
These configurations are notified through `NETLINK` to this app.
```
$ sudo ip addr add 10.0.0.1/24 dev unp4s0f0
$ sudo ip addr add 10.0.1.1/24 dev unp5s0f0
```
