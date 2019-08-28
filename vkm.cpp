#include "vkm.hpp"




#include <exception>
#include <iostream>




namespace vkm {




	Allocator::Allocator() : _physicalDevice(), _device(), _memoryProperties() {

	}


	Allocator::Allocator(vk::PhysicalDevice physicalDevice, vk::Device device) : _physicalDevice(physicalDevice), _device(device) {
		_memoryProperties = _physicalDevice.getMemoryProperties();
	}


	void Allocator::create(vk::PhysicalDevice physicalDevice, vk::Device device) {
		_physicalDevice = physicalDevice;
		_device = device;
		_memoryProperties = _physicalDevice.getMemoryProperties();
	}




	Pool::Pool() : _memory(vk::DeviceMemory()), _blocks(std::vector<Block>()), _size(0), _memoryType(0), _allocator(nullptr) {

	}


	Block Pool::alloc(vk::DeviceSize size, vk::DeviceSize alignment) {

		if (_size < size)//si la taille d'allocation est plus petite que le pool
			throw std::exception("VkmError : failed to allocate block -> size too large");

		for (size_t blockIndex = 0; blockIndex < _blocks.size(); ++blockIndex) {
			
			if (!_blocks[blockIndex].isFree)//si non-libre
				continue;
			
			vk::DeviceSize shift = _blocks[blockIndex].offset % alignment == 0 
				? 0 : alignment - _blocks[blockIndex].offset % alignment;

			if (_blocks[blockIndex].size - shift < size) {
				++blockIndex;//on peut sauter celui d'après : la "fusion" permet de ne pas avoir de blocks libres adjacents
				continue;
			}

			else if (_blocks[blockIndex].size - shift > size) {

				_blocks.insert(
					_blocks.begin() + blockIndex + 1,
					{ _blocks[blockIndex].size - (shift + size), _blocks[blockIndex].offset + shift + size, true }
				);
			}

			_blocks[blockIndex].size = size;
			_blocks[blockIndex].isFree = false;

			if (shift > 0) {
				Block shiftBlock = { shift, _blocks[blockIndex].offset, true };
				_blocks[blockIndex].offset += shift;
				_blocks.insert(_blocks.begin() + blockIndex, shiftBlock);
				return _blocks[blockIndex + 1];
			}
			else
				return _blocks[blockIndex];

		}
		throw std::exception("VkmError : failed to allocate block");
	}


	void Pool::free(Block& block) {

		for (size_t blockIndex = 0; blockIndex < _blocks.size(); ++blockIndex) {

			if (_blocks[blockIndex].isFree)
				continue;

			else if (_blocks[blockIndex].offset < block.offset)
				continue;

			else if (_blocks[blockIndex].offset > block.offset)//on l'a dépassé, c'est mort !
				break;

			if (_blocks[blockIndex].size != block.size)//si les blocks n'ont pas la même taille
				throw std::exception("VkmError : failed to deallocate memory -> is not the same size");

			_blocks[blockIndex].isFree = true;

			if (
				blockIndex > 0 && blockIndex < _blocks.size() - 1 && 
				_blocks[blockIndex - 1].isFree && _blocks[blockIndex + 1].isFree) {

				_blocks[blockIndex - 1].size += _blocks[blockIndex].size + _blocks[blockIndex + 1].size;
				_blocks.erase(_blocks.begin() + blockIndex);
				_blocks.erase(_blocks.begin() + blockIndex);
			}

			else if (blockIndex > 0 && _blocks[blockIndex - 1].isFree) {

				_blocks[blockIndex - 1].size += _blocks[blockIndex].size;
				_blocks.erase(_blocks.begin() + blockIndex);
			}

			else if (blockIndex < _blocks.size() - 1 && _blocks[blockIndex + 1].isFree) {

				_blocks[blockIndex].size += _blocks[blockIndex + 1].size;
				_blocks.erase(_blocks.begin() + blockIndex + 1);
			}

			block = { 0, 0, true };

			return;
		}

		throw std::exception("VkmError : failed to deallocate memory");
	}


	std::string Pool::getInformations() const noexcept {

		std::string informations = "VulkanMemory Pool (num blocks = " + std::to_string(_blocks.size()) + ") : \n";
		for (size_t i = 0; i < _blocks.size(); ++i) {
			informations += "-Block -> ";
			informations += "offset : " + std::to_string(_blocks[i].offset) + ", ";
			informations += "size : " + std::to_string(_blocks[i].size) + ", ";
			std::string state = _blocks[i].isFree ? "free" : "reserved";
			informations += state + "\n";
		}
		return informations;
	}


	void Pool::singleCopy(const IGetBlock& memObject, const void* data, vk::DeviceSize offset, vk::DeviceSize size) {
		void* dst = map(memObject, offset, size);
		memcpy(dst, data, static_cast<size_t>(size));
		unmap();
	}


	void Pool::_create() {

		_memory = _allocator->getDevice().allocateMemory({ _size, _memoryType });
		_blocks.push_back({ _size, 0, true });
	}


	void Pool::_destroy() {

		_allocator->getDevice().freeMemory(_memory);
		_allocator = nullptr;
		_blocks.clear();
		_size = 0;
		_memoryType = 0;
	}


	uint32_t Pool::_findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {

		for (uint32_t i = 0; i < _allocator->getMemoryProperties().memoryTypeCount; i++)

			if (
				(typeFilter & (1 << i)) && 
				(_allocator->getMemoryProperties().memoryTypes[i].propertyFlags & properties) == properties)

				return i;

		throw std::exception("VkmError : failed to find suitable memory type");
	}




	BufferPool::BufferPool() : Pool(), _usage(vk::BufferUsageFlags()), _memoryProperties(vk::MemoryPropertyFlags()) {

	}


	BufferPool::BufferPool(Allocator& allocator, const BufferPoolCreateInfo& poolInfo, vk::MemoryPropertyFlags memoryProperties) {

		create(allocator, poolInfo, memoryProperties);
	}


	void BufferPool::create(Allocator& allocator, const BufferPoolCreateInfo& poolInfo, vk::MemoryPropertyFlags memoryProperties) {

		_usage = poolInfo.usage;
		_memoryProperties = memoryProperties;

		_allocator = &allocator;
		_size = poolInfo.size;
		_setMemoryRequirements();
		_create();
	}


	void BufferPool::destroy() {

		_destroy();
		_usage = vk::BufferUsageFlags();
		_memoryProperties = vk::MemoryPropertyFlags();
	}


	void BufferPool::_setMemoryRequirements() {

		vk::BufferCreateInfo bufferInfo = { {}, _size, _usage };
		vk::Buffer buffer = _allocator->getDevice().createBuffer(bufferInfo);
		vk::MemoryRequirements memRequirements = _allocator->getDevice().getBufferMemoryRequirements(buffer);
		_size = memRequirements.size;
		_memoryType = _findMemoryType(memRequirements.memoryTypeBits, _memoryProperties);
		_allocator->getDevice().destroyBuffer(buffer);
	}




	Buffer::Buffer() : _buffer(vk::Buffer()), _block{ 0, 0, true }, _pool(nullptr) {

	}


	Buffer::Buffer(BufferPool& pool, const BufferCreateInfo& bufferInfo) {

		create(pool, bufferInfo);
	}


	void Buffer::create(BufferPool& pool, const BufferCreateInfo& bufferInfo) {

		_pool = &pool;

		vk::BufferCreateInfo vkBufferInfo =
		{
			{}, bufferInfo.size, _pool->getUsages(), bufferInfo.sharingMode, bufferInfo.queueFamilyIndexCount,
			bufferInfo.pQueueFamilyIndices
		};

		_buffer = _pool->getDevice().createBuffer(vkBufferInfo);
		vk::MemoryRequirements memRequirements = _pool->getDevice().getBufferMemoryRequirements(_buffer);
		_block = _pool->alloc(memRequirements.size, memRequirements.alignment);
		_pool->bindBuffer(_buffer, _block);
	}


	void Buffer::destroy() {
		_pool->free(_block);
		_pool->getDevice().destroyBuffer(_buffer);
		_pool = nullptr;
	}


	Block Buffer::getBlock() const noexcept {
		return _block;
	}


	Buffer::operator vk::Buffer() {
		return _buffer;
	}
}