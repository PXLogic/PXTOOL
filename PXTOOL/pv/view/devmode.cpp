/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#include "devmode.h"
#include "view.h"
#include "trace.h"
#include "../sigsession.h" 

#include <assert.h> 
#include <QStyleOption>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QHBoxLayout>
#include <QAbstractItemView>

#include "../config/appconfig.h"
#include "../ui/msgbox.h"
#include "../log.h"
#include "../appcontrol.h"
#include "../ui/fn.h"


static const struct dev_mode_name dev_mode_name_list[] =
{
    {LOGIC, "la.svg"},
    {ANALOG, "daq.svg"},
    {DSO, "osc.svg"},
    {MSO, "osc.svg"},
};

static QIcon tinted_mode_icon(const QString &path, const QColor &color,
                              const QSize &size)
{
    QPixmap source = QIcon(path).pixmap(size);
    if (source.isNull())
        return QIcon();

    QPixmap tinted(source.size());
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();
    return QIcon(tinted);
}

static QIcon tinted_mode_icon(const QIcon &source, const QColor &color,
                              const QSize &size)
{
    QPixmap pixmap = source.pixmap(size);
    if (pixmap.isNull())
        return QIcon();
    QPixmap tinted(pixmap.size());
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    return QIcon(tinted);
}

class DevModeComboBox final : public DsComboBox
{
public:
    explicit DevModeComboBox(QWidget *parent) : DsComboBox(parent) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionComboBox option;
        initStyleOption(&option);
        option.currentIcon = QIcon();
        option.currentText.clear();

        QPainter painter(this);
        style()->drawComplexControl(QStyle::CC_ComboBox, &option, &painter, this);

        if (currentIndex() < 0)
            return;

        QRect content = style()->subControlRect(QStyle::CC_ComboBox, &option,
                                                 QStyle::SC_ComboBoxEditField,
                                                 this);
        const QFontMetrics metrics(font());
        const int mode = itemData(currentIndex()).toInt();
        const QString text = pv::view::DevMode::button_label_for_mode(mode);
        const QColor modeColor = mode == ANALOG
            ? QColor(QStringLiteral("#4ade80"))
            : mode == DSO
                ? QColor(QStringLiteral("#60a5fa"))
                : QColor(AppConfig::Instance().IsDarkStyle()
                             ? QStringLiteral("#c084fc")
                             : QStringLiteral("#7c3aed"));
        const QSize modeIconSize(43, 32);
        const QIcon icon = tinted_mode_icon(itemIcon(currentIndex()), modeColor,
                                            modeIconSize);
        const QSize requested = modeIconSize;
        const int textWidth = metrics.horizontalAdvance(text);
        const int gap = 3;
        const int chevronHeight = 6;
        const int chevronGap = 2;
        const int groupHeight = qMax(requested.height(), metrics.height());
        const int stackedHeight = groupHeight + chevronGap + chevronHeight;
        const QRect groupRow(content.left(),
            content.center().y() - stackedHeight / 2,
            content.width(), groupHeight);
        const int groupWidth = requested.width() + gap + textWidth;
        int x = groupRow.center().x() - groupWidth / 2;
        const int y = groupRow.center().y() - requested.height() / 2 + 8;
        const QPoint arrowCenter(groupRow.center().x(),
            groupRow.bottom() + chevronGap + chevronHeight / 2);

        icon.paint(&painter, QRect(x, y, requested.width(), requested.height()),
                   Qt::AlignCenter, QIcon::Normal, QIcon::On);
        x += requested.width() + gap;
        const QColor textColor = mode == DSO
            ? QColor(AppConfig::Instance().IsDarkStyle()
                         ? QStringLiteral("#8ab4f8") : QStringLiteral("#2563eb"))
            : mode == ANALOG
                ? QColor(QStringLiteral("#4ade80"))
                : QColor(AppConfig::Instance().IsDarkStyle()
                             ? QStringLiteral("#c084fc") : QStringLiteral("#7c3aed"));
        painter.setPen(textColor);
        painter.setFont(font());
        painter.drawText(QRect(x, groupRow.top(), textWidth, groupRow.height()),
                         Qt::AlignVCenter | Qt::AlignLeft, text);

        QPen arrowPen(textColor, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(arrowPen);
        painter.drawLine(arrowCenter + QPoint(-4, -2), arrowCenter + QPoint(0, 2));
        painter.drawLine(arrowCenter + QPoint(0, 2), arrowCenter + QPoint(4, -2));
    }
};
  
namespace pv {
namespace view {

DevMode::DevMode(QWidget *parent, SigSession *session) :
    QWidget(parent) 
{
    setObjectName("DevMode");
    setAccessibleName(tr("Capture mode switch"));
    setAccessibleDescription(
        tr("Switch between Logic Analyzer, Data Acquisition, and Oscilloscope."));

    _bFile = false;
    _updating_mode = false;

    _session = session;
    _device_agent = session->get_device();

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    _close_button = new XToolButton();
    _close_button->setObjectName("FileCloseButton");
    _close_button->setContentsMargins(0, 0, 0, 0);
    _close_button->setFixedWidth(10);
    _close_button->setFixedHeight(height());
    _close_button->setIconSize(QSize(10, 10));
    _close_button->setToolButtonStyle(Qt::ToolButtonIconOnly); 
    _close_button->setMinimumWidth(10);

    _mode_btn = new DevModeComboBox(this);
    _mode_btn->setObjectName("ModeButton");
    _mode_btn->setFrame(false);
    apply_mode_style();
    // Keep the mode switch as a real popup button so keyboard and accessibility
    // clients can discover and activate it without relying on the icon.
    _mode_btn->setAccessibleName(tr("Mode"));
    _mode_btn->setAccessibleDescription(
        tr("Select the active capture mode. Opens a menu of available modes."));
    _mode_btn->setToolTip(tr("Capture mode"));
    _mode_btn->setStatusTip(tr("Select Logic Analyzer, Data Acquisition, or Oscilloscope"));
    _mode_btn->setFocusPolicy(Qt::StrongFocus);
    _mode_btn->setIconSize(QSize(32, 24));
    _mode_btn->setContentsMargins(0, 0, 0, 0);
    _mode_btn->setMinimumHeight(42);
    _mode_btn->setMinimumWidth(150);
    _mode_btn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    _mode_btn->setPopupItemHeight(28);
    _mode_btn->setPopupFitContents(true);
    connect(_mode_btn, SIGNAL(currentIndexChanged(int)), this, SLOT(on_mode_change(int)));

    layout->addWidget(_close_button);
    layout->addWidget(_mode_btn); 

    layout->setStretch(1, 100); 
    setLayout(layout);

    connect(_close_button, SIGNAL(clicked()), this, SLOT(on_close()));

    ADD_UI(this);
}

DevMode::~DevMode()
{
    REMOVE_UI(this);
}

void DevMode::set_device()
{ 
     if (_device_agent->have_instance() == false){
        dsv_detail("DevMode::set_device, Have no device.");   
        return;
     }

    _bFile = false;
   
    _updating_mode = true;
    _mode_btn->clear();

    _close_button->setIcon(QIcon());
    _close_button->setDisabled(true); 

    QString iconPath = GetIconPath() + "/";
    const QColor menuIconColor(QStringLiteral("#f3f4f6"));
    auto dev_mode_list  = _device_agent->get_device_mode_list();

    for (const GSList *l = dev_mode_list; l; l = l->next)
    {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        auto *mode_name = get_mode_name(mode->mode);
        QString icon_name = QString::fromLocal8Bit(mode_name->_logo);

        int md = mode->mode;
        const QString label = display_name_for_mode(md);

        _mode_btn->addItem(tinted_mode_icon(iconPath + icon_name, menuIconColor,
                                             QSize(32, 24)), label, md);
        QFont popupFont = _mode_btn->font();
        popupFont.setBold(false);
        popupFont.setUnderline(false);
        _mode_btn->setItemData(_mode_btn->count() - 1, popupFont, Qt::FontRole);
    }
    const QFontMetrics modeMetrics(_mode_btn->font());
    int popupTextWidth = 0;
    for (int i = 0; i < _mode_btn->count(); ++i)
        popupTextWidth = qMax(popupTextWidth,
                              modeMetrics.horizontalAdvance(_mode_btn->itemText(i)));
    _mode_btn->setPopupFitContents(false);
    _mode_btn->view()->setMinimumWidth(popupTextWidth + 32 + 8 + 28);
    _updating_mode = false;

    _close_button->setVisible(false);

    if (_device_agent->is_file()){
        _close_button->setVisible(true);
        _close_button->setDisabled(false);
        _close_button->setIcon(QIcon(iconPath + "/close.svg"));
        _bFile = true;
    }

    sync_mode_button(_device_agent->get_work_mode(), iconPath);

    UpdateFont();
    apply_mode_style();
    update();    
}

void DevMode::apply_mode_style()
{
    if (!_mode_btn)
        return;

    const int fontSize = qMax(1, qRound(AppConfig::Instance().appOptions.fontSize));
    const bool isDark = AppConfig::Instance().IsDarkStyle();
    const QString purpleBg = isDark
        ? QStringLiteral("rgba(147, 51, 234, 0.20)")
        : QStringLiteral("rgba(147, 51, 234, 0.08)");
    const QString popupText = isDark
        ? QStringLiteral("#d1d5db") : QStringLiteral("#374151");
    const int activeMode = _mode_btn->currentData().toInt();
    QString modeBg = purpleBg;
    QString modeText = isDark ? QStringLiteral("#c084fc")
                              : QStringLiteral("#7c3aed");
    QString modeHoverText = isDark ? QStringLiteral("#e9d5ff")
                                   : QStringLiteral("#5b21b6");
    if (activeMode == ANALOG) {
        modeBg = QStringLiteral("rgba(22, 163, 74, 0.10)");
        modeText = QStringLiteral("#4ade80");
        modeHoverText = QStringLiteral("#86efac");
    } else if (activeMode == DSO) {
        modeBg = isDark ? QStringLiteral("rgba(59, 130, 246, 0.20)")
                        : QStringLiteral("rgba(59, 130, 246, 0.08)");
        modeText = isDark ? QStringLiteral("#8ab4f8") : QStringLiteral("#2563eb");
        modeHoverText = isDark ? QStringLiteral("#dbeafe") : QStringLiteral("#1d4ed8");
    }

    QFont font = _mode_btn->font();
    font.setPixelSize(fontSize);
    font.setBold(true);
    font.setUnderline(true);
    _mode_btn->setFont(font);
    _mode_btn->setStyleSheet(
        QStringLiteral("QComboBox#ModeButton { background: transparent; border: none; "
                       "border-radius: 4px; color: %3; "
                       "font-size: %1px; font-weight: 600; padding: 3px 8px 6px 4px; }"
                       "QComboBox#ModeButton:hover, QComboBox#ModeButton:focus, "
                       "QComboBox#ModeButton:on { background: transparent; border: none; "
                       "color: %4; }"
                       "QComboBox#ModeButton::drop-down { width: 0px; border: none; }"
                       "QComboBox#ModeButton::down-arrow { image: none; width: 0px; "
                       "height: 0px; }"
                       "QComboBox#ModeButton QAbstractItemView { color: %2; "
                       "font-size: %1px; font-weight: normal; text-decoration: none; }"
                       "QComboBox#ModeButton QAbstractItemView::item:hover, "
                       "QComboBox#ModeButton QAbstractItemView::item:selected "
                       "{ background-color: #7c3aed; color: #ffffff; }")
            .arg(QString::number(fontSize), popupText, modeText, modeHoverText));
    QFont popupFont = font;
    popupFont.setBold(false);
    popupFont.setUnderline(false);
    _mode_btn->view()->setFont(popupFont);
    QPalette popupPalette = _mode_btn->view()->palette();
    popupPalette.setColor(QPalette::Text, popupText);
    popupPalette.setColor(QPalette::HighlightedText, Qt::white);
    _mode_btn->view()->setPalette(popupPalette);
    setStyleSheet(QStringLiteral(
        "QWidget#DevMode { background-color: %1; border: none; "
        "border-radius: 4px; }" ).arg(modeBg));
}

void DevMode::paintEvent(QPaintEvent*)
{  
    using pv::view::Trace;

    QStyleOption o;
    o.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &painter, this);
}

void DevMode::sync_mode_button_width()
{
    _mode_btn->setFixedWidth(150);
}

void DevMode::sync_mode_button(int mode, const QString &iconPath)
{
    const QString label = display_name_for_mode(mode);

    (void)iconPath;
    const bool oldUpdating = _updating_mode;
    _updating_mode = true;
    for (int i = 0; i < _mode_btn->count(); ++i) {
        if (_mode_btn->itemData(i).toInt() == mode) {
            _mode_btn->setCurrentIndex(i);
            break;
        }
    }
    _updating_mode = oldUpdating;
    _mode_btn->setAccessibleName(tr("Mode"));
    _mode_btn->setAccessibleDescription(
        tr("Current mode: %1. Opens a menu to switch capture mode.").arg(label));
    _mode_btn->setToolTip(tr("Capture mode: %1 (click to switch)").arg(label));

}

void DevMode::on_mode_change(int index)
{
    if (_updating_mode || index < 0 || index >= _mode_btn->count())
        return;

    if (_device_agent->have_instance() == false){
        assert(false);
    }

    const int mode = _mode_btn->itemData(index).toInt();
    if (_device_agent->get_work_mode() == mode){
        return;
    }

    QString iconPath = GetIconPath();

    _session->stop_capture();
    _session->session_save();
    _session->switch_work_mode(mode);
    sync_mode_button(mode, iconPath);
    apply_mode_style();

    UpdateFont();
}

void DevMode::on_close()
{
   if (_device_agent->have_instance() == false){
        assert(false);
    }

    if (_bFile && MsgBox::Confirm(tr("Are you sure to close the device?"))){
        _session->close_file(_device_agent->handle());
    }
}

void DevMode::mousePressEvent(QMouseEvent *event)
{
	assert(event);
	(void)event;
}

void DevMode::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
        (void)event;
}

void DevMode::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	_mouse_point = event->pos();
	update();
}

void DevMode::leaveEvent(QEvent*)
{
	_mouse_point = QPoint(-1, -1);
	update();
}

const struct dev_mode_name* DevMode::get_mode_name(int mode) 
{
    for(auto &o : dev_mode_name_list)
        if (mode == o._mode){
            return &o;
    }
    assert(false);
}

void DevMode::UpdateLanguage()
{
    set_device();
}

void DevMode::UpdateTheme()
{
    apply_mode_style();
    set_device();
}

void DevMode::UpdateFont()
{
    QFont font = this->font();
    font.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));
    
    auto buttons = this->findChildren<QToolButton*>();
    for(auto o : buttons)
    { 
        o->setFont(font);
    }

    _mode_btn->setFont(font);
    apply_mode_style();
    sync_mode_button_width();
}

} // namespace view
} // namespace pv
