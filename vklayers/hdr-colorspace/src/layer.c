/*
 * VK_LAYER_GE_hdr_colorspace
 *
 * Wine-Wayland shows HDR swapchains with the compositor's Windows HDR image
 * descriptions (windows_bt2100, windows_scrgb). For that the host driver must
 * create the swapchain with VK_COLOR_SPACE_PASS_THROUGH_EXT, so it doesn't
 * attach an image description of its own. Host layers above the driver, such
 * as the Steam overlay and MangoHud, then also see PASS_THROUGH and draw SDR
 * colors into the HDR image, which looks oversaturated.
 *
 * Wine enables this layer itself, which puts it below every implicit layer,
 * closest to the driver. Wine creates the swapchain with the game's real HDR
 * colorspace, so the layers above see it, and right before that tells this
 * layer, through vkSetNextSwapchainHostColorSpaceGE(), which colorspace the
 * driver should get instead. The override applies to the next
 * vkCreateSwapchainKHR call on the same thread only.
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#define EXPORT __attribute__((visibility("default")))

typedef void (VKAPI_PTR *PFN_vkSetNextSwapchainHostColorSpaceGE)( VkDevice device, VkColorSpaceKHR color_space );

struct instance_data
{
    struct instance_data *next;
    void *key;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;
    PFN_vkDestroyInstance destroy_instance;
};

struct device_data
{
    struct device_data *next;
    void *key;
    PFN_vkGetDeviceProcAddr get_device_proc_addr;
    PFN_vkDestroyDevice destroy_device;
    PFN_vkCreateSwapchainKHR create_swapchain;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct instance_data *instances;
static struct device_data *devices;

static __thread int override_pending;
static __thread VkColorSpaceKHR override_color_space;

static void *dispatch_key( const void *handle )
{
    return *(void **)handle;
}

static struct instance_data *find_instance( const void *handle )
{
    struct instance_data *data;
    void *key = dispatch_key( handle );

    pthread_mutex_lock( &lock );
    for (data = instances; data; data = data->next) if (data->key == key) break;
    pthread_mutex_unlock( &lock );
    return data;
}

static struct device_data *find_device( const void *handle )
{
    struct device_data *data;
    void *key = dispatch_key( handle );

    pthread_mutex_lock( &lock );
    for (data = devices; data; data = data->next) if (data->key == key) break;
    pthread_mutex_unlock( &lock );
    return data;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL layer_GetInstanceProcAddr( VkInstance instance, const char *name );
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL layer_GetDeviceProcAddr( VkDevice device, const char *name );

static VKAPI_ATTR VkResult VKAPI_CALL layer_CreateInstance( const VkInstanceCreateInfo *create_info,
                                                            const VkAllocationCallbacks *allocator,
                                                            VkInstance *instance )
{
    VkLayerInstanceCreateInfo *link = (VkLayerInstanceCreateInfo *)create_info->pNext;
    PFN_vkGetInstanceProcAddr next_gipa;
    PFN_vkCreateInstance next_create;
    struct instance_data *data;
    VkResult res;

    while (link && !(link->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
                     link->function == VK_LAYER_LINK_INFO))
        link = (VkLayerInstanceCreateInfo *)link->pNext;
    if (!link) return VK_ERROR_INITIALIZATION_FAILED;

    next_gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    if (!(next_create = (PFN_vkCreateInstance)next_gipa( VK_NULL_HANDLE, "vkCreateInstance" )))
        return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;

    if (!(data = calloc( 1, sizeof(*data) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if ((res = next_create( create_info, allocator, instance )))
    {
        free( data );
        return res;
    }

    data->key = dispatch_key( *instance );
    data->get_instance_proc_addr = next_gipa;
    data->destroy_instance = (PFN_vkDestroyInstance)next_gipa( *instance, "vkDestroyInstance" );

    pthread_mutex_lock( &lock );
    data->next = instances;
    instances = data;
    pthread_mutex_unlock( &lock );
    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL layer_DestroyInstance( VkInstance instance, const VkAllocationCallbacks *allocator )
{
    struct instance_data **iter, *data = NULL;
    void *key;

    if (!instance) return;
    key = dispatch_key( instance );

    pthread_mutex_lock( &lock );
    for (iter = &instances; *iter; iter = &(*iter)->next)
    {
        if ((*iter)->key != key) continue;
        data = *iter;
        *iter = data->next;
        break;
    }
    pthread_mutex_unlock( &lock );

    if (!data) return;
    if (data->destroy_instance) data->destroy_instance( instance, allocator );
    free( data );
}

static VKAPI_ATTR VkResult VKAPI_CALL layer_CreateDevice( VkPhysicalDevice physical_device,
                                                          const VkDeviceCreateInfo *create_info,
                                                          const VkAllocationCallbacks *allocator,
                                                          VkDevice *device )
{
    VkLayerDeviceCreateInfo *link = (VkLayerDeviceCreateInfo *)create_info->pNext;
    PFN_vkGetInstanceProcAddr next_gipa;
    PFN_vkGetDeviceProcAddr next_gdpa;
    PFN_vkCreateDevice next_create;
    struct device_data *data;
    VkResult res;

    while (link && !(link->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
                     link->function == VK_LAYER_LINK_INFO))
        link = (VkLayerDeviceCreateInfo *)link->pNext;
    if (!link) return VK_ERROR_INITIALIZATION_FAILED;

    next_gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    next_gdpa = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    if (!(next_create = (PFN_vkCreateDevice)next_gipa( VK_NULL_HANDLE, "vkCreateDevice" )))
        return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;

    if (!(data = calloc( 1, sizeof(*data) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if ((res = next_create( physical_device, create_info, allocator, device )))
    {
        free( data );
        return res;
    }

    data->key = dispatch_key( *device );
    data->get_device_proc_addr = next_gdpa;
    data->destroy_device = (PFN_vkDestroyDevice)next_gdpa( *device, "vkDestroyDevice" );
    data->create_swapchain = (PFN_vkCreateSwapchainKHR)next_gdpa( *device, "vkCreateSwapchainKHR" );

    pthread_mutex_lock( &lock );
    data->next = devices;
    devices = data;
    pthread_mutex_unlock( &lock );
    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL layer_DestroyDevice( VkDevice device, const VkAllocationCallbacks *allocator )
{
    struct device_data **iter, *data = NULL;
    void *key;

    if (!device) return;
    key = dispatch_key( device );

    pthread_mutex_lock( &lock );
    for (iter = &devices; *iter; iter = &(*iter)->next)
    {
        if ((*iter)->key != key) continue;
        data = *iter;
        *iter = data->next;
        break;
    }
    pthread_mutex_unlock( &lock );

    if (!data) return;
    if (data->destroy_device) data->destroy_device( device, allocator );
    free( data );
}

static VKAPI_ATTR VkResult VKAPI_CALL layer_CreateSwapchainKHR( VkDevice device,
                                                                const VkSwapchainCreateInfoKHR *create_info,
                                                                const VkAllocationCallbacks *allocator,
                                                                VkSwapchainKHR *swapchain )
{
    struct device_data *data = find_device( device );
    VkSwapchainCreateInfoKHR info;

    if (!data || !data->create_swapchain) return VK_ERROR_INITIALIZATION_FAILED;

    if (override_pending)
    {
        override_pending = 0;
        if (create_info->imageColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ||
            create_info->imageColorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
        {
            info = *create_info;
            info.imageColorSpace = override_color_space;
            return data->create_swapchain( device, &info, allocator, swapchain );
        }
    }

    return data->create_swapchain( device, create_info, allocator, swapchain );
}

static VKAPI_ATTR void VKAPI_CALL layer_SetNextSwapchainHostColorSpaceGE( VkDevice device, VkColorSpaceKHR color_space )
{
    (void)device;
    override_color_space = color_space;
    override_pending = 1;
}

static PFN_vkVoidFunction layer_device_function( const char *name )
{
    if (!strcmp( name, "vkGetDeviceProcAddr" )) return (PFN_vkVoidFunction)layer_GetDeviceProcAddr;
    if (!strcmp( name, "vkDestroyDevice" )) return (PFN_vkVoidFunction)layer_DestroyDevice;
    if (!strcmp( name, "vkCreateSwapchainKHR" )) return (PFN_vkVoidFunction)layer_CreateSwapchainKHR;
    if (!strcmp( name, "vkSetNextSwapchainHostColorSpaceGE" ))
        return (PFN_vkVoidFunction)layer_SetNextSwapchainHostColorSpaceGE;
    return NULL;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL layer_GetDeviceProcAddr( VkDevice device, const char *name )
{
    struct device_data *data;
    PFN_vkVoidFunction func;

    if ((func = layer_device_function( name ))) return func;
    if (!device || !(data = find_device( device ))) return NULL;
    return data->get_device_proc_addr( device, name );
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL layer_GetInstanceProcAddr( VkInstance instance, const char *name )
{
    struct instance_data *data;
    PFN_vkVoidFunction func;

    if (!strcmp( name, "vkGetInstanceProcAddr" )) return (PFN_vkVoidFunction)layer_GetInstanceProcAddr;
    if (!strcmp( name, "vkCreateInstance" )) return (PFN_vkVoidFunction)layer_CreateInstance;
    if (!strcmp( name, "vkDestroyInstance" )) return (PFN_vkVoidFunction)layer_DestroyInstance;
    if (!strcmp( name, "vkCreateDevice" )) return (PFN_vkVoidFunction)layer_CreateDevice;
    if ((func = layer_device_function( name ))) return func;

    if (!instance || !(data = find_instance( instance ))) return NULL;
    return data->get_instance_proc_addr( instance, name );
}

EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion( VkNegotiateLayerInterface *version )
{
    if (!version || version->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (version->loaderLayerInterfaceVersion > 2) version->loaderLayerInterfaceVersion = 2;
    version->pfnGetInstanceProcAddr = layer_GetInstanceProcAddr;
    version->pfnGetDeviceProcAddr = layer_GetDeviceProcAddr;
    version->pfnGetPhysicalDeviceProcAddr = NULL;
    return VK_SUCCESS;
}
