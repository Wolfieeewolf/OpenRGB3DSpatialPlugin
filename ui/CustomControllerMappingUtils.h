// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERMAPPINGUTILS_H
#define CUSTOMCONTROLLERMAPPINGUTILS_H

#include "CustomControllerTypes.h"

#include <vector>

class RGBControllerInterface;

namespace CustomControllerMapping
{

bool IsControllerRegistered(RGBControllerInterface* controller, const std::vector<RGBControllerInterface*>& controllers);

void SyncIdentity(GridLEDMapping& mapping);

void FinalizeMapping(GridLEDMapping& mapping);

bool RebindAll(std::vector<GridLEDMapping>& mappings, std::vector<RGBControllerInterface*>& controllers);

int UnresolvedCount(const std::vector<GridLEDMapping>& mappings);

RGBControllerInterface* FindControllerForMappings(const std::vector<RGBControllerInterface*>& controllers,
                                           const std::string& controller_name,
                                           const std::string& controller_location,
                                           const std::vector<const GridLEDMapping*>& mappings);

RGBControllerInterface* FindByStoredIdentity(const std::vector<RGBControllerInterface*>& controllers,
                                    const std::string& controller_name,
                                    const std::string& controller_location);

bool MappingOwnedByController(const GridLEDMapping& mapping,
                              RGBControllerInterface* controller,
                              const std::vector<RGBControllerInterface*>& controllers);

} // namespace CustomControllerMapping

#endif
