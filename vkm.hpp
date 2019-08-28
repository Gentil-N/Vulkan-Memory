#pragma once
#include <vulkan/vulkan.hpp>




namespace vkm {




	enum class Size : uint32_t {

		Ko = 1000,
		Mo = 1000000,
		Go = 1000000000
	};




	struct Block {

		vk::DeviceSize size = 0, offset = 0;

		bool isFree = false;
	};




	class Allocator {


	public :


		Allocator();

		Allocator(vk::PhysicalDevice physicalDevice, vk::Device device);

		void create(vk::PhysicalDevice physicalDevice, vk::Device device);
		
		inline vk::PhysicalDevice getPhysicalDevice() const noexcept{
			return _physicalDevice;
		}

		inline const vk::PhysicalDeviceMemoryProperties& getMemoryProperties() const noexcept {
			return _memoryProperties;
		}

		inline vk::Device getDevice() const noexcept {
			return _device;
		}


	private :

		vk::PhysicalDevice _physicalDevice;

		vk::PhysicalDeviceMemoryProperties _memoryProperties;

		vk::Device _device;
	};




	class IGetBlock {

	public :

		virtual Block getBlock() const noexcept = 0;
	};




	class Pool {


	public:

		Pool();

		Block alloc(vk::DeviceSize size, vk::DeviceSize alignment);

		void free(Block& block);

		virtual void destroy() = 0;

		std::string getInformations() const noexcept;

		inline void* map(const IGetBlock& memObject, vk::DeviceSize offset, vk::DeviceSize size) const {
			return _allocator->getDevice().mapMemory(_memory, memObject.getBlock().offset + offset, size);
		}

		void singleCopy(const IGetBlock& memObject, const void* data, vk::DeviceSize offset, vk::DeviceSize size);

		inline void unmap() const {
			_allocator->getDevice().unmapMemory(_memory);
		}

		inline vk::DeviceSize getSize() const noexcept {
			return _size;
		}

		inline uint32_t getMemoryType() const noexcept {
			return _memoryType;
		}

		inline vk::Device getDevice() const noexcept {
			return _allocator->getDevice();
		}

	protected:

		void _create();

		void _destroy();

		uint32_t _findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

		vk::DeviceMemory _memory;

		std::vector<Block> _blocks;

		vk::DeviceSize _size;

		uint32_t _memoryType;

		Allocator* _allocator;
	};




	struct BufferPoolCreateInfo {

		vk::DeviceSize size = 0;

		vk::BufferUsageFlags usage = vk::BufferUsageFlags();
	};




	class BufferPool : public Pool {


	public:

		BufferPool();

		BufferPool(Allocator& allocator, const BufferPoolCreateInfo& poolInfo, vk::MemoryPropertyFlags memoryProperties);

		void create(Allocator& allocator, const BufferPoolCreateInfo& poolInfo, vk::MemoryPropertyFlags memoryProperties);

		void destroy() override;

		inline void bindBuffer(vk::Buffer buffer, const Block& block) const {
			_allocator->getDevice().bindBufferMemory(buffer, _memory, block.offset);
		}

		inline vk::BufferUsageFlags getUsages() const noexcept {
			return _usage;
		}

		inline vk::MemoryPropertyFlags getMemoryProperties() const noexcept {
			return _memoryProperties;
		}

	protected:

		void _setMemoryRequirements();

		vk::BufferUsageFlags _usage;

		vk::MemoryPropertyFlags _memoryProperties;
	};




	struct BufferCreateInfo {

		vk::DeviceSize size = 0;

		vk::SharingMode sharingMode = vk::SharingMode::eExclusive;

		uint32_t queueFamilyIndexCount = 0;

		const uint32_t* pQueueFamilyIndices = nullptr;
	};




	class Buffer : public IGetBlock {


	public:

		Buffer();

		Buffer(BufferPool& pool, const BufferCreateInfo& bufferInfo);

		void create(BufferPool& pool, const BufferCreateInfo& bufferInfo);

		void destroy();

		Block getBlock() const noexcept override;

		operator vk::Buffer();

	private:

		vk::Buffer _buffer;

		Block _block;

		BufferPool* _pool;
	};
}