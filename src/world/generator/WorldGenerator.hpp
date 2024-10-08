#pragma once

#include <array>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "constants.hpp"
#include "typedefs.hpp"
#include "voxels/voxel.hpp"
#include "SurroundMap.hpp"
#include "StructurePlacement.hpp"

class Content;
struct GeneratorDef;
class Heightmap;
struct Biome;
class VoxelFragment;

enum class ChunkPrototypeLevel {
    VOID=0, BIOMES, HEIGHTMAP, STRUCTURES
};

struct ChunkPrototype {
    ChunkPrototypeLevel level = ChunkPrototypeLevel::VOID;

    /// @brief chunk biomes matrix
    std::unique_ptr<const Biome*[]> biomes;

    /// @brief chunk heightmap
    std::shared_ptr<Heightmap> heightmap;

    std::vector<StructurePlacement> structures;

    std::vector<LinePlacement> lines;
};

struct WorldGenDebugInfo {
    int areaOffsetX;
    int areaOffsetY;
    uint areaWidth;
    uint areaHeight;
    std::unique_ptr<ubyte[]> areaLevels;
};

/// @brief High-level world generation controller
class WorldGenerator {
    /// @param def generator definition
    const GeneratorDef& def;
    /// @param content world content
    const Content* content;
    /// @param seed world seed
    uint64_t seed;
    /// @brief Chunk prototypes main storage
    std::unordered_map<glm::ivec2, std::unique_ptr<ChunkPrototype>> prototypes;
    /// @brief Chunk prototypes loading surround map
    SurroundMap surroundMap;

    /// @brief Generate chunk prototype (see ChunkPrototype)
    /// @param x chunk position X divided by CHUNK_W
    /// @param z chunk position Y divided by CHUNK_D
    std::unique_ptr<ChunkPrototype> generatePrototype(int x, int z);

    ChunkPrototype& requirePrototype(int x, int z);

    void generateStructures(ChunkPrototype& prototype, int x, int z);

    void generateBiomes(ChunkPrototype& prototype, int x, int z);

    void generateHeightmap(ChunkPrototype& prototype, int x, int z);

    void placeStructure(
        const glm::ivec3& offset, size_t structure, uint8_t rotation,
        int chunkX, int chunkZ
    );

    void placeLine(const LinePlacement& line);

    void generateLines(
        const ChunkPrototype& prototype, voxel* voxels, int x, int z
    );
public:
    WorldGenerator(
        const GeneratorDef& def,
        const Content* content,
        uint64_t seed
    );
    ~WorldGenerator();

    void update(int centerX, int centerY, int loadDistance);

    /// @brief Generate complete chunk voxels
    /// @param voxels destinatiopn chunk voxels buffer
    /// @param x chunk position X divided by CHUNK_W
    /// @param z chunk position Y divided by CHUNK_D
    void generate(voxel* voxels, int x, int z);

    WorldGenDebugInfo createDebugInfo() const;

    /// @brief Default generator name // TODO: move to config
    inline static std::string DEFAULT = "core:default";
};
