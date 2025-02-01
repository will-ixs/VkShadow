Vulkan Shadowmapping\
This project is a reimplementation of one of my class projects using Vulkan. 


Requires features from Vulkan 1.3 (Dynamic Rendering, Synchronization2, Buffer Device Adress, Descriptor Indexing).

Shadow Mapping: Rendering the scene from the light's point of view and saving the depth information to a texture. When rendering the final image, reprojecting each pixel into the camera's view space and comparing that pixel's depth to the one stored in the first depth pass. 

Asynchronous Mesh Uploading: Meshes are uploaded in a background thread, using a separate queue family and transfer queue.

<br>

Based on [vkguide](https://vkguide.dev): Inspired by vkguide by vblanco, following up to around Chapter 3 before branching off to focus on shadow mapping.

<br>

This project was started without a build system, as it was just intended to learn. There is a release available if you just want to run it.


This visual studio project uses environment variables for the includes and linking, \
STB_DIR points to stb, a folder containing [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h) \
TINY_OBJLOADER points to a folder containing [tiny_obj_loader.h](https://github.com/tinyobjloader/tinyobjloader/blob/release/tiny_obj_loader.h) \
GLFW_BIN_DIR points to a folder containing the pre-compiled [GLFW binaries for Windows](https://www.glfw.org/download.html) \
GLM_INCLUDE_DIR pooints the base folder for the [glm repo](https://github.com/g-truc/glm) \
VMA_INCLUDE_DIR points to the include folder in the [Vulkan Memory Allocator repo](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) \
VULKAN_DIR points to the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) 

<br>

Thank you to:\
vkguide by vblanco for a structured introduction to Vulkan.\
The Vulkan community discord for extensive resources and support.
