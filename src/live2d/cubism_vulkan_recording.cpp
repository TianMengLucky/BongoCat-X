#include "cubism_vulkan_recording.hpp"
#include "cubism_vulkan_bindless.hpp"
#include "cubism_vulkan_submission.hpp"
#include <algorithm>
#include <array>
#include <exception>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

namespace bongo_cat {
namespace {
constexpr size_t max_workers = 4;
constexpr size_t parallel_threshold = 128;
void checked(VkResult result, const char *message) {
    if (result != VK_SUCCESS) throw std::runtime_error(message);
}
struct Worker {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable ready, finished;
    size_t count = 0;
    bool pending = false, stop = false;
    std::exception_ptr failure;
};
class Recorder final {
public:
    explicit Recorder(VkDevice device) : device_(device) {}
    ~Recorder() {
        for (auto &worker : workers_) {
            { std::lock_guard<std::mutex> lock(worker.mutex); worker.stop = true; }
            worker.ready.notify_one();
        }
        for (auto &worker : workers_) if (worker.thread.joinable()) worker.thread.join();
        // Async submissions retain these buffers until their fence. Shutdown
        // also covers an interrupted draw before releasing GPU-owned resources.
        vkDeviceWaitIdle(device_);
        for (const auto &worker : workers_)
            if (worker.pool) vkDestroyCommandPool(device_, worker.pool, nullptr);
    }
    void initialize(uint32_t family, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            VkCommandPoolCreateInfo pool{};
            pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool.queueFamilyIndex = family;
            pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            checked(vkCreateCommandPool(device_, &pool, nullptr, &workers_[i].pool),
                "Cannot create Live2D recording pool");
            VkCommandBufferAllocateInfo allocate{};
            allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate.commandPool = workers_[i].pool;
            allocate.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
            allocate.commandBufferCount = 1;
            checked(vkAllocateCommandBuffers(device_, &allocate, &workers_[i].command),
                "Cannot allocate Live2D secondary commands");
        }
        count_ = count;
        for (size_t i = 0; i < count_; ++i)
            workers_[i].thread = std::thread([this, i] {
                auto &worker = workers_[i];
                std::unique_lock<std::mutex> lock(worker.mutex);
                for (;;) {
                    worker.ready.wait(lock, [&worker] { return worker.pending || worker.stop; });
                    if (worker.stop) return;
                    const auto ranges = worker.count;
                    lock.unlock();
                    std::exception_ptr failure;
                    try { record(i, ranges); } catch (...) { failure = std::current_exception(); }
                    lock.lock();
                    worker.failure = failure; worker.pending = false;
                    worker.finished.notify_one();
                }
            });
    }
    bool begin(VkCommandBuffer primary, VkFormat color, VkFormat depth,
        VkExtent2D extent, bool compatible) {
        if (!compatible) return false;
        if (primary_) throw std::runtime_error("Nested Live2D recording pass");
        // Called after the SDK's synchronized mask submission, and before
        // recording this frame's draw pass. No previous secondary is pending.
        for (size_t i = 0; i < count_; ++i)
            checked(vkResetCommandPool(device_, workers_[i].pool, 0),
                "Cannot reset Live2D recording pool");
        draws_.clear();
        color_ = color;
        depth_ = depth;
        extent_ = extent;
        primary_ = primary;
        return true;
    }
    void cancel() {
        primary_ = VK_NULL_HANDLE;
        draws_.clear();
    }
    bool active(VkCommandBuffer primary) const {
        return primary_ && primary_ == primary;
    }
    bool draw(VkCommandBuffer primary, const VulkanDrawSnapshot &draw) {
        if (!active(primary)) return false;
        draws_.push_back(draw);
        return true;
    }
    void end(VkCommandBuffer primary) {
        if (!active(primary)) return;
        primary_ = VK_NULL_HANDLE;
        if (draws_.empty()) return;
        const size_t count = draws_.size() < parallel_threshold ? 1 : count_;
        std::exception_ptr failure;
        // Persistent workers keep separate command pools and contiguous ranges.
        if (count == 1) record(0, 1);
        else {
            for (size_t i = 0; i < count; ++i) {
                auto &worker = workers_[i];
                { std::lock_guard<std::mutex> lock(worker.mutex);
                  worker.count = count; worker.failure = nullptr; worker.pending = true; }
                worker.ready.notify_one();
            }
            for (size_t i = 0; i < count; ++i) {
                auto &worker = workers_[i];
                std::unique_lock<std::mutex> lock(worker.mutex);
                worker.finished.wait(lock, [&worker] { return !worker.pending; });
                if (!failure && worker.failure) failure = worker.failure;
            }
        }
        if (failure) std::rethrow_exception(failure);
        std::array<VkCommandBuffer, max_workers> commands{};
        for (size_t i = 0; i < count; ++i) commands[i] = workers_[i].command;
        vkCmdExecuteCommands(primary, static_cast<uint32_t>(count), commands.data());
        draws_.clear();
    }
private:
    void record(size_t worker, size_t count) const {
        VkCommandBufferInheritanceRenderingInfo rendering{};
        rendering.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &color_;
        rendering.depthAttachmentFormat = depth_;
        rendering.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkCommandBufferInheritanceInfo inheritance{};
        inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritance.pNext = &rendering;
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        begin.pInheritanceInfo = &inheritance;
        VkCommandBuffer command = workers_[worker].command;
        checked(vulkan_begin_commands(command, &begin), "Cannot begin Live2D secondary commands");
        const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent_.width),
            static_cast<float>(extent_.height), 0.0f, 1.0f};
        const VkRect2D scissor{{0, 0}, extent_};
        vkCmdSetViewport(command, 0, 1, &viewport);
        vkCmdSetScissor(command, 0, 1, &scissor);
        const size_t first = draws_.size() * worker / count;
        const size_t last = draws_.size() * (worker + 1) / count;
        for (size_t i = first; i < last; ++i) {
            const auto &draw = draws_[i];
            vulkan_set_cull(command, draw.cull);
            vkCmdBindVertexBuffers(command, 0, 1, &draw.vertices, &draw.vertex_offset);
            vkCmdBindIndexBuffer(command, draw.indices, 0, VK_INDEX_TYPE_UINT16);
            vulkan_bind_descriptors(command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                draw.layout, 0, 1, &draw.descriptor, 0, nullptr);
            vulkan_bindless_bind(command, draw.layout, draw.texture);
            vulkan_bind_pipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.pipeline);
            vkCmdDrawIndexed(command, draw.index_count, 1, 0, 0, 0);
        }
        checked(vkEndCommandBuffer(command), "Cannot end Live2D secondary commands");
    }
    VkDevice device_;
    std::array<Worker, max_workers> workers_{};
    size_t count_ = 0;
    VkCommandBuffer primary_ = VK_NULL_HANDLE;
    VkFormat color_ = VK_FORMAT_UNDEFINED, depth_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VulkanDrawSnapshot> draws_;
};
std::unique_ptr<Recorder> recorder;
}
bool vulkan_recording_configure(VkDevice device, uint32_t family, bool enabled) {
    recorder.reset();
    if (!enabled || !device) return false;
    const size_t count = std::min(max_workers,
        static_cast<size_t>(std::thread::hardware_concurrency()));
    if (count < 2) return false;
    try {
        auto candidate = std::make_unique<Recorder>(device);
        candidate->initialize(family, count);
        recorder = std::move(candidate);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}
void vulkan_recording_release() { recorder.reset(); }
void vulkan_recording_cancel() { if (recorder) recorder->cancel(); }
bool vulkan_recording_active(VkCommandBuffer primary) {
    return recorder && recorder->active(primary);
}
bool vulkan_recording_begin(VkCommandBuffer primary, VkFormat color,
    VkFormat depth, VkExtent2D extent, bool compatible) {
    return recorder && recorder->begin(primary, color, depth, extent, compatible);
}
bool vulkan_recording_draw(VkCommandBuffer primary, const VulkanDrawSnapshot &draw) {
    return recorder && recorder->draw(primary, draw);
}
void vulkan_recording_end(VkCommandBuffer primary) {
    if (recorder) recorder->end(primary);
}
}
