#include "physics/physics_layers.h"

namespace gg {

BPLayerInterfaceImpl::BPLayerInterfaceImpl() {
    mapping_[Layers::STATIC]  = BPLayers::NON_MOVING;
    mapping_[Layers::DYNAMIC] = BPLayers::MOVING;
    mapping_[Layers::TRIGGER] = BPLayers::MOVING;
}

unsigned int BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
    return BPLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer layer) const {
    return mapping_[layer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const {
    switch (static_cast<JPH::BroadPhaseLayer::Type>(layer)) {
        case static_cast<JPH::BroadPhaseLayer::Type>(BPLayers::NON_MOVING): return "NON_MOVING";
        case static_cast<JPH::BroadPhaseLayer::Type>(BPLayers::MOVING):     return "MOVING";
        default: return "UNKNOWN";
    }
}
#endif

bool ObjectVsBPLayerFilterImpl::ShouldCollide(JPH::ObjectLayer obj, JPH::BroadPhaseLayer bp) const {
    switch (obj) {
        case Layers::STATIC:  return bp == BPLayers::MOVING;
        case Layers::DYNAMIC: return true;
        case Layers::TRIGGER: return bp == BPLayers::MOVING;
        default: return false;
    }
}

bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const {
    switch (a) {
        case Layers::STATIC:  return b == Layers::DYNAMIC;
        case Layers::DYNAMIC: return true;
        case Layers::TRIGGER: return b == Layers::DYNAMIC;
        default: return false;
    }
}

} // namespace gg
