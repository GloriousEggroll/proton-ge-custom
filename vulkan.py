import uuid
from collections.abc import Generator
from ctypes import (
    CDLL,
    c_int,
    c_void_p,
    c_uint32,
    c_char_p,
    POINTER,
    Structure,
    c_float,
    c_char,
    c_uint8,
    c_size_t,
    c_uint64,
    c_int32,
    byref,
    cast,
    pointer
)
from dataclasses import dataclass
from enum import Enum
from functools import reduce
from typing import Self

libvulkan = CDLL('libvulkan.so.1')


class VulkanError(RuntimeError):
    pass


def attach_vkresult_error_handler(func):
    def check(result, _func, _args):
        if result != VkResult.SUCCESS:
            raise VulkanError(f'{func.__name__} returned {result}')

    func.errcheck = check
    func.restype = VkResult
    return func


# https://docs.vulkan.org/refpages/latest/refpages/source/VkInstance.html
VkInstance = c_void_p

# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VkPhysicalDevice
VkPhysicalDevice = c_void_p

# https://docs.vulkan.org/refpages/latest/refpages/source/VkBool32.html
VkBool32 = c_uint32

# https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceSize.html
VkDeviceSize = c_uint64

# https://docs.vulkan.org/refpages/latest/refpages/source/VkFlags.html
VkFlags = c_uint32

# https://docs.vulkan.org/refpages/latest/refpages/source/VkSampleCountFlags.html
VkSampleCountFlags = VkFlags


class VkResult(Enum):
    SUCCESS = 0
    NOT_READY = 1
    TIMEOUT = 2
    EVENT_SET = 3
    EVENT_RESET = 4
    INCOMPLETE = 5
    ERROR_OUT_OF_HOST_MEMORY = -1
    ERROR_OUT_OF_DEVICE_MEMORY = -2
    ERROR_INITIALIZATION_FAILED = -3
    ERROR_DEVICE_LOST = -4
    ERROR_MEMORY_MAP_FAILED = -5
    ERROR_LAYER_NOT_PRESENT = -6
    ERROR_EXTENSION_NOT_PRESENT = -7
    ERROR_FEATURE_NOT_PRESENT = -8
    ERROR_INCOMPATIBLE_DRIVER = -9
    ERROR_TOO_MANY_OBJECTS = -10
    ERROR_FORMAT_NOT_SUPPORTED = -11
    ERROR_FRAGMENTED_POOL = -12
    ERROR_UNKNOWN = -13
    # Provided by VK_VERSION_1_0
    VK_ERROR_VALIDATION_FAILED = -1000011001
    # Provided by VK_VERSION_1_1
    VK_ERROR_OUT_OF_POOL_MEMORY = -1000069000
    VK_ERROR_INVALID_EXTERNAL_HANDLE = -1000072003


# https://docs.vulkan.org/refpages/latest/refpages/source/VkStructureType.html
class VkStructureType(Enum):
    APPLICATION_INFO = 0
    INSTANCE_CREATE_INFO = 1
    PHYSICAL_DEVICE_FEATURES_2 = 1000059000
    PHYSICAL_DEVICE_PROPERTIES_2 = 1000059001
    PHYSICAL_DEVICE_ID_PROPERTIES = 1000071004
    PHYSICAL_DEVICE_DRIVER_PROPERTIES = 1000196000
    # Provided by VK_VERSION_1_2
    PHYSICAL_DEVICE_VULKAN_1_1_FEATURES = 49
    PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES = 50
    PHYSICAL_DEVICE_VULKAN_1_2_FEATURES = 51
    PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES = 52
    # Provided by VK_VERSION_1_3
    PHYSICAL_DEVICE_VULKAN_1_3_FEATURES = 53
    PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES = 54
    # Provided by VK_VERSION_1_4
    PHYSICAL_DEVICE_VULKAN_1_4_FEATURES = 55
    PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES = 56


# https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceType.html
class VkPhysicalDeviceType(Enum):
    OTHER = 0
    INTEGRATED_GPU = 1
    DISCRETE_GPU = 2
    VIRTUAL_GPU = 3
    CPU = 4


# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VkDriverId
class VkDriverId(Enum):
    AMD_PROPRIETARY = 1
    AMD_OPEN_SOURCE = 2
    MESA_RADV = 3
    NVIDIA_PROPRIETARY = 4
    INTEL_PROPRIETARY_WINDOWS = 5
    INTEL_OPEN_SOURCE_MESA = 6
    IMAGINATION_PROPRIETARY = 7
    QUALCOMM_PROPRIETARY = 8
    ARM_PROPRIETARY = 9
    GOOGLE_SWIFTSHADER = 10
    GGP_PROPRIETARY = 11
    BROADCOM_PROPRIETARY = 12
    MESA_LLVMPIPE = 13
    MOLTENVK = 14
    COREAVI_PROPRIETARY = 15
    JUICE_PROPRIETARY = 16
    VERISILICON_PROPRIETARY = 17
    MESA_TURNIP = 18
    MESA_V3DV = 19
    MESA_PANVK = 20
    SAMSUNG_PROPRIETARY = 21
    MESA_VENUS = 22
    MESA_DOZEN = 23
    MESA_NVK = 24
    IMAGINATION_OPEN_SOURCE_MESA = 25
    MESA_HONEYKRISP = 26
    VULKAN_SC_EMULATION_ON_VULKAN = 27
    MESA_KOSMICKRISP = 28
    MESA_GFXSTREAM = 29
    APE_SOFT = 30

    UNKNOWN = -1

    @classmethod
    def _missing_(cls, value):
        # We want to allow other values here, since the driver ID *may* be different
        if isinstance(value, int):
            return cls.UNKNOWN
        return super()._missing_(value)


# https://docs.vulkan.org/refpages/latest/refpages/source/VkApplicationInfo.html
class VkApplicationInfo(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('pApplicationName', c_char_p),
        ('applicationVersion', c_uint32),
        ('pEngineName', c_char_p),
        ('engineVersion', c_uint32),
        ('apiVersion', c_uint32)
    ]


# https://docs.vulkan.org/refpages/latest/refpages/source/VkInstanceCreateInfo.html
class VkInstanceCreateInfo(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('flags', c_uint32),
        ('pApplicationInfo', POINTER(VkApplicationInfo)),
        ('enabledLayerCount', c_uint32),
        ('ppEnabledLayerNames', POINTER(c_char_p)),
        ('enabledExtensionCount', c_uint32),
        ('ppEnabledExtensionNames', POINTER(c_char_p))
    ]


# https://docs.vulkan.org/spec/latest/chapters/limits.html#VkPhysicalDeviceLimits
class VkPhysicalDeviceLimits(Structure):
    _fields_ = [
        ('maxImageDimension1D', c_uint32),
        ('maxImageDimension2D', c_uint32),
        ('maxImageDimension3D', c_uint32),
        ('maxImageDimensionCube', c_uint32),
        ('maxImageArrayLayers', c_uint32),
        ('maxTexelBufferElements', c_uint32),
        ('maxUniformBufferRange', c_uint32),
        ('maxStorageBufferRange', c_uint32),
        ('maxPushConstantsSize', c_uint32),
        ('maxMemoryAllocationCount', c_uint32),
        ('maxSamplerAllocationCount', c_uint32),
        ('bufferImageGranularity', VkDeviceSize),
        ('sparseAddressSpaceSize', VkDeviceSize),
        ('maxBoundDescriptorSets', c_uint32),
        ('maxPerStageDescriptorSamplers', c_uint32),
        ('maxPerStageDescriptorUniformBuffers', c_uint32),
        ('maxPerStageDescriptorStorageBuffers', c_uint32),
        ('maxPerStageDescriptorSampledImages', c_uint32),
        ('maxPerStageDescriptorStorageImages', c_uint32),
        ('maxPerStageDescriptorInputAttachments', c_uint32),
        ('maxPerStageResources', c_uint32),
        ('maxDescriptorSetSamplers', c_uint32),
        ('maxDescriptorSetUniformBuffers', c_uint32),
        ('maxDescriptorSetUniformBuffersDynamic', c_uint32),
        ('maxDescriptorSetStorageBuffers', c_uint32),
        ('maxDescriptorSetStorageBuffersDynamic', c_uint32),
        ('maxDescriptorSetSampledImages', c_uint32),
        ('maxDescriptorSetStorageImages', c_uint32),
        ('maxDescriptorSetInputAttachments', c_uint32),
        ('maxVertexInputAttributes', c_uint32),
        ('maxVertexInputBindings', c_uint32),
        ('maxVertexInputAttributeOffset', c_uint32),
        ('maxVertexInputBindingStride', c_uint32),
        ('maxVertexOutputComponents', c_uint32),
        ('maxTessellationGenerationLevel', c_uint32),
        ('maxTessellationPatchSize', c_uint32),
        ('maxTessellationControlPerVertexInputComponents', c_uint32),
        ('maxTessellationControlPerVertexOutputComponents', c_uint32),
        ('maxTessellationControlPerPatchOutputComponents', c_uint32),
        ('maxTessellationControlTotalOutputComponents', c_uint32),
        ('maxTessellationEvaluationInputComponents', c_uint32),
        ('maxTessellationEvaluationOutputComponents', c_uint32),
        ('maxGeometryShaderInvocations', c_uint32),
        ('maxGeometryInputComponents', c_uint32),
        ('maxGeometryOutputComponents', c_uint32),
        ('maxGeometryOutputVertices', c_uint32),
        ('maxGeometryTotalOutputComponents', c_uint32),
        ('maxFragmentInputComponents', c_uint32),
        ('maxFragmentOutputAttachments', c_uint32),
        ('maxFragmentDualSrcAttachments', c_uint32),
        ('maxFragmentCombinedOutputResources', c_uint32),
        ('maxComputeSharedMemorySize', c_uint32),
        ('maxComputeWorkGroupCount', c_uint32 * 3),
        ('maxComputeWorkGroupInvocations', c_uint32),
        ('maxComputeWorkGroupSize', c_uint32 * 3),
        ('subPixelPrecisionBits', c_uint32),
        ('subTexelPrecisionBits', c_uint32),
        ('mipmapPrecisionBits', c_uint32),
        ('maxDrawIndexedIndexValue', c_uint32),
        ('maxDrawIndirectCount', c_uint32),
        ('maxSamplerLodBias', c_float),
        ('maxSamplerAnisotropy', c_float),
        ('maxViewports', c_uint32),
        ('maxViewportDimensions', c_uint32 * 2),
        ('viewportBoundsRange', c_float * 2),
        ('viewportSubPixelBits', c_uint32),
        ('minMemoryMapAlignment', c_size_t),
        ('minTexelBufferOffsetAlignment', VkDeviceSize),
        ('minUniformBufferOffsetAlignment', VkDeviceSize),
        ('minStorageBufferOffsetAlignment', VkDeviceSize),
        ('minTexelOffset', c_int32),
        ('maxTexelOffset', c_uint32),
        ('minTexelGatherOffset', c_int32),
        ('maxTexelGatherOffset', c_uint32),
        ('minInterpolationOffset', c_float),
        ('maxInterpolationOffset', c_float),
        ('subPixelInterpolationOffsetBits', c_uint32),
        ('maxFramebufferWidth', c_uint32),
        ('maxFramebufferHeight', c_uint32),
        ('maxFramebufferLayers', c_uint32),
        ('framebufferColorSampleCounts', VkSampleCountFlags),
        ('framebufferDepthSampleCounts', VkSampleCountFlags),
        ('framebufferStencilSampleCounts', VkSampleCountFlags),
        ('framebufferNoAttachmentsSampleCounts', VkSampleCountFlags),
        ('maxColorAttachments', c_uint32),
        ('sampledImageColorSampleCounts', VkSampleCountFlags),
        ('sampledImageIntegerSampleCounts', VkSampleCountFlags),
        ('sampledImageDepthSampleCounts', VkSampleCountFlags),
        ('sampledImageStencilSampleCounts', VkSampleCountFlags),
        ('storageImageSampleCounts', VkSampleCountFlags),
        ('maxSampleMaskWords', c_uint32),
        ('timestampComputeAndGraphics', VkBool32),
        ('timestampPeriod', c_float),
        ('maxClipDistances', c_uint32),
        ('maxCullDistances', c_uint32),
        ('maxCombinedClipAndCullDistances', c_uint32),
        ('discreteQueuePriorities', c_uint32),
        ('pointSizeRange', c_float * 2),
        ('lineWidthRange', c_float * 2),
        ('pointSizeGranularity', c_float),
        ('lineWidthGranularity', c_float),
        ('strictLines', VkBool32),
        ('standardSampleLocations', VkBool32),
        ('optimalBufferCopyOffsetAlignment', VkDeviceSize),
        ('optimalBufferCopyRowPitchAlignment', VkDeviceSize),
        ('nonCoherentAtomSize', VkDeviceSize),
    ]


# https://docs.vulkan.org/spec/latest/chapters/sparsemem.html#VkPhysicalDeviceSparseProperties
class VkPhysicalDeviceSparseProperties(Structure):
    _fields_ = [
        ('residencyStandard2DBlockShape', VkBool32),
        ('residencyStandard2DMultisampleBlockShape', VkBool32),
        ('residencyStandard3DBlockShape', VkBool32),
        ('residencyAlignedMipSize', VkBool32),
        ('residencyNonResidentStrict', VkBool32),
    ]


# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VK_MAX_PHYSICAL_DEVICE_NAME_SIZE
VK_MAX_PHYSICAL_DEVICE_NAME_SIZE = 256

# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VK_UUID_SIZE
VK_UUID_SIZE = 16

# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VK_LUID_SIZE
VK_LUID_SIZE = 8

# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VkPhysicalDeviceProperties
class VkPhysicalDeviceProperties(Structure):
    _fields_ = [
        ('apiVersion', c_uint32),
        ('driverVersion', c_uint32),
        ('vendorID', c_uint32),
        ('deviceID', c_uint32),
        ('deviceType', c_int),
        ('deviceName', c_char * VK_MAX_PHYSICAL_DEVICE_NAME_SIZE),
        ('pipelineCacheUUID', c_uint8 * VK_UUID_SIZE),
        ('limits', VkPhysicalDeviceLimits),
        ('sparseProperties', VkPhysicalDeviceSparseProperties)
    ]


# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VkPhysicalDeviceProperties2
class VkPhysicalDeviceProperties2(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('properties', VkPhysicalDeviceProperties)
    ]


# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VK_MAX_DRIVER_NAME_SIZE
VK_MAX_DRIVER_NAME_SIZE = 256

# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VK_MAX_DRIVER_INFO_SIZE
VK_MAX_DRIVER_INFO_SIZE = 256


class VkConformanceVersion(Structure):
    _fields_ = [
        ('major', c_uint8),
        ('minor', c_uint8),
        ('subminor', c_uint8),
        ('patch', c_uint8)
    ]


# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VkPhysicalDeviceDriverProperties
class VkPhysicalDeviceDriverProperties(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('driverID', c_int),
        ('driverName', c_char * VK_MAX_DRIVER_NAME_SIZE),
        ('driverInfo', c_char * VK_MAX_DRIVER_INFO_SIZE),
        ('conformanceVersion', VkConformanceVersion)
    ]


# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VkPhysicalDeviceIDProperties
class VkPhysicalDeviceIDProperties(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('deviceUUID', c_uint8 * VK_UUID_SIZE),
        ('driverUUID', c_uint8 * VK_UUID_SIZE),
        ('deviceLUID', c_uint8 * VK_LUID_SIZE),
        ('deviceNodeMask', c_uint32),
        ('deviceLUIDValid', VkBool32)
    ]


# https://docs.vulkan.org/refpages/latest/refpages/source/VK_MAX_EXTENSION_NAME_SIZE.html
VK_MAX_EXTENSION_NAME_SIZE = 256


# https://docs.vulkan.org/refpages/latest/refpages/source/VkExtensionProperties.html
class VkExtensionProperties(Structure):
    _fields_ = [
        ('extensionName', c_char * VK_MAX_EXTENSION_NAME_SIZE),
        ('specVersion', c_int32),
    ]


# https://docs.vulkan.org/spec/latest/chapters/features.html#VkPhysicalDeviceFeatures
class VkPhysicalDeviceFeatures(Structure):
    _fields_ = [
        ('robustBufferAccess', VkBool32),
        ('fullDrawIndexUint32', VkBool32),
        ('imageCubeArray', VkBool32),
        ('independentBlend', VkBool32),
        ('geometryShader', VkBool32),
        ('tessellationShader', VkBool32),
        ('sampleRateShading', VkBool32),
        ('dualSrcBlend', VkBool32),
        ('logicOp', VkBool32),
        ('multiDrawIndirect', VkBool32),
        ('drawIndirectFirstInstance', VkBool32),
        ('depthClamp', VkBool32),
        ('depthBiasClamp', VkBool32),
        ('fillModeNonSolid', VkBool32),
        ('depthBounds', VkBool32),
        ('wideLines', VkBool32),
        ('largePoints', VkBool32),
        ('alphaToOne', VkBool32),
        ('multiViewport', VkBool32),
        ('samplerAnisotropy', VkBool32),
        ('textureCompressionETC2', VkBool32),
        ('textureCompressionASTC_LDR', VkBool32),
        ('textureCompressionBC', VkBool32),
        ('occlusionQueryPrecise', VkBool32),
        ('pipelineStatisticsQuery', VkBool32),
        ('vertexPipelineStoresAndAtomics', VkBool32),
        ('fragmentStoresAndAtomics', VkBool32),
        ('shaderTessellationAndGeometryPointSize', VkBool32),
        ('shaderImageGatherExtended', VkBool32),
        ('shaderStorageImageExtendedFormats', VkBool32),
        ('shaderStorageImageMultisample', VkBool32),
        ('shaderStorageImageReadWithoutFormat', VkBool32),
        ('shaderStorageImageWriteWithoutFormat', VkBool32),
        ('shaderUniformBufferArrayDynamicIndexing', VkBool32),
        ('shaderSampledImageArrayDynamicIndexing', VkBool32),
        ('shaderStorageBufferArrayDynamicIndexing', VkBool32),
        ('shaderStorageImageArrayDynamicIndexing', VkBool32),
        ('shaderClipDistance', VkBool32),
        ('shaderCullDistance', VkBool32),
        ('shaderFloat64', VkBool32),
        ('shaderInt64', VkBool32),
        ('shaderInt16', VkBool32),
        ('shaderResourceResidency', VkBool32),
        ('shaderResourceMinLod', VkBool32),
        ('sparseBinding', VkBool32),
        ('sparseResidencyBuffer', VkBool32),
        ('sparseResidencyImage2D', VkBool32),
        ('sparseResidencyImage3D', VkBool32),
        ('sparseResidency2Samples', VkBool32),
        ('sparseResidency4Samples', VkBool32),
        ('sparseResidency8Samples', VkBool32),
        ('sparseResidency16Samples', VkBool32),
        ('sparseResidencyAliased', VkBool32),
        ('variableMultisampleRate', VkBool32),
        ('inheritedQueries', VkBool32),
    ]


# https://docs.vulkan.org/spec/latest/chapters/features.html#VkPhysicalDeviceVulkan11Features
class VkPhysicalDeviceVulkan11Features(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('storageBuffer16BitAccess', VkBool32),
        ('uniformAndStorageBuffer16BitAccess', VkBool32),
        ('storagePushConstant16', VkBool32),
        ('storageInputOutput16', VkBool32),
        ('multiview', VkBool32),
        ('multiviewGeometryShader', VkBool32),
        ('multiviewTessellationShader', VkBool32),
        ('variablePointersStorageBuffer', VkBool32),
        ('variablePointers', VkBool32),
        ('protectedMemory', VkBool32),
        ('samplerYcbcrConversion', VkBool32),
        ('shaderDrawParameters', VkBool32),
    ]


# https://docs.vulkan.org/spec/latest/chapters/features.html#VkPhysicalDeviceVulkan12Features
class VkPhysicalDeviceVulkan12Features(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('samplerMirrorClampToEdge', VkBool32),
        ('drawIndirectCount', VkBool32),
        ('storageBuffer8BitAccess', VkBool32),
        ('uniformAndStorageBuffer8BitAccess', VkBool32),
        ('storagePushConstant8', VkBool32),
        ('shaderBufferInt64Atomics', VkBool32),
        ('shaderSharedInt64Atomics', VkBool32),
        ('shaderFloat16', VkBool32),
        ('shaderInt8', VkBool32),
        ('descriptorIndexing', VkBool32),
        ('shaderInputAttachmentArrayDynamicIndexing', VkBool32),
        ('shaderUniformTexelBufferArrayDynamicIndexing', VkBool32),
        ('shaderStorageTexelBufferArrayDynamicIndexing', VkBool32),
        ('shaderUniformBufferArrayNonUniformIndexing', VkBool32),
        ('shaderSampledImageArrayNonUniformIndexing', VkBool32),
        ('shaderStorageBufferArrayNonUniformIndexing', VkBool32),
        ('shaderStorageImageArrayNonUniformIndexing', VkBool32),
        ('shaderInputAttachmentArrayNonUniformIndexing', VkBool32),
        ('shaderUniformTexelBufferArrayNonUniformIndexing', VkBool32),
        ('shaderStorageTexelBufferArrayNonUniformIndexing', VkBool32),
        ('descriptorBindingUniformBufferUpdateAfterBind', VkBool32),
        ('descriptorBindingSampledImageUpdateAfterBind', VkBool32),
        ('descriptorBindingStorageImageUpdateAfterBind', VkBool32),
        ('descriptorBindingStorageBufferUpdateAfterBind', VkBool32),
        ('descriptorBindingUniformTexelBufferUpdateAfterBind', VkBool32),
        ('descriptorBindingStorageTexelBufferUpdateAfterBind', VkBool32),
        ('descriptorBindingUpdateUnusedWhilePending', VkBool32),
        ('descriptorBindingPartiallyBound', VkBool32),
        ('descriptorBindingVariableDescriptorCount', VkBool32),
        ('runtimeDescriptorArray', VkBool32),
        ('samplerFilterMinmax', VkBool32),
        ('scalarBlockLayout', VkBool32),
        ('imagelessFramebuffer', VkBool32),
        ('uniformBufferStandardLayout', VkBool32),
        ('shaderSubgroupExtendedTypes', VkBool32),
        ('separateDepthStencilLayouts', VkBool32),
        ('hostQueryReset', VkBool32),
        ('timelineSemaphore', VkBool32),
        ('bufferDeviceAddress', VkBool32),
        ('bufferDeviceAddressCaptureReplay', VkBool32),
        ('bufferDeviceAddressMultiDevice', VkBool32),
        ('vulkanMemoryModel', VkBool32),
        ('vulkanMemoryModelDeviceScope', VkBool32),
        ('vulkanMemoryModelAvailabilityVisibilityChains', VkBool32),
        ('shaderOutputViewportIndex', VkBool32),
        ('shaderOutputLayer', VkBool32),
        ('subgroupBroadcastDynamicId', VkBool32),
    ]


# https://docs.vulkan.org/spec/latest/chapters/features.html#VkPhysicalDeviceVulkan13Features
class VkPhysicalDeviceVulkan13Features(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('robustImageAccess', VkBool32),
        ('inlineUniformBlock', VkBool32),
        ('descriptorBindingInlineUniformBlockUpdateAfterBind', VkBool32),
        ('pipelineCreationCacheControl', VkBool32),
        ('privateData', VkBool32),
        ('shaderDemoteToHelperInvocation', VkBool32),
        ('shaderTerminateInvocation', VkBool32),
        ('subgroupSizeControl', VkBool32),
        ('computeFullSubgroups', VkBool32),
        ('synchronization2', VkBool32),
        ('textureCompressionASTC_HDR', VkBool32),
        ('shaderZeroInitializeWorkgroupMemory', VkBool32),
        ('dynamicRendering', VkBool32),
        ('shaderIntegerDotProduct', VkBool32),
        ('maintenance4', VkBool32),
    ]


# https://docs.vulkan.org/spec/latest/chapters/features.html#VkPhysicalDeviceVulkan14Features
class VkPhysicalDeviceVulkan14Features(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('globalPriorityQuery', VkBool32),
        ('shaderSubgroupRotate', VkBool32),
        ('shaderSubgroupRotateClustered', VkBool32),
        ('shaderFloatControls2', VkBool32),
        ('shaderExpectAssume', VkBool32),
        ('rectangularLines', VkBool32),
        ('bresenhamLines', VkBool32),
        ('smoothLines', VkBool32),
        ('stippledRectangularLines', VkBool32),
        ('stippledBresenhamLines', VkBool32),
        ('stippledSmoothLines', VkBool32),
        ('vertexAttributeInstanceRateDivisor', VkBool32),
        ('vertexAttributeInstanceRateZeroDivisor', VkBool32),
        ('indexTypeUint8', VkBool32),
        ('dynamicRenderingLocalRead', VkBool32),
        ('maintenance5', VkBool32),
        ('maintenance6', VkBool32),
        ('pipelineProtectedAccess', VkBool32),
        ('pipelineRobustness', VkBool32),
        ('hostImageCopy', VkBool32),
        ('pushDescriptor', VkBool32),
    ]


# https://docs.vulkan.org/spec/latest/chapters/features.html#vkGetPhysicalDeviceFeatures2
class VkPhysicalDeviceFeatures2(Structure):
    _fields_ = [
        ('sType', c_int),
        ('pNext', c_void_p),
        ('features', VkPhysicalDeviceFeatures)
    ]


# https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateInstance.html
vkCreateInstance = attach_vkresult_error_handler(libvulkan.vkCreateInstance)
vkCreateInstance.argtypes = [POINTER(VkInstanceCreateInfo), c_void_p, POINTER(VkInstance)]

# https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyInstance.html
vkDestroyInstance = libvulkan.vkDestroyInstance
vkDestroyInstance.argtypes = [VkInstance, c_void_p]
vkDestroyInstance.restype = None

# https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumeratePhysicalDevices.html
vkEnumeratePhysicalDevices = attach_vkresult_error_handler(libvulkan.vkEnumeratePhysicalDevices)
vkEnumeratePhysicalDevices.argtypes = [VkInstance, POINTER(c_uint32), POINTER(VkPhysicalDevice)]

# https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceFeatures2.html
vkGetPhysicalDeviceFeatures2 = libvulkan.vkGetPhysicalDeviceFeatures2
vkGetPhysicalDeviceFeatures2.argtypes = [VkPhysicalDevice, POINTER(VkPhysicalDeviceFeatures2)]
vkGetPhysicalDeviceFeatures2.restype = None

# https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#vkGetPhysicalDeviceProperties2
vkGetPhysicalDeviceProperties2 = libvulkan.vkGetPhysicalDeviceProperties2
vkGetPhysicalDeviceProperties2.argtypes = [VkPhysicalDevice, POINTER(VkPhysicalDeviceProperties2)]
vkGetPhysicalDeviceProperties2.restype = None

# https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumerateDeviceExtensionProperties.html
vkEnumerateDeviceExtensionProperties = libvulkan.vkEnumerateDeviceExtensionProperties
vkEnumerateDeviceExtensionProperties.argtypes = [VkPhysicalDevice, c_char_p, POINTER(c_uint32), POINTER(VkExtensionProperties)]
vkEnumerateDeviceExtensionProperties.restype = None

# https://docs.vulkan.org/spec/latest/chapters/extensions.html#extendingvulkan-coreversions-versionnumbers
VK_VERSION_MAJOR = lambda version: (version >> 22)
VK_VERSION_MINOR = lambda version: ((version >> 12) & 0x3FF)
VK_VERSION_PATCH = lambda version: (version & 0xFFF)
VK_MAKE_VERSION = lambda major, minor, patch: ((major << 22) | (minor << 12) | patch)
# https://docs.vulkan.org/refpages/latest/refpages/source/VK_MAKE_API_VERSION.html
VK_MAKE_API_VERSION = lambda variant, major, minor, patch: ((variant << 29) | (major << 22) | (minor << 12) | patch)


#
# Non-standard wrappers
#


class VulkanVersion:
    def __init__(self, packed_version: int):
        self._version = packed_version

    def __repr__(self) -> str:
        return f'{VK_VERSION_MAJOR(self._version)}.{VK_VERSION_MINOR(self._version)}.{VK_VERSION_PATCH(self._version)}'

    @classmethod
    def from_version(cls, major: int, minor: int, patch: int) -> Self:
        return cls(VK_MAKE_VERSION(major, minor, patch))

    @classmethod
    def from_tuple(cls, ver: tuple[int, int, int]):
        return cls.from_version(*ver)

    @classmethod
    def _try_convert_to_cls(cls, thing) -> Self | None:
        if isinstance(thing, cls):
            return thing
        if isinstance(thing, tuple):
            major, minor, patch = thing
            return cls.from_version(major, minor, patch)
        return None

    def __ge__(self, other) -> bool:
        other_ver = self._try_convert_to_cls(other)
        if other_ver:
            return self._version >= other_ver._version
        return super().__ge__(self, other)


@dataclass
class VulkanPhysicalDeviceProperties:
    apiVersion: VulkanVersion
    driverVersion: VulkanVersion
    vendorID: int
    deviceID: int
    deviceType: VkPhysicalDeviceType
    deviceName: str
    pipelineCacheUUID: list[int]
    # limits
    # sparseProperties

    @classmethod
    def from_native_obj(cls, properties: VkPhysicalDeviceProperties) -> Self:
        return cls(
            VulkanVersion(properties.apiVersion),
            VulkanVersion(properties.driverVersion),
            properties.vendorID,
            properties.deviceID,
            VkPhysicalDeviceType(properties.deviceType),
            properties.deviceName.decode(),
            properties.pipelineCacheUUID,
        )


@dataclass
class VulkanPhysicalDeviceIDProperties:
    deviceUUID: uuid.UUID
    driverUUID: uuid.UUID
    deviceLUID: list[int]
    deviceNodeMask: int
    deviceLUIDValid: bool

    @classmethod
    def from_native_obj(cls, properties: VkPhysicalDeviceIDProperties) -> Self:
        make_uuid = lambda uuid_arr: uuid.UUID(int=int.from_bytes(uuid_arr, 'big'))
        return cls(
            make_uuid(properties.deviceUUID),
            make_uuid(properties.driverUUID),
            properties.deviceLUID,
            properties.deviceNodeMask,
            properties.deviceLUIDValid,
        )


@dataclass
class VulkanPhysicalDeviceDriverProperties:
    driverID: VkDriverId
    driverName: str
    driverInfo: str
    # conformanceVersion

    @classmethod
    def from_native_obj(cls, properties: VkPhysicalDeviceDriverProperties) -> Self:
        return cls(
            VkDriverId(properties.driverID),
            properties.driverName,
            properties.driverInfo
        )


class VulkanPhysicalDeviceFeatures(set):
    def __init__(self, arg):
        super().__init__(arg)

    @classmethod
    def from_native_obj(cls, features: Structure):
        return cls(
            name
            for name, dtype in features._fields_
            if dtype == VkBool32 and bool(getattr(features, name))
        )


@dataclass
class VulkanExtensionProperties:
    extensionName: str
    specVersion: int

    @classmethod
    def from_native_obj(cls, properties: VkExtensionProperties) -> Self:
        return cls(
            properties.extensionName.decode(),
            properties.specVersion,
        )


class VulkanPhysicalDevice:
    def __init__(self, handle: VkPhysicalDevice):
        self._handle = handle

    def get_properties(self) -> tuple[
        VulkanPhysicalDeviceProperties, VulkanPhysicalDeviceDriverProperties, VulkanPhysicalDeviceIDProperties
    ]:
        idprop = VkPhysicalDeviceIDProperties()
        idprop.sType = VkStructureType.PHYSICAL_DEVICE_ID_PROPERTIES.value
        idprop.pNext = c_void_p()

        driver_props = VkPhysicalDeviceDriverProperties()
        driver_props.sType = VkStructureType.PHYSICAL_DEVICE_DRIVER_PROPERTIES.value
        driver_props.pNext = cast(pointer(idprop), c_void_p)

        prop2 = VkPhysicalDeviceProperties2()
        prop2.sType = VkStructureType.PHYSICAL_DEVICE_PROPERTIES_2.value
        prop2.pNext = cast(pointer(driver_props), c_void_p)

        vkGetPhysicalDeviceProperties2(self._handle, prop2)

        return (
            VulkanPhysicalDeviceProperties.from_native_obj(prop2.properties),
            VulkanPhysicalDeviceDriverProperties.from_native_obj(driver_props),
            VulkanPhysicalDeviceIDProperties.from_native_obj(idprop)
        )

    def get_features(self) -> VulkanPhysicalDeviceFeatures:
        features14 = VkPhysicalDeviceVulkan14Features()
        features14.sType = VkStructureType.PHYSICAL_DEVICE_VULKAN_1_4_FEATURES.value
        features14.pNext = c_void_p()

        features13 = VkPhysicalDeviceVulkan13Features()
        features13.sType = VkStructureType.PHYSICAL_DEVICE_VULKAN_1_3_FEATURES.value
        features13.pNext = cast(pointer(features14), c_void_p)

        features12 = VkPhysicalDeviceVulkan12Features()
        features12.sType = VkStructureType.PHYSICAL_DEVICE_VULKAN_1_2_FEATURES.value
        features12.pNext = cast(pointer(features13), c_void_p)

        features11 = VkPhysicalDeviceVulkan11Features()
        features11.sType = VkStructureType.PHYSICAL_DEVICE_VULKAN_1_1_FEATURES.value
        features11.pNext = cast(pointer(features12), c_void_p)

        features = VkPhysicalDeviceFeatures2()
        features.sType = VkStructureType.PHYSICAL_DEVICE_FEATURES_2.value
        features.pNext = cast(pointer(features11), c_void_p)
        vkGetPhysicalDeviceFeatures2(self._handle, features)

        native_features = map(VulkanPhysicalDeviceFeatures.from_native_obj, (features.features, features11, features12, features13, features14))
        return reduce(lambda x, y: x | y, native_features)

    def get_extensions(self) -> list[VulkanExtensionProperties]:
        count = c_uint32()
        vkEnumerateDeviceExtensionProperties(self._handle, c_char_p(), pointer(count), cast(c_void_p(), POINTER(VkExtensionProperties)))
        properties = (VkExtensionProperties * count.value)()
        vkEnumerateDeviceExtensionProperties(self._handle, c_char_p(), count, properties)
        return [VulkanExtensionProperties.from_native_obj(p) for p in properties]


@dataclass
class VulkanInstanceCreateInfo:
    # flags: int
    # applicationInfo
    # enabledLayers
    # enabledExtensions

    @property
    def _as_parameter_(self):
        info = VkApplicationInfo()
        info.sType = VkStructureType.APPLICATION_INFO.value
        info.apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0)
        return byref(VkInstanceCreateInfo(
            sType=VkStructureType.INSTANCE_CREATE_INFO.value,
            pApplicationInfo=pointer(info)
        ))

    def from_param(self):
        return self._as_parameter_


class VulkanInstance:
    def __init__(self, handle: VkInstance | None = None):
        if handle is None:
            handle = c_void_p()
            info = VulkanInstanceCreateInfo()
            vkCreateInstance(info, None, handle)

        self._handle = handle

    def __enter__(self) -> Self:
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.destroy()

    def destroy(self) -> None:
        vkDestroyInstance(self._handle, None)

    def enumerate_physical_devices(self, num: int | None = None) -> Generator[VulkanPhysicalDevice]:
        num = c_uint32(num or 0)
        if not num:
            vkEnumeratePhysicalDevices(self._handle, num, None)

        physical_devices = (VkPhysicalDevice * num.value)()
        vkEnumeratePhysicalDevices(self._handle, num, physical_devices)
        for device_handle in physical_devices:
            yield VulkanPhysicalDevice(device_handle)
