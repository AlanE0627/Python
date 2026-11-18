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

    bool is_in_range(Point<int16_t> cursorpos) const override;

    void send_scroll(double yoffset) override;

    UIElement::Type get_type() const override;

    // maple-offline: called when the server changes a quest record
    void refresh();

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    void change_tab(uint16_t tabid);
    void load_list();
    void select(int16_t qid);
    bool can_start(int16_t qid) const;

    // maple-offline: v83 UIWindow.img/Quest (245x396) — three tabs and a
    // scrolling list of quest names; the detail panel (backgrnd2/4,
    // 305x396) opens to the right of the list.
    static constexpr int16_t ROWS_ = 17;
    static constexpr int16_t ROW_HEIGHT_ = 18;
    static constexpr int16_t LIST_Y_ = 52;
    static constexpr int16_t LIST_W_ = 245;
    static constexpr int16_t DETAIL_W_ = 305;
    static constexpr int16_t DESC_TOP_ = 126;
    static constexpr int16_t DESC_BOTTOM_ = 362;
    static constexpr int16_t DESC_BOTTOM_REWARD_ = 242;

    enum Buttons : uint16_t { TAB0, TAB1, TAB2, CLOSE, GIVEUP };

    const QuestLog &questlog_;
    uint16_t tab_;
    Slider slider_;
    int16_t offset_;
    std::vector<int16_t> qids_;
    std::vector<Text> rows_;
    Text empty_;
    Texture icon_;

    // detail panel
    int16_t selected_;
    Texture detail_bg_;
    Texture detail_bg_reward_;
    Texture reward_label_;
    Text detail_name_;
    Text detail_npc_;
    Text detail_level_;
    Text detail_desc_;
    std::vector<Text> reward_lines_;
    bool has_reward_;
    int16_t desc_scroll_;
    Point<int16_t> last_cursor_;
};
}  // namespace ms
