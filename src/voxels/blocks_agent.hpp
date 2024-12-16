#pragma once

#include "voxel.hpp"
#include "Block.hpp"
#include "Chunk.hpp"
#include "Chunks.hpp"
#include "GlobalChunks.hpp"
#include "constants.hpp"
#include "typedefs.hpp"
#include "content/Content.hpp"
#include "maths/voxmaths.hpp"

#include <set>
#include <stdexcept>
#include <glm/glm.hpp>

/// Using templates to minimize OOP overhead

namespace blocks_agent {

template<class Storage>
inline Chunk* get_chunk(const Storage& chunks, int cx, int cz) {
    return chunks.getChunk(cx, cz);
}

template<class Storage>
inline voxel* get(const Storage& chunks, int32_t x, int32_t y, int32_t z) {
    if (y < 0 || y >= CHUNK_H) {
        return nullptr;
    }
    int cx = floordiv<CHUNK_W>(x);
    int cz = floordiv<CHUNK_D>(z);
    Chunk* chunk = get_chunk(chunks, cx, cz);
    if (chunk == nullptr) {
        return nullptr;
    }
    int lx = x - cx * CHUNK_W;
    int lz = z - cz * CHUNK_D;
    return &chunk->voxels[(y * CHUNK_D + lz) * CHUNK_W + lx];
}

template<class Storage>
inline voxel& require(const Storage& chunks, int32_t x, int32_t y, int32_t z) {
    auto vox = get(chunks, x, y, z);
    if (vox == nullptr) {
        throw std::runtime_error("voxel does not exist");
    }
    return *vox;
}

template<class Storage>
inline const Block& get_block_def(const Storage& chunks, blockid_t id) {
    return chunks.getContentIndices().blocks.require(id);
}

template<class Storage>
inline bool is_solid_at(const Storage& chunks, int32_t x, int32_t y, int32_t z) {
    if (auto vox = get(chunks, x, y, z)) {
        return get_block_def(chunks, vox->id).rt.solid;
    }
    return false;
}

template<class Storage>
inline bool is_replaceable_at(const Storage& chunks, int32_t x, int32_t y, int32_t z) {
    if (auto vox = get(chunks, x, y, z)) {
        return get_block_def(chunks, vox->id).replaceable;
    }
    return false;
}

void set(
    Chunks& chunks,
    int32_t x,
    int32_t y,
    int32_t z,
    uint32_t id,
    blockstate state
);

void set(
    GlobalChunks& chunks,
    int32_t x,
    int32_t y,
    int32_t z,
    uint32_t id,
    blockstate state
);

template<class Storage>
inline void erase_segments(
    Storage& chunks, const Block& def, blockstate state, int x, int y, int z
) {
    const auto& rotation = def.rotations.variants[state.rotation];
    for (int sy = 0; sy < def.size.y; sy++) {
        for (int sz = 0; sz < def.size.z; sz++) {
            for (int sx = 0; sx < def.size.x; sx++) {
                if ((sx | sy | sz) == 0) {
                    continue;
                }
                glm::ivec3 pos(x, y, z);
                pos += rotation.axisX * sx;
                pos += rotation.axisY * sy;
                pos += rotation.axisZ * sz;
                set(chunks, pos.x, pos.y, pos.z, 0, {});
            }
        }
    }
}

static constexpr uint8_t segment_to_int(int sx, int sy, int sz) {
    return ((sx > 0) | ((sy > 0) << 1) | ((sz > 0) << 2));
}

template <class Storage>
inline void repair_segments(
    Storage& chunks, const Block& def, blockstate state, int x, int y, int z
) {
    const auto& rotation = def.rotations.variants[state.rotation];
    const auto id = def.rt.id;
    const auto size = def.size;
    for (int sy = 0; sy < size.y; sy++) {
        for (int sz = 0; sz < size.z; sz++) {
            for (int sx = 0; sx < size.x; sx++) {
                if ((sx | sy | sz) == 0) {
                    continue;
                }
                blockstate segState = state;
                segState.segment = segment_to_int(sx, sy, sz);

                glm::ivec3 pos(x, y, z);
                pos += rotation.axisX * sx;
                pos += rotation.axisY * sy;
                pos += rotation.axisZ * sz;
                set(chunks, pos.x, pos.y, pos.z, id, segState);
            }
        }
    }
}

template <class Storage>
inline glm::ivec3 seek_origin(
    Storage& chunks, const glm::ivec3& srcpos, const Block& def, blockstate state
) {
    auto pos = srcpos;
    const auto& rotation = def.rotations.variants[state.rotation];
    auto segment = state.segment;
    while (true) {
        if (!segment) {
            return pos;
        }
        if (segment & 1) pos -= rotation.axisX;
        if (segment & 2) pos -= rotation.axisY;
        if (segment & 4) pos -= rotation.axisZ;

        if (auto* voxel = get(chunks, pos.x, pos.y, pos.z)) {
            segment = voxel->state.segment;
        } else {
            return pos;
        }
    }
}

template <class Storage>
inline bool check_replaceability(
    const Storage& chunks,
    const Block& def,
    blockstate state,
    const glm::ivec3& origin,
    blockid_t ignore
) {
    const auto& blocks = chunks.getContentIndices().blocks;
    const auto& rotation = def.rotations.variants[state.rotation];
    const auto size = def.size;
    for (int sy = 0; sy < size.y; sy++) {
        for (int sz = 0; sz < size.z; sz++) {
            for (int sx = 0; sx < size.x; sx++) {
                auto pos = origin;
                pos += rotation.axisX * sx;
                pos += rotation.axisY * sy;
                pos += rotation.axisZ * sz;
                if (auto vox = get(chunks, pos.x, pos.y, pos.z)) {
                    auto& target = blocks.require(vox->id);
                    if (!target.replaceable && vox->id != ignore) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        }
    }
    return true;
}

/// @brief Set rotation to an extended block
/// @tparam Storage chunks storage
/// @param def block definition
/// @param state current block state
/// @param origin extended block origin
/// @param index target rotation index
template <class Storage>
inline void set_rotation_extended(
    Storage& chunks,
    const Block& def,
    blockstate state,
    const glm::ivec3& origin,
    uint8_t index
) {
    auto newstate = state;
    newstate.rotation = index;

    // unable to rotate block (cause: obstacles)
    if (!check_replaceability(chunks, def, newstate, origin, def.rt.id)) {
        return;
    }

    const auto& rotation = def.rotations.variants[index];
    const auto size = def.size;
    std::vector<glm::ivec3> segmentBlocks;

    for (int sy = 0; sy < size.y; sy++) {
        for (int sz = 0; sz < size.z; sz++) {
            for (int sx = 0; sx < size.x; sx++) {
                auto pos = origin;
                pos += rotation.axisX * sx;
                pos += rotation.axisY * sy;
                pos += rotation.axisZ * sz;

                blockstate segState = newstate;
                segState.segment = segment_to_int(sx, sy, sz);

                auto vox = get(chunks, pos.x, pos.y, pos.z);
                // checked for nullptr by checkReplaceability
                if (vox->id != def.rt.id) {
                    set(chunks, pos.x, pos.y, pos.z, def.rt.id, segState);
                } else {
                    vox->state = segState;
                    int cx = floordiv<CHUNK_W>(pos.x);
                    int cz = floordiv<CHUNK_D>(pos.z);
                    auto chunk = get_chunk(chunks, cx, cz);
                    assert(chunk != nullptr);
                    chunk->setModifiedAndUnsaved();
                    segmentBlocks.emplace_back(pos);
                }
            }
        }
    }
    const auto& prevRotation = def.rotations.variants[state.rotation];
    for (int sy = 0; sy < size.y; sy++) {
        for (int sz = 0; sz < size.z; sz++) {
            for (int sx = 0; sx < size.x; sx++) {
                auto pos = origin;
                pos += prevRotation.axisX * sx;
                pos += prevRotation.axisY * sy;
                pos += prevRotation.axisZ * sz;
                if (std::find(
                        segmentBlocks.begin(), segmentBlocks.end(), pos
                    ) == segmentBlocks.end()) {
                    set(chunks, pos.x, pos.y, pos.z, 0, {});
                }
            }
        }
    }
}

template <class Storage>
inline void set_rotation(
    Storage& chunks, int32_t x, int32_t y, int32_t z, uint8_t index
) {
    if (index >= BlockRotProfile::MAX_COUNT) {
        return;
    }
    auto vox = get(chunks, x, y, z);
    if (vox == nullptr) {
        return;
    }
    const auto& def = chunks.getContentIndices().blocks.require(vox->id);
    if (!def.rotatable || vox->state.rotation == index) {
        return;
    }
    if (def.rt.extended) {
        auto origin = seek_origin(chunks, {x, y, z}, def, vox->state);
        vox = get(chunks, origin.x, origin.y, origin.z);
        set_rotation_extended(chunks, def, vox->state, origin, index);
    } else {
        vox->state.rotation = index;
        int cx = floordiv<CHUNK_W>(x);
        int cz = floordiv<CHUNK_D>(z);
        auto chunk = get_chunk(chunks, cx, cz);
        assert(chunk != nullptr);
        chunk->setModifiedAndUnsaved();
    }
}

voxel* raycast(
    const Chunks& chunks,
    const glm::vec3& start,
    const glm::vec3& dir,
    float maxDist,
    glm::vec3& end,
    glm::ivec3& norm,
    glm::ivec3& iend,
    std::set<blockid_t> filter
);

voxel* raycast(
    const GlobalChunks& chunks,
    const glm::vec3& start,
    const glm::vec3& dir,
    float maxDist,
    glm::vec3& end,
    glm::ivec3& norm,
    glm::ivec3& iend,
    std::set<blockid_t> filter
);

} // blocks_agent
