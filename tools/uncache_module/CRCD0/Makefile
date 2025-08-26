# ──────────────── Makefile ────────────────
# 把 cd_disable.c 编译成 cd_disable.ko
obj-m := cache_disable_all.o

# 让 “make” 直接调用内核构建系统
all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
# ───────────────────────────────────────────
