// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERMAPPINGUTILS_H
#define CUSTOMCONTROLLERMAPPINGUTILS_H

#include "CustomControllerTypes.h"

#include <vector>

class RGBController;

namespace CustomControllerMapping
{

bool IsControllerRegistered(RGBController* controller, const std::vector<RGBController*>& controllers);

void SyncIdentity(GridLEDMapping& mapping);

void FinalizeMapping(GridLEDMapping& mapping);

void RebindAll(std::vector<GridLEDMapping>& mappings, std::vector<RGBController*>& controllers);

int UnresolvedCount(const std::vector<GridLEDMapping>& mappings);

RGBController* FindByStoredIdentity(const std::vector<RGBController*>& controllers,
                                    const std::string& controller_name,
                                    const std::string& controller_location);

} // namespace CustomControllerMapping

#endif
