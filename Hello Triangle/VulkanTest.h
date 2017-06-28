#pragma once

#ifdef NDEBUG
# define ENABLE_VALIDATION_LAYER false
# define FRAME_CAP_ENABLE true
#else
#ifdef _NO_FRAME_CAP
#  define FRAME_CAP_ENABLE false
# else
#  define FRAME_CAP_ENABLE true
#endif
# define ENABLE_VALIDATION_LAYER true
#endif

struct		QueueFamilyIndices
{
	int		graphicsFamily = -1;
	int		presentFamily = -1;

	bool	isComplete()
	{
		return (graphicsFamily >= 0 && presentFamily >= 0);
	}
};

struct							SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR		capabilities;
	std::vector<VkSurfaceFormatKHR>	formats;
	std::vector<VkPresentModeKHR>	presentModes;
};