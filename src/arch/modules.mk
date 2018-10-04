
ifeq ($(PLATFORM),"hawknest")
include src/arch/hawknest/modules.mk
else
include src/arch/x64-linux/modules.mk
endif
