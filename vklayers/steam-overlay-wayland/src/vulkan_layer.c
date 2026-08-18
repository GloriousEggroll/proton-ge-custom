/* A deliberately transparent Vulkan layer. Its shared object also hosts the
 * Wine Wayland Steam overlay bridge, including for non-Vulkan applications. */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#if defined(__GNUC__)
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#else
#define VK_LAYER_EXPORT
#endif

struct instance_dispatch
{
    void *key;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;
    PFN_vkDestroyInstance destroy_instance;
    struct instance_dispatch *next;
};

struct device_dispatch
{
    void *key;
    PFN_vkGetDeviceProcAddr get_device_proc_addr;
    PFN_vkDestroyDevice destroy_device;
    struct device_dispatch *next;
};

static pthread_mutex_t dispatch_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct instance_dispatch *instance_dispatches;
static struct device_dispatch *device_dispatches;

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
GEOverlay_GetInstanceProcAddr(VkInstance instance, const char *name);
VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
GEOverlay_GetDeviceProcAddr(VkDevice device, const char *name);

static void *dispatch_key(const void *handle)
{
    return handle ? *(void *const *)handle : NULL;
}

static VkLayerInstanceCreateInfo *find_instance_link_info(
    const VkInstanceCreateInfo *create_info)
{
    VkLayerInstanceCreateInfo *info =
        (VkLayerInstanceCreateInfo *)create_info->pNext;

    while (info)
    {
        if (info->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
            info->function == VK_LAYER_LINK_INFO)
            return info;
        info = (VkLayerInstanceCreateInfo *)info->pNext;
    }
    return NULL;
}

static VkLayerDeviceCreateInfo *find_device_link_info(
    const VkDeviceCreateInfo *create_info)
{
    VkLayerDeviceCreateInfo *info =
        (VkLayerDeviceCreateInfo *)create_info->pNext;

    while (info)
    {
        if (info->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
            info->function == VK_LAYER_LINK_INFO)
            return info;
        info = (VkLayerDeviceCreateInfo *)info->pNext;
    }
    return NULL;
}

static PFN_vkGetInstanceProcAddr find_instance_gipa(const void *handle)
{
    struct instance_dispatch *entry;
    PFN_vkGetInstanceProcAddr result = NULL;
    void *key = dispatch_key(handle);

    pthread_mutex_lock(&dispatch_mutex);
    for (entry = instance_dispatches; entry; entry = entry->next)
        if (entry->key == key)
        {
            result = entry->get_instance_proc_addr;
            break;
        }
    pthread_mutex_unlock(&dispatch_mutex);
    return result;
}

static PFN_vkGetDeviceProcAddr find_device_gdpa(VkDevice device)
{
    struct device_dispatch *entry;
    PFN_vkGetDeviceProcAddr result = NULL;
    void *key = dispatch_key(device);

    pthread_mutex_lock(&dispatch_mutex);
    for (entry = device_dispatches; entry; entry = entry->next)
        if (entry->key == key)
        {
            result = entry->get_device_proc_addr;
            break;
        }
    pthread_mutex_unlock(&dispatch_mutex);
    return result;
}

static VKAPI_ATTR VkResult VKAPI_CALL overlay_create_instance(
    const VkInstanceCreateInfo *create_info,
    const VkAllocationCallbacks *allocator, VkInstance *instance)
{
    VkLayerInstanceCreateInfo *link_info;
    PFN_vkGetInstanceProcAddr next_gipa;
    PFN_vkCreateInstance next_create_instance;
    struct instance_dispatch *entry;
    VkResult result;

    if (!create_info || !instance ||
        !(link_info = find_instance_link_info(create_info)) ||
        !link_info->u.pLayerInfo)
        return VK_ERROR_INITIALIZATION_FAILED;

    next_gipa = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    if (!next_gipa) return VK_ERROR_INITIALIZATION_FAILED;
    next_create_instance = (PFN_vkCreateInstance)next_gipa(
        VK_NULL_HANDLE, "vkCreateInstance");
    if (!next_create_instance) return VK_ERROR_INITIALIZATION_FAILED;

    link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
    result = next_create_instance(create_info, allocator, instance);
    if (result != VK_SUCCESS) return result;

    if (!(entry = calloc(1, sizeof(*entry))))
    {
        PFN_vkDestroyInstance destroy_instance =
            (PFN_vkDestroyInstance)next_gipa(*instance, "vkDestroyInstance");
        if (destroy_instance) destroy_instance(*instance, allocator);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    entry->key = dispatch_key(*instance);
    entry->get_instance_proc_addr = next_gipa;
    entry->destroy_instance =
        (PFN_vkDestroyInstance)next_gipa(*instance, "vkDestroyInstance");
    pthread_mutex_lock(&dispatch_mutex);
    entry->next = instance_dispatches;
    instance_dispatches = entry;
    pthread_mutex_unlock(&dispatch_mutex);
    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL overlay_destroy_instance(
    VkInstance instance, const VkAllocationCallbacks *allocator)
{
    struct instance_dispatch **cursor;
    struct instance_dispatch *entry = NULL;
    void *key = dispatch_key(instance);

    pthread_mutex_lock(&dispatch_mutex);
    for (cursor = &instance_dispatches; *cursor; cursor = &(*cursor)->next)
        if ((*cursor)->key == key)
        {
            entry = *cursor;
            *cursor = entry->next;
            break;
        }
    pthread_mutex_unlock(&dispatch_mutex);

    if (entry)
    {
        if (entry->destroy_instance)
            entry->destroy_instance(instance, allocator);
        free(entry);
    }
}

static VKAPI_ATTR VkResult VKAPI_CALL overlay_create_device(
    VkPhysicalDevice physical_device, const VkDeviceCreateInfo *create_info,
    const VkAllocationCallbacks *allocator, VkDevice *device)
{
    VkLayerDeviceCreateInfo *link_info;
    PFN_vkGetInstanceProcAddr next_gipa;
    PFN_vkGetDeviceProcAddr next_gdpa;
    PFN_vkCreateDevice next_create_device;
    struct device_dispatch *entry;
    VkResult result;

    if (!create_info || !device ||
        !(link_info = find_device_link_info(create_info)) ||
        !link_info->u.pLayerInfo)
        return VK_ERROR_INITIALIZATION_FAILED;

    next_gipa = link_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    next_gdpa = link_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    if (!next_gipa || !next_gdpa) return VK_ERROR_INITIALIZATION_FAILED;
    next_create_device = (PFN_vkCreateDevice)next_gipa(
        VK_NULL_HANDLE, "vkCreateDevice");
    if (!next_create_device) return VK_ERROR_INITIALIZATION_FAILED;

    link_info->u.pLayerInfo = link_info->u.pLayerInfo->pNext;
    result = next_create_device(physical_device, create_info, allocator, device);
    if (result != VK_SUCCESS) return result;

    if (!(entry = calloc(1, sizeof(*entry))))
    {
        PFN_vkDestroyDevice destroy_device =
            (PFN_vkDestroyDevice)next_gdpa(*device, "vkDestroyDevice");
        if (destroy_device) destroy_device(*device, allocator);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    entry->key = dispatch_key(*device);
    entry->get_device_proc_addr = next_gdpa;
    entry->destroy_device =
        (PFN_vkDestroyDevice)next_gdpa(*device, "vkDestroyDevice");
    pthread_mutex_lock(&dispatch_mutex);
    entry->next = device_dispatches;
    device_dispatches = entry;
    pthread_mutex_unlock(&dispatch_mutex);
    return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL overlay_destroy_device(
    VkDevice device, const VkAllocationCallbacks *allocator)
{
    struct device_dispatch **cursor;
    struct device_dispatch *entry = NULL;
    void *key = dispatch_key(device);

    pthread_mutex_lock(&dispatch_mutex);
    for (cursor = &device_dispatches; *cursor; cursor = &(*cursor)->next)
        if ((*cursor)->key == key)
        {
            entry = *cursor;
            *cursor = entry->next;
            break;
        }
    pthread_mutex_unlock(&dispatch_mutex);

    if (entry)
    {
        if (entry->destroy_device) entry->destroy_device(device, allocator);
        free(entry);
    }
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
GEOverlay_GetInstanceProcAddr(VkInstance instance, const char *name)
{
    PFN_vkGetInstanceProcAddr next_gipa;

    if (!name) return NULL;
    if (!strcmp(name, "vkGetInstanceProcAddr"))
        return (PFN_vkVoidFunction)GEOverlay_GetInstanceProcAddr;
    if (!strcmp(name, "vkGetDeviceProcAddr"))
        return (PFN_vkVoidFunction)GEOverlay_GetDeviceProcAddr;
    if (!strcmp(name, "vkCreateInstance"))
        return (PFN_vkVoidFunction)overlay_create_instance;
    if (!strcmp(name, "vkDestroyInstance"))
        return (PFN_vkVoidFunction)overlay_destroy_instance;
    if (!strcmp(name, "vkCreateDevice"))
        return (PFN_vkVoidFunction)overlay_create_device;
    if (!strcmp(name, "vkDestroyDevice"))
        return (PFN_vkVoidFunction)overlay_destroy_device;
    if (!instance || !(next_gipa = find_instance_gipa(instance))) return NULL;
    return next_gipa(instance, name);
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
GEOverlay_GetDeviceProcAddr(VkDevice device, const char *name)
{
    PFN_vkGetDeviceProcAddr next_gdpa;

    if (!name) return NULL;
    if (!strcmp(name, "vkGetDeviceProcAddr"))
        return (PFN_vkVoidFunction)GEOverlay_GetDeviceProcAddr;
    if (!strcmp(name, "vkDestroyDevice"))
        return (PFN_vkVoidFunction)overlay_destroy_device;
    if (!device || !(next_gdpa = find_device_gdpa(device))) return NULL;
    return next_gdpa(device, name);
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
GEOverlay_NegotiateLoaderLayerInterfaceVersion(
    VkNegotiateLayerInterface *interface)
{
    if (!interface || interface->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT ||
        interface->loaderLayerInterfaceVersion < 2)
        return VK_ERROR_INITIALIZATION_FAILED;

    if (interface->loaderLayerInterfaceVersion >
        CURRENT_LOADER_LAYER_INTERFACE_VERSION)
        interface->loaderLayerInterfaceVersion =
            CURRENT_LOADER_LAYER_INTERFACE_VERSION;

    interface->pfnGetInstanceProcAddr = GEOverlay_GetInstanceProcAddr;
    interface->pfnGetDeviceProcAddr = GEOverlay_GetDeviceProcAddr;
    interface->pfnGetPhysicalDeviceProcAddr = NULL;
    return VK_SUCCESS;
}
