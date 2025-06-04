obj-m += led_driver.o

# Wenn KERNEL_SRC nicht gesetzt ist → lokal bauen
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
