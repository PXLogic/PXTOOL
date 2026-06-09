/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef DSCOMBOBOX_H
#define DSCOMBOBOX_H

#include <QComboBox>
#include <QKeyEvent>


class DsComboBox : public QComboBox
{
public:
    explicit DsComboBox(QWidget *parent = nullptr);

    ~DsComboBox(); 

public:
    void showPopup() override;

    void hidePopup() override;

    inline bool  IsPopup(){
        return _bPopup;
    }

    /** Recompute popup min-width after model items change. */
    void refreshPopupLayout();

    /** Fixed popup row height in px (QSS min-height is unreliable on Windows). */
    void setPopupItemHeight(int height);

    /** Resize combo and list view to the widest item (optional max width cap). */
    void adjustToItemContents(int maxWidgetWidth = 0);

    /** Shrink the dropdown popup to item text width when shown (Windows). */
    void setPopupFitContents(bool fit);

private:
    void measureSize();
    int maxItemTextWidth() const;
    int popupContentWidth() const;

private: 
    bool    _bPopup;
    bool    _popupFitContents = false;
};


#endif // DSCOMBOBOX_H
