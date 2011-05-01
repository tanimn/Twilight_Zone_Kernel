ccflags-y := \
	-Wno-pointer-sign\
	-fno-strict-overflow\
#	-fconserve-stack\
	-Wdeclaration-after-statement\
	-Wpointer-arith\
	-Wmissing-format-attribute\
	-Wno-format-zero-length\
	-Wstrict-prototypes\
	-Wno-unused-parameter\
	-Wno-sign-compare\
	-Werror\
	-fno-strict-aliasing\
	-Wno-pointer-arith\
	-Os

#	-Wmissing-prototypes\

ccflags-y += -DLINUX -D__linux__ -DANDROID -DSERVICES4
ccflags-y += -Idrivers/gpu/pvr \
	     -Idrivers/gpu/pvr/sgx

# options
#SUPPORT_SGX = 1
#check PVRSRV_RESET_ON_HWTIMEOUT = 1
#config PVRSRV_USSE_EDM_STATUS_DEBUG = 1
#config PVRSRV_DUMP_MK_TRACE = 1
#check SUPPORT_HW_RECOVERY ?= 1
#check SUPPORT_SGX_HWPERF = 1
#check SYS_USING_INTERRUPTS = 1
#check SUPPORT_CPU_CACHED_BUFFERS ?= 1
#check SUPPORT_CACHEFLUSH_ON_ALLOC = 1
#check SUPPORT_MEMINFO_IDS = 1
#check PVR2D_ALT_2DHW = 1
#check LDM_PLATFORM ?= 1
#config  SUPPORT_ACTIVE_POWER_MANAGEMENT = 1
#check SUPPORT_LINUX_USING_WORKQUEUES = 1

# defaults
#check PVR_PROC_USE_SEQ_FILE ?= 1
#check TRANSFER_QUEUE ?= 1
#check SUPPORT_SGX_EVENT_OBJECT ?=1
#check SUPPORT_SECURE_HANDLES          = 1
#check SUPPORT_SRVINIT = 1
#config SUPPORT_PERCONTEXT_PB = 1
#config SUPPORT_SGX_LOW_LATENCY_SCHEDULING ?=1
#check PVR_SECURE_FD_EXPORT = 1

ccflags-y += \
	-DSUPPORT_SGX \
	-DPVRSRV_RESET_ON_HWTIMEOUT \
	-DSUPPORT_HW_RECOVERY \
	-DSUPPORT_SGX_HWPERF \
	-DSYS_USING_INTERRUPTS \
	-DSUPPORT_CPU_CACHED_BUFFERS \
	-DSUPPORT_CACHEFLUSH_ON_ALLOC \
	-DSUPPORT_MEMINFO_IDS \
	-DPVR2D_ALT_2DHW \
	-DLDM_PLATFORM \
	-DPVR_PROC_USE_SEQ_FILE \
	-DTRANSFER_QUEUE \
	-DSUPPORT_SGX_EVENT_OBJECT \
	-DPVR_SECURE_HANDLES \
	-DSUPPORT_SRVINIT \
	-DPVR_SECURE_FD_EXPORT

#ccflags-$(CONFIG_PREEMPT) += \
# in later sources it requires kernel preemption
ccflags-y += \
	-DPVR_LINUX_USING_WORKQUEUES \
	-DPVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE \
	-DPVR_LINUX_TIMERS_USING_WORKQUEUES \
	-DSYS_CUSTOM_POWERLOCK_WRAP

ccflags-y += \
	-DPVR_BUILD_DATE="" \
	-DPVRSRV_MODNAME="\"pvrsrvkm\""

ccflags-$(CONFIG_PVR_BUILD_RELEASE) += \
	-DPVR_BUILD_TYPE="\"release\"" -DRELEASE

ccflags-$(CONFIG_PVR_BUILD_DEBUG) += \
	-DPVR_BUILD_TYPE="\"debug\"" -DDEBUG

ccflags-$(CONFIG_PVR_NEED_PVR_DPF) += -DPVRSRV_NEED_PVR_DPF
ccflags-$(CONFIG_PVR_NEED_PVR_ASSERT) += -DPVRSRV_NEED_PVR_ASSERT

ccflags-$(CONFIG_PVR_DEBUG_MEMORY) += \
	-DDEBUG_LINUX_MEMORY_ALLOCATIONS \
	-DDEBUG_LINUX_MEM_AREAS \
	-DDEBUG_LINUX_MMAP_AREAS

ccflags-$(CONFIG_PVR_DEBUG_BRIDGE_KM) += -DDEBUG_BRIDGE_KM
ccflags-$(CONFIG_PVR_DEBUG_TRACE_BRIDGE_KM) += -DDEBUG_TRACE_BRIDGE_KM
ccflags-$(CONFIG_PVR_DEBUG_BRIDGE_KM_DISPATCH_TABLE) += -DDEBUG_BRIDGE_KM_DISPATCH_TABLE

ccflags-$(CONFIG_PVR_PERCONTEXT_PB) += -DSUPPORT_PERCONTEXT_PB
ccflags-$(CONFIG_PVR_SGX_LOW_LATENCY_SCHEDULING) += -DSUPPORT_SGX_LOW_LATENCY_SCHEDULING
ccflags-$(CONFIG_PVR_ACTIVE_POWER_MANAGEMENT) += -DSUPPORT_ACTIVE_POWER_MANAGEMENT -DSYS_SGX_ACTIVE_POWER_LATENCY_MS=$(CONFIG_PVR_ACTIVE_POWER_LATENCY_MS)
ccflags-$(CONFIG_PVR_USSE_EDM_STATUS_DEBUG) += -DPVRSRV_USSE_EDM_STATUS_DEBUG
ccflags-$(CONFIG_PVR_DUMP_MK_TRACE) += -DPVRSRV_DUMP_MK_TRACE

ccflags-$(CONFIG_PVR_PDUMP) += \
	-DPDUMP -DSUPPORT_DBGDRV_EVENT_OBJECTS -DSUPPORT_PDUMP_MULTI_PROCESS

pvrsrvkm-y := \
	osfunc.o \
	mutils.o \
	mmap.o \
	module.o \
	pdump.o \
	proc.o \
	pvr_bridge_k.o \
	pvr_debug.o \
	mm.o \
	mutex.o \
	event.o \
	osperproc.o \
	buffer_manager.o \
	devicemem.o \
	deviceclass.o \
	handle.o \
	hash.o \
	metrics.o \
	pvrsrv.o \
	queue.o \
	ra.o \
	resman.o \
	power.o \
	mem.o \
	pdump_common.o \
	bridged_support.o \
	bridged_pvr_bridge.o \
	perproc.o \
	lists.o \
	sgx/bridged_sgx_bridge.o \
	sgx/sgxinit.o \
	sgx/sgxpower.o \
	sgx/sgxreset.o \
	sgx/sgxutils.o \
	sgx/sgxkick.o \
	sgx/sgxtransfer.o \
	sgx/mmu.o \
	sgx/pb.o

ccflags-$(CONFIG_ARCH_S5PV210) += \
	-DPVR_BUILD_DIR="\"smdkc110_android\"" \
	-Idrivers/gpu/pvr/s5pc110 \
	-DDISPLAY_CONTROLLER=s3c_lcd \
    -DSLSI_S5PC110

ccflags-$(CONFIG_ARCH_S5PV210) += \
	-DSGX540 -DSUPPORT_SGX540 \
	-DSGX_CORE_REV=120

pvrsrvkm-$(CONFIG_ARCH_S5PV210) += \
	s5pc110/sysconfig.o \
	s5pc110/sysutils.o

s3c_lcd-y := \
	s3c_lcd/s3c_displayclass.o \
	s3c_lcd/s3c_lcd.o

s3c_bc-y := \
	s3c_bc/s3c_bc.o \
	s3c_bc/s3c_bc_linux.o


obj-$(CONFIG_PVR_PDUMP) += dbgdrv

obj-$(CONFIG_PVR_SGX) += pvrsrvkm.o

obj-m += s3c_lcd.o

obj-m += s3c_bc.o
