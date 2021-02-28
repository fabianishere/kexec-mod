.PHONY: all module clean prepare

all: module

module:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

prepare:
	make -C $(KDIR) modules_prepare
