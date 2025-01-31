/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/JsonObject.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Widget.h>
#include <LibGfx/Orientation.h>
#include <stdio.h>

//#define GBOXLAYOUT_DEBUG

REGISTER_WIDGET(GUI, HorizontalBoxLayout)
REGISTER_WIDGET(GUI, VerticalBoxLayout)

namespace GUI {

BoxLayout::BoxLayout(Orientation orientation)
    : m_orientation(orientation)
{
    register_property(
        "orientation", [this] { return m_orientation == Gfx::Orientation::Vertical ? "Vertical" : "Horizontal"; }, nullptr);
}

Gfx::IntSize BoxLayout::preferred_size() const
{
    Gfx::IntSize size;
    size.set_primary_size_for_orientation(orientation(), preferred_primary_size());
    size.set_secondary_size_for_orientation(orientation(), preferred_secondary_size());
    return size;
}

int BoxLayout::preferred_primary_size() const
{
    int size = 0;

    for (auto& entry : m_entries) {
        if (!entry.widget || !entry.widget->is_visible())
            continue;
        int min_size = entry.widget->min_size().primary_size_for_orientation(orientation());
        int max_size = entry.widget->max_size().primary_size_for_orientation(orientation());
        int preferred_primary_size = -1;
        if (entry.widget->is_shrink_to_fit() && entry.widget->layout()) {
            preferred_primary_size = entry.widget->layout()->preferred_size().primary_size_for_orientation(orientation());
        }
        int item_size = max(0, preferred_primary_size);
        item_size = max(min_size, item_size);
        item_size = min(max_size, item_size);
        size += item_size + spacing();
    }
    if (size > 0)
        size -= spacing();

    if (orientation() == Gfx::Orientation::Horizontal)
        size += margins().left() + margins().right();
    else
        size += margins().top() + margins().bottom();

    if (!size)
        return -1;
    return size;
}

int BoxLayout::preferred_secondary_size() const
{
    int size = 0;
    for (auto& entry : m_entries) {
        if (!entry.widget || !entry.widget->is_visible())
            continue;
        int min_size = entry.widget->min_size().secondary_size_for_orientation(orientation());
        int preferred_secondary_size = -1;
        if (entry.widget->is_shrink_to_fit() && entry.widget->layout()) {
            preferred_secondary_size = entry.widget->layout()->preferred_size().secondary_size_for_orientation(orientation());
            size = max(size, preferred_secondary_size);
        }
        size = max(min_size, size);
    }

    if (orientation() == Gfx::Orientation::Horizontal)
        size += margins().top() + margins().bottom();
    else
        size += margins().left() + margins().right();

    if (!size)
        return -1;
    return size;
}

void BoxLayout::run(Widget& widget)
{
    if (m_entries.is_empty())
        return;

    struct Item {
        Widget* widget { nullptr };
        int min_size { -1 };
        int max_size { -1 };
        int size { 0 };
        bool final { false };
    };

    Vector<Item, 32> items;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = m_entries[i];
        if (entry.type == Entry::Type::Spacer) {
            items.append(Item { nullptr, -1, -1 });
            continue;
        }
        if (!entry.widget)
            continue;
        if (!entry.widget->is_visible())
            continue;
        auto min_size = entry.widget->min_size();
        auto max_size = entry.widget->max_size();

        if (entry.widget->is_shrink_to_fit() && entry.widget->layout()) {
            auto preferred_size = entry.widget->layout()->preferred_size();
            min_size = max_size = preferred_size;
        }

        items.append(Item { entry.widget.ptr(), min_size.primary_size_for_orientation(orientation()), max_size.primary_size_for_orientation(orientation()) });
    }

    if (items.is_empty())
        return;

    int available_size = widget.size().primary_size_for_orientation(orientation()) - spacing() * (items.size() - 1);
    int unfinished_items = items.size();

    if (orientation() == Gfx::Orientation::Horizontal)
        available_size -= margins().left() + margins().right();
    else
        available_size -= margins().top() + margins().bottom();

    // Pass 1: Set all items to their minimum size.
    for (auto& item : items) {
        item.size = 0;
        if (item.min_size >= 0)
            item.size = item.min_size;
        available_size -= item.size;

        if (item.min_size >= 0 && item.max_size >= 0 && item.min_size == item.max_size) {
            // Fixed-size items finish immediately in the first pass.
            item.final = true;
            --unfinished_items;
        }
    }

    // Pass 2: Distribute remaining available size evenly, respecting each item's maximum size.
    while (unfinished_items && available_size > 0) {
        int slice = available_size / unfinished_items;
        available_size = 0;

        for (auto& item : items) {
            if (item.final)
                continue;

            int item_size_with_full_slice = item.size + slice;
            item.size = item_size_with_full_slice;

            if (item.max_size >= 0)
                item.size = min(item.max_size, item_size_with_full_slice);

            // If the slice was more than we needed, return remained to available_size.
            int remainder_to_give_back = item_size_with_full_slice - item.size;
            available_size += remainder_to_give_back;

            if (item.max_size >= 0 && item.size == item.max_size) {
                // We've hit the item's max size. Don't give it any more space.
                item.final = true;
                --unfinished_items;
            }
        }
    }

    // Pass 3: Place the widgets.
    int current_x = margins().left();
    int current_y = margins().top();

    for (auto& item : items) {
        Gfx::IntRect rect { current_x, current_y, 0, 0 };

        rect.set_primary_size_for_orientation(orientation(), item.size);

        if (item.widget) {
            int secondary = widget.size().secondary_size_for_orientation(orientation());
            if (orientation() == Gfx::Orientation::Horizontal)
                secondary -= margins().top() + margins().bottom();
            else
                secondary -= margins().left() + margins().right();

            int min_secondary = item.widget->min_size().secondary_size_for_orientation(orientation());
            int max_secondary = item.widget->max_size().secondary_size_for_orientation(orientation());
            if (min_secondary >= 0)
                secondary = max(secondary, min_secondary);
            if (max_secondary >= 0)
                secondary = min(secondary, max_secondary);

            rect.set_secondary_size_for_orientation(orientation(), secondary);

            if (orientation() == Gfx::Orientation::Horizontal)
                rect.center_vertically_within(widget.rect());
            else
                rect.center_horizontally_within(widget.rect());

            item.widget->set_relative_rect(rect);
        }

        if (orientation() == Gfx::Orientation::Horizontal)
            current_x += rect.width() + spacing();
        else
            current_y += rect.height() + spacing();
    }
}

}
