# Vulkan-Memory
A little memory manager for Vulkan API, written in c++.

# A simple example
## The Allocator
Firstly, create a new *Allocator* object :
```c++
//previously initialized
vk::PhysicalDevice physicalDevice;
vk::Device device;

vkm::Allocator allocator(physicalDevice, device);
```
The *allocator* is automatically destroyed.

## The Pool
For now, the only *Pool* that can be created is the *BufferPool*. The *BufferPoolCreateInfo* structure informs the size of the pool and the usage of all buffers created by this.
Note : the size of the *Pool* after creation is not necessary the size requested but the *Pool* assure that the size cannot be smaller.
```c++
struct Uniform {
  float intensity;
};

vk::DeviceSize poolSize = sizeof(Uniform);

vkm::BufferPool pool(allocator, { poolSize, vk::BufferUsageFlagBits::eUniformBuffer }, vk::MemoryPropertyFlagBits::eHostVisible |  vk::MemoryPropertyFlagBits::eHostCoherent);
```
At the end of its use, you can simply *destroy* the pool.
```c++
pool.destroy();
```

## The Buffer
The *BufferCreateInfo* structure is based on the same *vk::BufferCreateInfo* without flags and usages (usages are already defined in the pool). You can destroy the buffer like *Pool* objects but before the pool that allocated it.
Note : the real size of the buffer is set by vulkan with *vk::MemoryRequirements* and maybe it's different from the parameter. To get this new size, you can call the *getBlock()* function.
```c++
vkm::Buffer buffer(pool, { sizeof(Uniform), vk::SharingMode::eExclusive, 0, nullptr });

//using buffer

buffer.destroy();
//pool.destroy();
```
## Use buffer and pool
The most interesting thing would be to copy data into the new buffer. For a single copy, you can use this function :
```c++
Uniform uniform = { 0.4f };

pool.singleCopy(buffer, &uniform, 0, sizeof(Uniform));
```
Note : offset is relative to the buffer.

For more complex uses :
```c++
struct Color {
  float r = 0.0f, g = 0.0f, b = 0.0f;
} color;

struct LightPos {
  float x = 0.0f, y = 0.0f, z = 0.0f;
} light;

color = { 0.2f, 0.8f, 0.7f };
lightPos = { 20.0f, 4.0f, -7.0f };

buffer.create(pool, { sizeof(Color) + sizeof(LightPos) });

void* colorData = pool.map(buffer, 0, sizeof(Color));
memcpy(colorData, &color, sizeof(Color));
void* lightData = pool.map(buffer, sizeof(Color), sizeof(LightPos));
memcpy(lightData, &lightData, sizeof(LightPos));
pool.unmap();
```
Finally, to access to the *vk::Buffer*, you can cast *vkm::Buffer* objects or do nothing !
```c++
vkm::Buffer modelData;
vk::Buffer buffers[] = { static_cast<vk::Buffer>(modelData) };

//...

vkm::Buffer uniformData;
vk::DescriptorBufferInfo bufferInfo = { uniformData, 0, sizeof(float) };
```

## Pool informations
To control the use of the pool, you can call "getInformations()" :
```c++
std::string infos = pool.getInformations();
std::cout << infos << std::endl;
```
And see something like this :
```
VulkanMemory Pool (num blocks = 3) :
-Block -> offset : 0, size : 58, reserved
-Block -> offset : 58, size : 2, free
-Block -> offset : 60, size : 40, reserved
```
