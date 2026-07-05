"""Various utility functions for use in the proton script"""

import io
import sys
import os
import uuid
from argparse import Namespace
from collections import defaultdict
from dataclasses import dataclass
from functools import cache
from itertools import groupby
from pathlib import Path

from vulkan import (
    VulkanPhysicalDeviceFeatures,
    VulkanExtensionProperties,
    VkPhysicalDeviceType,
    VulkanError,
    VulkanInstance,
    VulkanVersion,
    VulkanPhysicalDevice,
    VkDriverId
)


base_config = Path(os.getenv('XDG_CONFIG_HOME', '~/.config')).expanduser()
base_cache = Path(os.getenv('XDG_CACHE_HOME', '~/.cache')).expanduser()


class Config(Namespace):
    class Path(Namespace):
        config_dir: Path = base_cache.joinpath('protonfixes')
        cache_dir: Path = base_cache.joinpath('protonfixes')

    path = Path()


class Log:
    @staticmethod
    def info(msg):
        sys.stderr.write('[Utilities] INFO: ' + msg)
        sys.stderr.flush()

    @staticmethod
    def warn(msg):
        sys.stderr.write('[Utilities] WARN: ' + msg)
        sys.stderr.flush()

    @staticmethod
    def crit(msg):
        sys.stderr.write('[Utilities] ERROR: ' + msg)
        sys.stderr.flush()


config = Config()
log = Log()


_config_envvars = {
    'wow64': 'PROTON_USE_WOW64',
    'dlss': 'PROTON_DLSS_UPGRADE',
    'xess': 'PROTON_XESS_UPGRADE',
    'fsr3': 'PROTON_FSR4_UPGRADE',
    'fsr4': 'PROTON_FSR4_UPGRADE',
    'ffx3': 'PROTON_FFX3_UPGRADE',
    'ffx4': 'PROTON_FFX4_UPGRADE',
    'optiscaler': 'PROTON_USE_OPTISCALER',
}


def proton_add_config() -> set:
    configs = os.environ.get("PROTON_ADD_CONFIG", "").split(',')
    steam_configs = set()
    for config in configs:
        if not config or config == '=':
            continue
        if '=' not in config:
            steam_configs.add(config)
            if config in _config_envvars:
                os.environ[_config_envvars[config]] = '1'
        else:
            option, value = config.split('=', maxsplit=1)
            steam_configs.add(option)
            if value and option in _config_envvars:
                os.environ[_config_envvars[option]] = value
    return steam_configs


def log_environment(env: dict, log_file: io.TextIOWrapper):
    log_file.write('======================\n')
    log_file.write('Inherited environment\n')
    for var in (name for name in (
        'DISPLAY',
        'DXVK_FILTER_DEVICE_NAME',
        'DXVK_FILTER_DEVICE_UUID',
        '__NV_PRIME_RENDER_OFFLOAD',
        '__VK_LAYER_NV_optimus',
        '__GLX_VENDOR_LIBRARY_NAME',
        'DXVK_CONFIG',
        'VKD3D_CONFIG',
        'MANGOHUD',
        'PROTON_DISCORD_BRIDGE',
        'PROTON_DLSS_UPGRADE',
        'PROTON_XESS_UPGRADE',
        'PROTON_FSR3_UPGRADE',
        'PROTON_FSR4_UPGRADE',
        'PROTON_FSR4_RDNA3_UPGRADE',
        'PROTON_FFX3_UPGRADE',
        'PROTON_FFX4_UPGRADE',
        'PROTON_MLFG_UPGRADE',
        'PROTON_LOCAL_SHADER_CACHE',
        'WINE_CANONICAL_HOLE',
    ) if name in env):
        log_file.write(var + ": " + env[var] + "\n")


def is_driver_loaded(d):
    try:
        with open('/proc/modules') as f:
            drivers = set([line.partition(' ')[0] for line in f.read().splitlines()])
            if drivers.intersection(d):
                return True
            else:
                return False
    except OSError:
        return False


@dataclass
class GPU:
    deviceType: VkPhysicalDeviceType
    deviceUUID: uuid.UUID
    deviceName: str
    vendorID: int
    driverID: VkDriverId
    driverName: str
    driverInfo: str
    apiVersion: VulkanVersion
    features: VulkanPhysicalDeviceFeatures
    extensions: list[VulkanExtensionProperties]

    @classmethod
    def from_physical_device(cls, device: VulkanPhysicalDevice):
        properties, driver_properties, id_properties = device.get_properties()
        features = device.get_features()
        extensions = device.get_extensions()
        return cls(
            properties.deviceType,
            id_properties.deviceUUID,
            properties.deviceName,
            properties.vendorID,
            driver_properties.driverID,
            driver_properties.driverName,
            driver_properties.driverInfo,
            properties.apiVersion,
            features,
            extensions
        )


@cache
def get_vulkan_gpus() -> list[GPU]:
    gpus: list[GPU] = []
    try:
        with VulkanInstance() as instance:
            for device in instance.enumerate_physical_devices():
                gpus.append(GPU.from_physical_device(device))
    except VulkanError:
        pass
    return gpus


def primary_gpu_supports_vulkan(
    major: int, minor: int, patch: int = 0, /,
    device_filter: str = '',
    device_features: list[str] | None = None,
    device_extensions: dict[str, int] | None = None
) -> bool:
    if device_features is None:
        device_features = []
    if device_extensions is None:
        device_extensions = {}

    gpus = get_vulkan_gpus()
    grouped: dict[VkPhysicalDeviceType, list[GPU]] = defaultdict(list)
    for group_type, gpus in groupby(gpus, lambda x: x.deviceType):
        grouped[group_type] = list(
            gpu
            for gpu in gpus
            if (
                not device_filter or
                device_filter in gpu.deviceName or
                device_filter in str(gpu.deviceUUID).replace('-', '')
            )
        )
    primary_category = (
            grouped[VkPhysicalDeviceType.DISCRETE_GPU] or
            grouped[VkPhysicalDeviceType.INTEGRATED_GPU] or
            grouped[VkPhysicalDeviceType.VIRTUAL_GPU]
    )

    if not primary_category:
        return True

    def supports_extension(gpu: GPU, name: str, version: int) -> bool:
        extension = next((
            e
            for e in gpu.extensions
            if e.extensionName == name
        ), None)
        if not extension:
            return False
        return extension.specVersion >= version

    return any(
        (gpu.apiVersion >= (major, minor, patch)) and
        (all(map(lambda feature: feature in gpu.features, device_features))) and
        (all(map(lambda requested_extension: supports_extension(gpu, *requested_extension), device_extensions.items())))
        for gpu in primary_category
    )


if __name__ == '__main__':

    os.environ["PROTON_ADD_CONFIG"] = ""
    print("PROTON_ADD_CONFIG:", os.environ["PROTON_ADD_CONFIG"])
    print("config set:", proton_add_config())

    os.environ["PROTON_ADD_CONFIG"] = "wow64,fsr4=4.0.0,xess=0,dlss,enablenvapi,noenv=5,,="
    print("PROTON_ADD_CONFIG:", os.environ["PROTON_ADD_CONFIG"])
    print("config set:", proton_add_config())
    for key in os.environ:
        if key in _config_envvars.values():
            print(key, os.environ[key])

    print("\nDifferent versions")
    print(primary_gpu_supports_vulkan(1,2))
    print(primary_gpu_supports_vulkan(1,3))
    print(primary_gpu_supports_vulkan(1,4))
    print(primary_gpu_supports_vulkan(1,5))

    print("\nWith filter")
    print(primary_gpu_supports_vulkan(1,1, device_filter="744c6095a55e1f94172f15a2c18ea17a"))
    print(primary_gpu_supports_vulkan(1,1, device_filter="744c6095a55e1f94172f15a2c18ea17b"))
    print(primary_gpu_supports_vulkan(1,1, device_filter="NVIDIA"))
    print(primary_gpu_supports_vulkan(1,1, device_filter="AMD"))

    print("\nWith features")
    print(primary_gpu_supports_vulkan(1, 1, device_features=['descriptorIndexing']))

    print("\nDriver Info:")
    for gpu in get_vulkan_gpus():
        print(f'{gpu.deviceName=} {gpu.deviceUUID=} {gpu.driverID=} {gpu.driverName=} {gpu.driverInfo=}')

    pass


__all__ = ['primary_gpu_supports_vulkan', 'get_vulkan_gpus', 'log_environment', 'is_driver_loaded']
