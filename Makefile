ifneq ($(KERNELRELEASE),)
obj-m := nunchuk.o
else
#KDIR := ../../../../src/linux
all:
	$(MAKE) -C $(KDIR) M=$$PWD
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
endif

install:
	cp ./nunchuk.ko $(rpi_output)/br_shadow/target/root
	@echo "./nunchuk.ko is installed to $(rpi_output)/br_shadow/target/root"
