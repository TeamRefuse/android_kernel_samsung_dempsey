ifeq ($(CONFIG_MACH_S5PC110_ARIES),y)
   zreladdr-y	:= 0x30008000
params_phys-y	:= 0x30000100
else ifeq ($(CONFIG_MACH_S5PC110_P1),y)
   zreladdr-y	:= 0x30008000
params_phys-y	:= 0x30000100
else
   zreladdr-y	:= 0x20008000
params_phys-y	:= 0x20000100
endif
