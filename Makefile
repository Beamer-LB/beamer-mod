# TODO: KDIR

KDIR		:= ../mptcp
PWD		:= $(shell pwd)

kbuild:
	make -C $(KDIR) M=$(PWD)

modules: kbuild
	make -C $(KDIR) M=$(PWD) modules

install:
	make -C $(KDIR) M=$(PWD) modules_install
	depmod -a

clean:
	make -C $(KDIR) M=$(PWD) clean
	-rm -f Module.symvers *~
