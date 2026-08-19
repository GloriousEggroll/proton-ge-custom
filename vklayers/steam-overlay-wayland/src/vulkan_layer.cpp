#define VK_USE_PLATFORM_WAYLAND_KHR

#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <wayland-client.h>

#include "steam_overlay_bridge.h"
#include "vkroots.h"

namespace ge_steam_overlay
{

struct surface_state
{
    VkInstance instance;
    ge_overlay_wayland_surface *wayland;
};

static std::mutex surface_mutex;
static std::unordered_map<VkSurfaceKHR, surface_state> surfaces;

static void destroy_surface_state(surface_state state)
{
    ge_overlay_wayland_surface_destroy(state.wayland);
}

class instance_overrides
{
public:
    static VkResult CreateWaylandSurfaceKHR(
        const vkroots::VkInstanceDispatch& dispatch,
        VkInstance instance,
        const VkWaylandSurfaceCreateInfoKHR *create_info,
        const VkAllocationCallbacks *allocator,
        VkSurfaceKHR *surface)
    {
        VkResult result = dispatch.CreateWaylandSurfaceKHR(
            instance, create_info, allocator, surface);

        if (result == VK_SUCCESS && create_info && surface && *surface)
        {
            if (auto *wayland = ge_overlay_wayland_surface_create(
                    create_info->display, create_info->surface))
            {
                std::lock_guard lock(surface_mutex);
                surfaces.emplace(*surface, surface_state{instance, wayland});
            }
        }
        return result;
    }

    static void DestroySurfaceKHR(
        const vkroots::VkInstanceDispatch& dispatch,
        VkInstance instance,
        VkSurfaceKHR surface,
        const VkAllocationCallbacks *allocator)
    {
        surface_state state{};

        {
            std::lock_guard lock(surface_mutex);
            if (auto entry = surfaces.find(surface); entry != surfaces.end())
            {
                state = entry->second;
                surfaces.erase(entry);
            }
        }

        if (state.wayland) destroy_surface_state(state);
        dispatch.DestroySurfaceKHR(instance, surface, allocator);
    }

    static void DestroyInstance(
        const vkroots::VkInstanceDispatch& dispatch,
        VkInstance instance,
        const VkAllocationCallbacks *allocator)
    {
        std::vector<surface_state> stale_surfaces;

        {
            std::lock_guard lock(surface_mutex);
            for (auto entry = surfaces.begin(); entry != surfaces.end();)
            {
                if (entry->second.instance != instance)
                {
                    ++entry;
                    continue;
                }
                stale_surfaces.push_back(entry->second);
                entry = surfaces.erase(entry);
            }
        }

        for (auto state : stale_surfaces) destroy_surface_state(state);
        dispatch.DestroyInstance(instance, allocator);
    }
};

class device_overrides
{
public:
    static VkResult QueuePresentKHR(
        const vkroots::VkQueueDispatch& dispatch,
        VkQueue queue,
        const VkPresentInfoKHR *present_info)
    {
        {
            std::lock_guard lock(surface_mutex);
            for (const auto& [surface, state] : surfaces)
            {
                (void)surface;
                ge_overlay_wayland_surface_dispatch(state.wayland);
            }
        }

        return dispatch.QueuePresentKHR(queue, present_info);
    }
};

} /* namespace ge_steam_overlay */

VKROOTS_DEFINE_LAYER_INTERFACES(ge_steam_overlay::instance_overrides,
                                ge_steam_overlay::device_overrides)
