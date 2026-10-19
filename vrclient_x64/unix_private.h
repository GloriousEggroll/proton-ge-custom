#ifndef __VRCLIENT_UNIX_PRIVATE_H
#define __VRCLIENT_UNIX_PRIVATE_H

#include <stdarg.h>
#include <stddef.h>

#include <windef.h>
#include <winbase.h>

#include "unixlib.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern char *json_convert_startup_info(const char *startup_info);
extern char *json_convert_paths(const char *input);

extern NTSTATUS vrclient_init( void *args );
extern NTSTATUS wow64_vrclient_init( void *args );
extern NTSTATUS vrclient_init_registry( void *args );
extern NTSTATUS wow64_vrclient_init_registry( void *args );
extern NTSTATUS vrclient_unload( void *args );
extern NTSTATUS wow64_vrclient_unload( void *args );
extern NTSTATUS vrclient_HmdSystemFactory( void *args );
extern NTSTATUS wow64_vrclient_HmdSystemFactory( void *args );
extern NTSTATUS vrclient_VRClientCoreFactory( void *args );
extern NTSTATUS wow64_vrclient_VRClientCoreFactory( void *args );
extern NTSTATUS vrclient_get_unix_buffer( void *args );
extern NTSTATUS wow64_vrclient_get_unix_buffer( void *args );

extern char *vrclient_dos_to_unix_path( const char *src );
extern void vrclient_free_path( char *path );
extern unsigned int vrclient_unix_path_to_dos_path( bool api_result, const char *src, char *dst, uint32_t dst_bytes );

extern BOOL load_vulkan(void);

#if defined(__x86_64__) || defined(__aarch64__)
#define VRCLIENT_UNIX_WOW64_IMPL( iface, version, method, ... ) \
    NTSTATUS wow64_ ## iface ## _ ## iface ## _ ## version ## _ ## method( void *args ) \
    { \
        auto params = (struct wow64_ ## iface ## _ ## iface ## _ ## version ## _ ## method ## _params *)args; \
        auto u_iface = (struct u_ ## iface ## _ ## iface ## _ ## version *)params->u_iface; \
        return iface ## _ ## method( u_iface, params, true, ## __VA_ARGS__ ); \
    }
#else /* defined(__x86_64__) || defined(__aarch64__) */
#define VRCLIENT_UNIX_WOW64_IMPL( iface, version, method, ... )
#endif /* defined(__x86_64__) || defined(__aarch64__) */

#define VRCLIENT_UNIX_IMPL( iface, version, method, ... ) \
    NTSTATUS iface ## _ ## iface ## _ ## version ## _ ## method( void *args ) \
    { \
        auto params = (struct iface ## _ ## iface ## _ ## version ## _ ## method ## _params *)args; \
        auto u_iface = (struct u_ ## iface ## _ ## iface ## _ ## version *)params->u_iface; \
        return iface ## _ ## method( u_iface, params, false, ## __VA_ARGS__ ); \
    } \
    VRCLIENT_UNIX_WOW64_IMPL( iface, version, method, ## __VA_ARGS__ )

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#include "unix_private_generated.h"

#endif /* __VRCLIENT_UNIX_PRIVATE_H */
