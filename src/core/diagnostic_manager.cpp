/// @file diagnostic_manager.cpp
/// @brief Implementation of DiagnosticManager

#include <kalahari/core/diagnostic_manager.h>

namespace kalahari {
namespace core {

DiagnosticManager& DiagnosticManager::getInstance() {
    static DiagnosticManager instance;
    return instance;
}

} // namespace core
} // namespace kalahari
