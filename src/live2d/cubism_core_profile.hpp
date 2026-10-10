#pragma once

#include <GL/glew.h>
#include <array>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace bongo_cat {

struct CoreProfileBuffers {
    GLuint vao = 0;
    GLuint buffers[3] = {};
    GLuint arena[3] = {};
    struct Range { size_t offset, size; uint64_t epoch; };
    uint64_t epoch = 0;
    std::array<std::unordered_map<const void *, Range>, 3> ranges;
    std::vector<float> positions, uv;
    std::vector<GLushort> indices;
    std::array<bool, 3> valid{}, dirty{};
    size_t vertex_cursor = 0, index_cursor = 0;
    const void *static_sources[2] = {};
    GLsizeiptr static_sizes[2] = {};

    CoreProfileBuffers() = default;
    CoreProfileBuffers(const CoreProfileBuffers &) = delete;
    CoreProfileBuffers &operator=(const CoreProfileBuffers &) = delete;

    void release() {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(3, buffers);
        glDeleteBuffers(3, arena);
        for (auto &buffer : arena) buffer = 0;
        for (auto &items : ranges) {
            std::unordered_map<const void *, Range> empty;
            items.swap(empty);
        }
        std::vector<float>().swap(positions);
        std::vector<float>().swap(uv);
        std::vector<GLushort>().swap(indices);
        valid = {};
        vao = 0;
        for (auto &buffer : buffers) buffer = 0;
        for (auto &source : static_sources) source = nullptr;
        for (auto &size : static_sizes) size = 0;
    }
};

// SDK callbacks use the buffers of the model currently being drawn.
class CoreProfileBinding {
public:
    explicit CoreProfileBinding(CoreProfileBuffers &resources)
        : resources_(resources), previous_(active_) {
        enabled_ = GLEW_VERSION_3_0 || GLEW_ARB_vertex_array_object;
        if (!enabled_) return;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array_);
        if (!resources_.vao) glGenVertexArrays(1, &resources_.vao);
        if (!resources_.buffers[0]) glGenBuffers(3, resources_.buffers);
        glBindVertexArray(resources_.vao);
        active_ = this;
    }

    ~CoreProfileBinding() {
        if (!enabled_) return;
        glBindVertexArray((GLuint)previous_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previous_array_);
        active_ = previous_;
    }

    CoreProfileBinding(const CoreProfileBinding &) = delete;
    CoreProfileBinding &operator=(const CoreProfileBinding &) = delete;

    static void attribute(unsigned slot, GLuint location,
        const void *data, GLsizeiptr size) {
        if (!active_) {
            glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, data);
            return;
        }
        const auto &ranges = active_->resources_.ranges[slot];
        const auto found = ranges.find(data);
        if (found != ranges.end() && found->second.epoch == active_->resources_.epoch && found->second.size == (size_t)size) {
            glBindBuffer(GL_ARRAY_BUFFER, active_->resources_.arena[slot]);
            glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2,
                reinterpret_cast<const void *>(found->second.offset));
            return;
        }
        glBindBuffer(GL_ARRAY_BUFFER, active_->resources_.buffers[slot]);
        // Core mesh UVs and SDK quad UVs are immutable. Positions still stream.
        if (slot == 1) upload_static(GL_ARRAY_BUFFER, 0, data, size);
        else upload_stream(GL_ARRAY_BUFFER, size, data);
        glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE,
            sizeof(GLfloat) * 2, nullptr);
    }

    static void draw(GLsizei count, const GLushort *indices) {
        if (!active_) { glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices); return; }
        const auto &ranges = active_->resources_.ranges[2];
        const auto found = ranges.find(indices);
        if (found != ranges.end() && found->second.epoch == active_->resources_.epoch && found->second.size == (size_t)count * sizeof(GLushort)) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, active_->resources_.arena[2]);
            glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT,
                reinterpret_cast<const void *>(found->second.offset));
            return;
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, active_->resources_.buffers[2]);
        upload_static(GL_ELEMENT_ARRAY_BUFFER, 1, indices,
            (GLsizeiptr)count * sizeof(GLushort));
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, nullptr);
    }

    static void prepare_begin(size_t vertices, size_t indices) {
        if (!active_) return;
        auto &r = active_->resources_;
        ++r.epoch;
        r.vertex_cursor = r.index_cursor = 0;
        if (r.positions.size() != vertices * 2 || r.indices.size() != indices) {
            r.positions.resize(vertices * 2); r.uv.resize(vertices * 2); r.indices.resize(indices);
            r.valid = {};
            for (auto &items : r.ranges) items.clear();
        }
        for (size_t i = 0; i < 3; ++i) r.dirty[i] = !r.valid[i];
        if (!r.arena[0]) glGenBuffers(3, r.arena);
    }
    static void prepare_mesh(const float *positions, const float *uv, size_t count,
        const GLushort *indices, size_t index_count) {
        if (!active_) return;
        auto &r = active_->resources_;
        const size_t bytes = count * sizeof(float) * 2, offset = r.vertex_cursor * sizeof(float);
        if (bytes) {
            float *targets[] = {r.positions.data() + r.vertex_cursor, r.uv.data() + r.vertex_cursor};
            const float *sources[] = {positions, uv};
            for (size_t i = 0; i < 2; ++i) {
                if (!r.valid[i] || std::memcmp(targets[i], sources[i], bytes) != 0) {
                    std::memcpy(targets[i], sources[i], bytes); r.dirty[i] = true;
                }
                r.ranges[i][sources[i]] = {offset, bytes, r.epoch};
            }
        }
        const size_t index_bytes = index_count * sizeof(GLushort);
        if (index_bytes) {
            auto *target = r.indices.data() + r.index_cursor;
            if (!r.valid[2] || std::memcmp(target, indices, index_bytes) != 0) {
                std::memcpy(target, indices, index_bytes); r.dirty[2] = true;
            }
            r.ranges[2][indices] = {r.index_cursor * sizeof(GLushort), index_bytes, r.epoch};
        }
        r.vertex_cursor += count * 2; r.index_cursor += index_count;
    }
    static void prepare_end() {
        if (!active_) return;
        auto &r = active_->resources_;
        const void *data[] = {r.positions.data(), r.uv.data(), r.indices.data()};
        const size_t sizes[] = {r.positions.size() * sizeof(float), r.uv.size() * sizeof(float),
            r.indices.size() * sizeof(GLushort)};
        for (size_t i = 0; i < 3; ++i) {
            if (!r.dirty[i] || !sizes[i]) continue;
            const GLenum target = i == 2 ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
            glBindBuffer(target, r.arena[i]);
            upload_stream(target, (GLsizeiptr)sizes[i], data[i]);
            if (glGetError() != GL_NO_ERROR) {
                r.valid = {};
                throw std::runtime_error("Cannot upload OpenGL frame geometry");
            }
            r.valid[i] = true;
        }
    }
private:
    static void upload_stream(GLenum target, GLsizeiptr size, const void *data) {
        // Orphan before writing so earlier draws keep their old storage.
        glBufferData(target, size, nullptr, GL_STREAM_DRAW);
        if (size <= 0) return;
        void *mapped = nullptr;
        if (GLEW_VERSION_3_0 || GLEW_ARB_map_buffer_range)
            mapped = glMapBufferRange(target, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (mapped) {
            std::memcpy(mapped, data, (size_t)size);
            if (glUnmapBuffer(target)) return;
        }
        glBufferSubData(target, 0, size, data);
    }
    static void upload_static(GLenum target, unsigned slot,
        const void *source, GLsizeiptr size) {
        auto &resources = active_->resources_;
        // Cache only the contents of the existing buffer, not an unbounded set
        // of mesh buffers. Rebinding after SDK PreDraw is still mandatory.
        if (resources.static_sources[slot] == source &&
            resources.static_sizes[slot] == size) return;
        glBufferData(target, size, source, GL_STREAM_DRAW);
        resources.static_sources[slot] = source;
        resources.static_sizes[slot] = size;
    }

    CoreProfileBuffers &resources_;
    CoreProfileBinding *previous_;
    bool enabled_ = false;
    GLint previous_vao_ = 0;
    GLint previous_array_ = 0;
    inline static thread_local CoreProfileBinding *active_ = nullptr;
};

} // namespace bongo_cat
