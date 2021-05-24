
# Makefile for the kernel CAN serial device drivers.
#
# Advantech eAutomation Division
#


UNAME := $(shell uname -r)
PWD := $(shell pwd)

ifneq ($(KERNELRELEASE),)
EXTRA_CFLAGS +=  -std=gnu99
obj-m := advsocketcan.o advcan_sja1000.o


else
	KDIR := /lib/modules/4.15.0-041500-generic/build/
	PWD  := $(shell pwd)
all:	
	$(MAKE) -w -C $(KDIR) SUBDIRS=$(PWD) modules 
	gcc readCAN.c -o readCAN
	gcc sendCAN.c -o sendCAN
endif


install:
	@echo "Intalling the advantech SocketCAN driver..."
	@modprobe can 2>/dev/null 
	@modprobe can_dev 2>/dev/null 
	@modprobe can_raw 2>/dev/null 
	$(shell ! [ -d /lib/modules/$(shell uname -r)/kernel/drivers/advsocketcan ] && mkdir /lib/modules/$(shell uname -r)/kernel/drivers/advsocketcan)
	$(shell [ -f advcan_sja1000.ko ]  && cp ./advcan_sja1000.ko /lib/modules/$(shell uname -r)/kernel/drivers/advsocketcan/ && depmod && modprobe advcan_sja1000)
	$(shell [ -f advsocketcan.ko ]  && cp ./advsocketcan.ko /lib/modules/$(shell uname -r)/kernel/drivers/advsocketcan/ && depmod && modprobe advsocketcan)
	@echo "Done"
uninstall:
	@echo "Uninstall the advantech SocketCAN driver..."
	$(shell if grep advsocketcan /proc/modules > /dev/null ; then \
	 rmmod advsocketcan; fi)
	$(shell if grep advcan_sja1000 /proc/modules > /dev/null ; then \
	 rmmod advcan_sja1000; fi)
	$(shell if grep can_raw /proc/modules > /dev/null ; then \
	 rmmod can_raw; fi)
	$(shell if grep can_dev /proc/modules > /dev/null ; then \
	 rmmod can_dev; fi)
	$(shell if grep can /proc/modules > /dev/null ; then \
	 rmmod can; fi)
	@echo "Done"
clean:
	-@rm -f tags
	-@rm -f *.o *.ko
	-@rm -f .*.cmd *.mod.c
	-@rm -rf .tmp_versions
	-@rm -rf *~
	-@rm -rf *odule*
	-@rm -f readCAN
	-@rm -f sendCAN
	



