/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewSize( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewSize_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewSize_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewSize( params->nWidth, params->nHeight );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewSize( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewSize_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewSize_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewSize( params->nWidth, params->nHeight );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewSize( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewSize_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewSize_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->GetHeadsetViewSize( params->pnWidth, params->pnHeight );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewSize( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewSize_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewSize_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->GetHeadsetViewSize( params->pnWidth, params->pnHeight );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewMode( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewMode_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewMode_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewMode( params->eHeadsetViewMode );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewMode( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewMode_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewMode_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewMode( params->eHeadsetViewMode );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewMode( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewMode_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewMode_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    params->_ret = iface->GetHeadsetViewMode(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewMode( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewMode_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewMode_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    params->_ret = iface->GetHeadsetViewMode(  );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewCropped( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewCropped_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewCropped_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewCropped( params->bCropped );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewCropped( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewCropped_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewCropped_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewCropped( params->bCropped );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewCropped( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewCropped_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewCropped_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    params->_ret = iface->GetHeadsetViewCropped(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewCropped( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewCropped_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewCropped_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    params->_ret = iface->GetHeadsetViewCropped(  );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewAspectRatio( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewAspectRatio_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewAspectRatio_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    params->_ret = iface->GetHeadsetViewAspectRatio(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewAspectRatio( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewAspectRatio_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewAspectRatio_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    params->_ret = iface->GetHeadsetViewAspectRatio(  );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewBlendRange( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewBlendRange_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewBlendRange_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewBlendRange( params->flStartPct, params->flEndPct );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewBlendRange( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewBlendRange_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_SetHeadsetViewBlendRange_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->SetHeadsetViewBlendRange( params->flStartPct, params->flEndPct );
    return 0;
}
#endif

NTSTATUS IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewBlendRange( void *args )
{
    struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewBlendRange_params *params = (struct IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewBlendRange_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->GetHeadsetViewBlendRange( params->pStartPct, params->pEndPct );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewBlendRange( void *args )
{
    struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewBlendRange_params *params = (struct wow64_IVRHeadsetView_IVRHeadsetView_001_GetHeadsetViewBlendRange_params *)args;
    struct u_IVRHeadsetView_IVRHeadsetView_001 *iface = (struct u_IVRHeadsetView_IVRHeadsetView_001 *)params->u_iface;
    iface->GetHeadsetViewBlendRange( params->pStartPct, params->pEndPct );
    return 0;
}
#endif

