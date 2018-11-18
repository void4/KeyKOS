	.global scsi_mname, v_eeprom_addr
! __ashldi3:     ta 0x41
scsi_mname:    ta 0x42

	.global start_mon_clock, omak_enteromak
start_mon_clock:   ta 0x44
! v_level10clk_addr: ta 0x45
omak_enteromak:    ta 0x46

	.global stop_mon_clock
stop_mon_clock:    retl; nop ! Worry about this!!
! scsi_cdb_size:     ta 0x49
! mmu_setctp:        ta 0x4a

	.global  ldphys, omak_default_breakpt, dat2inst
ldphys:       ta 0x4c
omak_default_breakpt: ta 0x4d
              retl
              nop
dat2inst:     ta 0x4e

	.global crash, mmu_flushall, bpt_reg
crash:     ta 0x52
mmu_flushall: ta 0x53
bpt_reg:      ta 0x55

	.global map_wellknown_devices, clkstart, omak_init
map_wellknown_devices: ta 0x56
! v_iommu_addr:          ta 0x57
clkstart:              ta 0x58
omak_init:             ta 0x59

	.global  Panic
Panic:  retl; ta 0x5e

	.global prom_panic
	.global prom_naked_enter
	.global prom_init
prom_panic:   ta 0x4f
prom_naked_enter:  ta 0x48
prom_init:    ta 0x50



