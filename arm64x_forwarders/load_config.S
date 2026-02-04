/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#define PTR .8byte
#define ALIGN 16
#define EXPORT_SYM(x) .globl x; x:
#define SYM(x) x

.text
.balign ALIGN
#ifdef __arm64ec__
/*
Calls to this are synthesized by the linker when calling into import libraries,
this is referred to as an 'Adjustor Thunk' in ARM64EC documentation.
*/
EXPORT_SYM(__icall_helper_arm64ec)
.seh_proc "__icall_helper_arm64ec"
  stp  fp,   lr, [sp, #-16]!
.seh_save_fplr_x 16
  mov  fp,   sp
.seh_set_fp
.seh_endprologue
  adrp x16, __os_arm64x_check_icall
  ldr  x16, [x16, #:lo12:__os_arm64x_check_icall]
  blr  x16
.seh_startepilogue
  ldp  fp,  lr,  [sp], #16
.seh_save_fplr_x 16
.seh_endepilogue
  br   x11
.seh_endproc
#endif

SYM(__guard_check_icall_dummy):
  ret

.section  .00cfg, "dr"
.balign ALIGN
#ifdef __arm64ec__
/*
These symbols are updated at runtime by the dynamic linker to point to emulator
helper routines.
*/
EXPORT_SYM(__os_arm64x_dispatch_call_no_redirect)
  PTR 0
EXPORT_SYM(__os_arm64x_dispatch_ret)
  PTR 0
EXPORT_SYM(__os_arm64x_check_icall)
EXPORT_SYM(__os_arm64x_dispatch_icall)
  PTR 0
EXPORT_SYM(__os_arm64x_check_call)
EXPORT_SYM(__os_arm64x_dispatch_call)
  PTR 0
EXPORT_SYM(__os_arm64x_check_icall_cfg)
EXPORT_SYM(__os_arm64x_check_dispatch_cfg)
  PTR 0
EXPORT_SYM(__os_arm64x_rdtsc)
EXPORT_SYM(__os_arm64x_get_x64_information)
  PTR 0
EXPORT_SYM(__os_arm64x_set_x64_information)
EXPORT_SYM(__os_arm64x_cpuidex)
  PTR 0
EXPORT_SYM(__os_arm64x_x64_jump)
EXPORT_SYM(__os_arm64x_dispatch_fptr)
  PTR 0
EXPORT_SYM(__os_arm64x_helper0)
  PTR 0
EXPORT_SYM(__os_arm64x_helper1)
  PTR 0
EXPORT_SYM(__os_arm64x_helper2)
  PTR 0
EXPORT_SYM(__os_arm64x_helper3)
  PTR 0
EXPORT_SYM(__os_arm64x_helper4)
  PTR 0
EXPORT_SYM(__os_arm64x_helper5)
  PTR 0
EXPORT_SYM(__os_arm64x_helper6)
  PTR 0
EXPORT_SYM(__os_arm64x_helper7)
  PTR 0
EXPORT_SYM(__os_arm64x_helper8)
  PTR 0
#endif
EXPORT_SYM(__guard_check_icall_fptr)
  PTR SYM(__guard_check_icall_dummy)

#ifdef __arm64ec__
/*
This structure is read at runtime by the dynamic linker on ARM64EC to configure
metadata necessary for EC code to interface with x86_64 code.
*/
.section  .rdata,"dr"
.balign ALIGN
EXPORT_SYM(__chpe_metadata)
  .4byte 1 /* Version */
  .4byte __hybrid_code_map@IMGREL /* CodeMap */
  .4byte __hybrid_code_map_count /* CodeMapCount */
  .4byte __x64_code_ranges_to_entry_points@IMGREL /* CodeRangesToEntryPoints */
  .4byte __arm64x_redirection_metadata@IMGREL /* RedirectionMetadata */
  .4byte __os_arm64x_dispatch_call_no_redirect@IMGREL /* __os_arm64x_dispatch_call_no_redirect */
  .4byte __os_arm64x_dispatch_ret@IMGREL /* __os_arm64x_dispatch_ret */
  .4byte __os_arm64x_check_call@IMGREL /* __os_arm64x_dispatch_call */
  .4byte __os_arm64x_check_icall@IMGREL /* __os_arm64x_dispatch_icall */
  .4byte __os_arm64x_check_icall_cfg@IMGREL /* __os_arm64x_dispatch_icall_cfg */
  .4byte __arm64x_native_entrypoint@IMGREL /* AlternateEntryPoint */
  .4byte __hybrid_auxiliary_iat@IMGREL /* AuxiliaryIAT */
  .4byte __x64_code_ranges_to_entry_points_count /* CodeRangesToEntryPointsCount */
  .4byte __arm64x_redirection_metadata_count /* RedirectionMetadataCount */
  .4byte __os_arm64x_get_x64_information@IMGREL /* GetX64InformationFunctionPointer */
  .4byte __os_arm64x_set_x64_information@IMGREL /* SetX64InformationFunctionPointer */
  .4byte __arm64x_extra_rfe_table@IMGREL /* ExtraRFETable */
  .4byte __arm64x_extra_rfe_table_size /* ExtraRFETableSize */
  .4byte __os_arm64x_x64_jump@IMGREL /* __os_arm64x_dispatch_fptr */
  .4byte __hybrid_auxiliary_iat_copy@IMGREL /* AuxiliaryIATCopy */
  /* The following members are undocumented */
  .4byte __os_arm64x_helper0@IMGREL
  .4byte __os_arm64x_helper1@IMGREL
  .4byte __os_arm64x_helper2@IMGREL
  .4byte __os_arm64x_helper3@IMGREL
  .4byte __os_arm64x_helper4@IMGREL
  .4byte __os_arm64x_helper5@IMGREL
  .4byte __os_arm64x_helper6@IMGREL
  .4byte __os_arm64x_helper7@IMGREL
  .4byte __os_arm64x_helper8@IMGREL
#endif

.section  .rdata,"dr"
.globl  SYM(_load_config_used)
.balign ALIGN
SYM(_load_config_used):
  .4byte  SYM(_load_config_used__end) - SYM(_load_config_used) /* Size */
  .4byte  0 /* TimeDateStamp */
  .2byte  0 /* MajorVersion */
  .2byte  0 /* MinorVersion */
  .4byte  0 /* GLobalFlagsClear */
  .4byte  0 /* GlobalFlagsSet */
  .4byte  0 /* CriticalSectionDefaultTimeout */
  PTR  0 /* DeCommitFreeBlockThreshold */
  PTR  0 /* DeCommitTotalFreeThreshold */
  PTR  0 /* LockPrefixTable */
  PTR  0 /* MaximumAllocationSize */
  PTR  0 /* VirtualMemoryThreshold */
  PTR  0 /* ProcessAffinityMask */
  .4byte  0 /* ProcessHeapFlags */
  .2byte  0 /* CSDVersion */
  .2byte  0 /* DependentLoadFlags */
  PTR  0 /* EditList */
  PTR  0 /* SecurityCookie */
  PTR  0 /* SEHandlerTable */
  PTR  0 /* SEHandlerCount */
  PTR  SYM(__guard_check_icall_fptr) /* GuardCFCheckFunction */
  PTR  0 /* GuardCFCheckDispatch */
  PTR  SYM(__guard_fids_table) /* GuardCFFunctionTable */
  PTR  SYM(__guard_fids_count) /* GuardCFFunctionCount */
  .4byte  SYM(__guard_flags) /* GuardFlags */
  .2byte  0 /* CodeIntegrity_Flags */
  .2byte  0 /* CodeIntegrity_Catalog */
  .4byte  0 /* CodeIntegrity_CatalogOffset */
  .4byte  0 /* CodeIntegrity_Reserved */
  PTR  SYM(__guard_iat_table) /* GuardAddressTakenIatEntryTable */
  PTR  SYM(__guard_iat_count) /* GuardAddressTakenIatEntryCount */
  PTR  SYM(__guard_longjmp_table) /* GuardLongJumpTargetTable */
  PTR  SYM(__guard_longjmp_count) /* GuardLongJumpTargetCount */
  PTR  0 /* DynamicValueRelocTable */
#ifdef __arm64ec__
  PTR  SYM(__chpe_metadata) /* CHPEMetadataPointer */
#endif
  PTR  0 /* GuardRFFailureRoutine */
  PTR  0 /* GuardRFFailureRoutineFunctionPointer */
  .4byte  0 /* DynamicValueRelocTableOffset */
  .2byte  0 /* DynamicValueRelocTableSection */
  .2byte  0 /* Reserved2 */
  PTR  0 /* GuardRFVerifyStackPointerFunctionPointer */
  .4byte  0 /* HotPatchTableOffset */
  .4byte  0 /* Reserved3 */
  PTR  0 /* EnclaveConfigurationPointer */
  PTR  0 /* VolatileMetadataPointer */
  PTR SYM(__guard_eh_cont_table) /* GuardEHContinuationTable */
  PTR SYM(__guard_eh_cont_count) /* GuardEHContinuationCount */
  PTR  0 /* GuardXFGCheckFunctionPointer */
  PTR  0 /* GuardXFGDispatchFunctionPointer */
  PTR  0 /* GuardXFGTableDispatchFunctionPointer */
  PTR  0 /* CastGuardOsDeterminedFailureMode */

SYM(_load_config_used__end):
