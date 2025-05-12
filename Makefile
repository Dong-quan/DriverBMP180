obj-m += bmp180_driver.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)


DTS_FILE := bmp180
DTB_OVERLAY := $(DTS_FILE).dtbo

all: $(DTB_OVERLAY)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(DTB_OVERLAY): $(DTS_FILE).dts
	dtc -@ -I dts -O dtb -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(DTB_OVERLAY)
	sudo rmmod bmp180_driver.ko
