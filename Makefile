CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv_new

obj-m += $(TARGET_MODULE).o
fibdrv_new-objs := \
	fibdrv.o \
	bn_kernel.o \
	bignum.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

all: $(GIT_HOOKS) client
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) client out *.txt *.png
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

client: client.c
	$(CC) -o $@ $^

PRINTF = env printf
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"

plot: all
	$(MAKE) unload
	$(MAKE) load
	@python3 scripts/driver.py
	$(MAKE) unload

check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
	@diff -u out scripts/expected.txt && $(call pass)
	@scripts/verify.py
