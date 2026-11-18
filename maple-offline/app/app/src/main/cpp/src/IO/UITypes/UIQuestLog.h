//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

#include "../../Character/QuestLog.h"
#include "../Components/Slider.h"
#include "../Components/Textfield.h"
#include "../UIDragElement.h"

namespace ms {
class UIQuestLog : public UIDragElement<PosQUEST> {
public:
    static constexpr Type TYPE = UIElement::Type::QUEST_LOG;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = true;

    UIQuestLog(const QuestLog &questLog);

    void draw(float inter) const override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

    UIElement::Type get_type() const override;

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    void change_tab(uint16_t tabid);
    void load_list();

    // maple-offline: v83 UIWindow.img/Quest (245x396) — three tabs and a
    // scrolling list of quest names.
    static constexpr int16_t ROWS_ = 17;
    static constexpr int16_t ROW_HEIGHT_ = 18;
    static constexpr int16_t LIST_Y_ = 52;

    enum Buttons : uint16_t { TAB0, TAB1, TAB2, CLOSE };

    const QuestLog &questlog_;
    uint16_t tab_;
    Slider slider_;
    int16_t offset_;
    std::vector<Text> rows_;
    Text empty_;
    Texture icon_;
};
}  // namespace ms