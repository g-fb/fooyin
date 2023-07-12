/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "librarytreegroupregistry.h"

namespace Fy::Gui::Widgets {
void loadDefaults(LibraryTreeGroupRegistry* registry)
{
    registry->addItem({.id     = 0,
                       .index  = 0,
                       .name   = "Artist/Album",
                       .script = "$if2(%albumartist%,%artist%)||%album% (%year%)||%disc%.$num(%track%,2). %title%"});
    registry->addItem(
        {.id = 1, .index = 1, .name = "Album", .script = "%album% (%year%)||%disc%.$num(%track%,2). %title%"});
}

LibraryTreeGroupRegistry::LibraryTreeGroupRegistry(Utils::SettingsManager* settings, QObject* parent)
    : ItemRegistry{settings, parent}
{
    QObject::connect(this, &Utils::RegistryBase::itemChanged, this, [this](int id) {
        const auto grouping = itemById(id);
        emit groupingChanged(grouping);
    });
}

void LibraryTreeGroupRegistry::loadItems()
{
    ItemRegistry::loadItems();

    if(m_items.empty()) {
        loadDefaults(this);
    }
}
} // namespace Fy::Gui::Widgets