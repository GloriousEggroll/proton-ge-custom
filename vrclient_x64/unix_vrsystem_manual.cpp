#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

WINE_DEFAULT_DEBUG_CHANNEL(vrclient);

static VkInstance_T *unwrap_instance( uint32_t type, VkInstance_T *instance )
{
    if (type == TextureType_Vulkan) return vulkan_instance_from_handle( instance )->host.instance;
    return instance;
}

static VkPhysicalDevice get_client_physical_device( VkInstance handle, VkPhysicalDevice host_physical_device )
{
    struct vulkan_instance *instance = vulkan_instance_from_handle( handle );
    unsigned int i;

    for (i = 0; i < instance->physical_device_count; ++i)
    {
        if (instance->physical_devices[i].host.physical_device == host_physical_device)
            return instance->physical_devices[i].client.physical_device;
    }

    ERR( "Unknown native physical device: %p, instance %p, handle %p\n", host_physical_device, instance, handle );
    return NULL;
}

static uint64_t wrap_device( uint32_t type, VkInstance_T *instance, uint64_t device )
{
    if (type == TextureType_Vulkan)
    {
        VkPhysicalDevice_T *phys_device = (VkPhysicalDevice_T *)( intptr_t)device;
        return (uint64_t)(intptr_t)get_client_physical_device( instance, phys_device );
    }

    return device;
}

template< typename Iface, typename Params >
static NTSTATUS IVRSystem_GetOutputDevice( Iface *iface, Params *params, bool wow64 )
{
    VkInstance_T *host_instance = unwrap_instance( params->textureType, params->pInstance );
    uint64_t host_device;

    iface->GetOutputDevice( &host_device, params->textureType, host_instance );
    *params->pnDevice = wrap_device( params->textureType, params->pInstance, host_device );
    return 0;
}

VRCLIENT_UNIX_IMPL( IVRSystem, 017, GetOutputDevice );
VRCLIENT_UNIX_IMPL( IVRSystem, 019, GetOutputDevice );
VRCLIENT_UNIX_IMPL( IVRSystem, 020, GetOutputDevice );
VRCLIENT_UNIX_IMPL( IVRSystem, 021, GetOutputDevice );
VRCLIENT_UNIX_IMPL( IVRSystem, 022, GetOutputDevice );
VRCLIENT_UNIX_IMPL( IVRSystem, 023, GetOutputDevice );
VRCLIENT_UNIX_IMPL( IVRSystem, 026, GetOutputDevice );
