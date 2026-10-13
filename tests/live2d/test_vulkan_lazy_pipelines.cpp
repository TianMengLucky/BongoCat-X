#include "test.h"
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
int bongo_cat_test_failures;
using csmInt32 = int;
using csmUint32 = unsigned;
using csmString = std::string;
using VkDescriptorSetLayout = unsigned;
constexpr int ShaderNames_Copy = 0, ColorBlendMode_None = 0, AlphaBlendMode_None = 0;
constexpr int ShaderNames_Normal = 2, ShaderNames_Add = 8;
constexpr int ShaderNames_MultMaskedInvertedPremultipliedAlpha = 19;
struct CubismPipeline_Vulkan {
    struct PipelineResource {
        std::atomic<int> creates{0}, releases{0};
        bool fail = false;
        unsigned last_layout = 0;
        bool CreateGraphicsPipeline(const std::string &, const std::string &,
            unsigned layout, unsigned, int, int) {
            ++creates;
            last_layout = layout;
            if (fail) throw std::runtime_error("mock partial allocation failure");
            return true;
        }
        void Release() { ++releases; }
    };
};
// Extracted verbatim from the production generated-SDK adaptation.
#include "vulkan_lazy_pipeline_test.inc"

int main() {
    CubismPipeline_Vulkan owner;
    CubismPipeline_Vulkan::PipelineResource resource;
    bongoPipelineDescriptions.emplace(BongoPipelineKey{&owner, ShaderNames_Normal},
        BongoPipelineDescription{&resource, "vertex", "fragment", 1, ShaderNames_Normal, 0, 0});
    CHECK(resource.creates == 0); // Initialization reserves no GPU pipeline.
    // Simulate reload replacing an unmaterialized descriptor layout.
    bongoPipelineDescriptions.begin()->second.layout = 2;
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) workers.emplace_back([&] {
        BongoEnsurePipeline(&owner, ShaderNames_Add, 0);
    });
    for (auto &worker : workers) worker.join();
    CHECK(resource.creates == 1 && resource.last_layout == 2);
    BongoEnsurePipeline(&owner, ShaderNames_Normal, 0);
    BongoEnsurePipeline(&owner, ShaderNames_Add + 6, 0);
    CHECK(resource.creates == 1);
    bool missing = false;
    try { BongoEnsurePipeline(&owner, ShaderNames_Normal + 1, 0); }
    catch (const std::runtime_error &) { missing = true; }
    CHECK(missing);
    bongoPipelineDescriptions.clear();
    resource.fail = true;
    bongoPipelineDescriptions.emplace(BongoPipelineKey{&owner, ShaderNames_Normal},
        BongoPipelineDescription{&resource, "vertex", "fragment", 3, ShaderNames_Normal, 0, 0});
    bool failed = false;
    try { BongoEnsurePipeline(&owner, ShaderNames_Normal, 0); }
    catch (const std::runtime_error &) { failed = true; }
    CHECK(failed && resource.releases == 1);
    CHECK(!bongoPipelineDescriptions.begin()->second.ready);
    resource.fail = false;
    BongoEnsurePipeline(&owner, ShaderNames_Normal, 0);
    CHECK(resource.creates == 3 && resource.last_layout == 3);
    CHECK(bongoPipelineDescriptions.begin()->second.ready);
    resource.fail = true;
    failed = false;
    try { BongoEnsurePipeline(&owner, ShaderNames_Normal, 2); }
    catch (const std::runtime_error &) { failed = true; }
    CHECK(failed && resource.releases == 1); // Live sibling survives.
    CHECK(bongoPipelineDescriptions.begin()->second.ready == 1);
    resource.fail = false;
    BongoEnsurePipeline(&owner, ShaderNames_Normal, 2);
    CHECK(bongoPipelineDescriptions.begin()->second.ready == 5);
    BongoEnsurePipeline(&owner, ShaderNames_Normal, 2);
    CHECK(resource.creates == 5);
    bongoPipelineDescriptions.clear();
    return bongo_cat_test_failures ? 1 : 0;
}
