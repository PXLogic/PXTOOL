/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#include "decoderoptionsdlg.h"
#include <libsigrokdecode.h>
#include <QScrollArea>
#include <QPushButton>
#include <QGroupBox>
#include <assert.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QFormLayout>
#include <QVariant>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QCheckBox>

#include "../data/decoderstack.h"
#include "../prop/binding/decoderoptions.h"
#include "../data/decode/decoder.h"
#include "../ui/dscombobox.h"
#include "../view/logicsignal.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../view/view.h"
#include "../view/cursor.h"
#include "../widgets/decodergroupbox.h"
#include "../view/decodetrace.h"
#include "../ui/msgbox.h"
#include "../log.h"

#include "../ui/langresource.h"
#include "../config/appconfig.h"

namespace pv {
namespace dialogs {


DecoderOptionsDlg::DecoderOptionsDlg(QWidget *parent)
:DSDialog(parent)
{
    _session = nullptr;
    _cursor1 = 0;
    _cursor2 = 0;
    _contentHeight = 0;
    _is_reload_form = false;
    _content_width = 0;
}

DecoderOptionsDlg::~DecoderOptionsDlg()
{
    // Disconnect region comboboxes before destroying _bindings.
    // Qt can fire currentIndexChanged during widget destruction, which would
    // call update_decode_range() on partially-freed members and crash.
    if (_start_comboBox)
        disconnect(_start_comboBox, nullptr, this, nullptr);
    if (_end_comboBox)
        disconnect(_end_comboBox, nullptr, this, nullptr);

    for(auto p : _bindings){
        delete p;
    }
    _bindings.clear();
}

void DecoderOptionsDlg::load_options(view::DecodeTrace *trace)
{
    assert(trace);
    _trace = trace;
    _session = trace->get_session();

    const char *dec_id = trace->decoder()->get_root_decoder_id();

    if (LangResource::Instance()->is_new_decoder(dec_id))
        LangResource::Instance()->reload_dynamic();

    load_options_view();

    LangResource::Instance()->release_dynamic();
}

void DecoderOptionsDlg::load_options_view()
{
    DSDialog *dlg = this;

    dlg->setTitle(tr("Decoder Options"));
    dlg->setObjectName("decoderOptionsDialog");
    dlg->SetTitleSpace(8);
    dlg->layout()->setSpacing(0);
    dlg->layout()->setDirection(QBoxLayout::TopToBottom);
    dlg->layout()->setAlignment(Qt::AlignTop);
    dlg->layout()->setContentsMargins(0, 5, 0, 0);

    // Top divider
    auto *top_sep = new QWidget(dlg);
    top_sep->setObjectName("device_options_divider");
    top_sep->setFixedHeight(1);
    dlg->layout()->addWidget(top_sep);

    // Scroll panel (outer, no margins)
    QWidget *scroll_panel = new QWidget();
    QVBoxLayout *scroll_lay = new QVBoxLayout();
    scroll_lay->setContentsMargins(0, 0, 0, 0);
    scroll_lay->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll_panel->setLayout(scroll_lay);
    dlg->layout()->addWidget(scroll_panel);

    // Container panel (inner, with breathing room)
    QWidget *container_panel = new QWidget();
    QVBoxLayout *container_lay = new QVBoxLayout();
    container_lay->setContentsMargins(12, 8, 12, 8);
    container_lay->setSpacing(6);
    container_lay->setDirection(QBoxLayout::TopToBottom);
    container_lay->setAlignment(Qt::AlignTop);
    container_panel->setLayout(container_lay);
    scroll_lay->addWidget(container_panel);

    // Decoder channel/option forms
    load_decoder_forms(container_panel);

    // Region selector group
    QGroupBox *region_box = new QGroupBox(
        tr("Decode Range"), dlg);
    QFont font = this->font();
    font.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));
    region_box->setFont(font);
    QFormLayout *region_form = new QFormLayout();
    region_form->setContentsMargins(8, 16, 8, 8);
    region_form->setVerticalSpacing(6);
    region_form->setFormAlignment(Qt::AlignLeft);
    region_form->setLabelAlignment(Qt::AlignLeft);
    region_box->setLayout(region_form);

    _start_comboBox = new DsComboBox(dlg);
    _end_comboBox   = new DsComboBox(dlg);
    _start_comboBox->addItem("Start");
    _end_comboBox->addItem("End");
    _start_comboBox->setMinimumContentsLength(7);
    _end_comboBox->setMinimumContentsLength(7);

    // Add cursor list
    auto view = _trace->get_view();
    int dex1 = 0;
    int dex2 = 0;
    if (view) {
        int num = 1;
        for (auto c : view->get_cursorList()) {
            QString cursor_name = tr("Cursor") +
                                  QString::number(num);
            _start_comboBox->addItem(cursor_name, QVariant((quint64)c->get_key()));
            _end_comboBox->addItem(cursor_name, QVariant((quint64)c->get_key()));
            if (c->get_key() == _cursor1) dex1 = num;
            if (c->get_key() == _cursor2) dex2 = num;
            num++;
        }
    }
    if (dex1 == 0) _cursor1 = 0;
    if (dex2 == 0) _cursor2 = 0;
    _start_comboBox->setCurrentIndex(dex1);
    _end_comboBox->setCurrentIndex(dex2);
    update_decode_range();

    QLabel *lb1 = new QLabel(
        tr("The cursor for decode start time"));
    QLabel *lb2 = new QLabel(
        tr("The cursor for decode end time"));
    region_form->addRow(_start_comboBox, lb1);
    region_form->addRow(_end_comboBox,   lb2);
    container_lay->addWidget(region_box);

    // Translate-params checkbox (non-English only)
    int h_ex2 = 0;
    bool bLang = AppConfig::Instance().appOptions.transDecoderDlg;
    if (!LangResource::Instance()->is_lang_en()) {
        QString trans_label(tr("Translate param names"));
        QCheckBox *ck_trans = new QCheckBox();
        ck_trans->setFixedSize(20, 20);
        ck_trans->setChecked(bLang);
        connect(ck_trans, SIGNAL(released()), this, SLOT(on_trans_pramas()));
        QLabel *trans_lb = new QLabel(trans_label);

        QHBoxLayout *trans_lay = new QHBoxLayout();
        QWidget *trans_wid = new QWidget();
        trans_wid->setLayout(trans_lay);
        trans_lay->setSpacing(6);
        trans_lay->setContentsMargins(0, 4, 0, 0);
        trans_lay->addWidget(ck_trans);
        trans_lay->addWidget(trans_lb);
        trans_lay->addStretch();
        container_lay->addWidget(trans_wid);
        h_ex2 = 36;
    }

    (void)h_ex2;

    // Bottom divider
    auto *bot_sep = new QWidget(dlg);
    bot_sep->setObjectName("device_options_divider");
    bot_sep->setFixedHeight(1);
    dlg->layout()->addWidget(bot_sep);

    // Footer: Cancel + OK
    auto *cancel_btn = new QPushButton(
        tr("Cancel"), dlg);
    auto *ok_btn = new QPushButton(tr("OK"), dlg);
    cancel_btn->setObjectName("device_cancel_btn");
    ok_btn->setObjectName("device_ok_btn");

    cancel_btn->setMinimumWidth(80);
    ok_btn->setMinimumWidth(60);

    auto *footer_lay = new QHBoxLayout();
    footer_lay->setContentsMargins(12, 10, 12, 10);
    footer_lay->setSpacing(8);
    footer_lay->addStretch();
    footer_lay->addWidget(cancel_btn);
    footer_lay->addWidget(ok_btn);
    dlg->layout()->addLayout(footer_lay);

    this->update_font();

    // Size calculation.
    //
    // Activate the container layout so that QFormLayout / nested widgets
    // publish their final sizeHint() values, then ask the layout itself
    // for the total required height.  This includes the layout's
    // contentsMargins (8+8) AND the per-widget setSpacing(6) gaps between
    // every pair of children — both of which the previous manual sum
    // missed (it only added one spacing for region_box).  With multiple
    // decoders in the stack the under-count was 6-12 px and clipped the
    // bottom of "Decode Range" / footer.
    container_panel->layout()->activate();
    const int content_height = container_panel->layout()->sizeHint().height();

    QSize tsize = dlg->sizeHint();
    int w = tsize.width();
    if (w < _content_width) w = _content_width;

    int cursor_line_w = lb1->sizeHint().width() + _start_comboBox->sizeHint().width() + 24;
    if (w < cursor_line_w) w = cursor_line_w;

    // overhead: titlebar(44) + titleSpace(8) + top_sep(1) + bot_sep(1)
    //         + main_layout top/bottom margins(5+10) + footer(margins 10+10
    //         + button ~32) ≈ ~110, plus a small buffer for font metrics.
    int overhead = 140;
#ifdef Q_OS_DARWIN
    overhead += 20;
#endif

    // Use the screen the dialog is currently on, not always the primary screen,
    // so layout calculations are correct on secondary / external displays.
    QScreen *_curScreen = (window() && window()->windowHandle())
        ? window()->windowHandle()->screen()
        : QGuiApplication::primaryScreen();
    float sk = _curScreen->logicalDotsPerInch() / 96;
    const int srcHeight = 600;
    container_panel->setFixedHeight(content_height);

    if ((content_height + overhead) * sk > srcHeight) {
        QScrollArea *scroll = new QScrollArea(scroll_panel);
        scroll->setWidget(container_panel);
        scroll->setStyleSheet("QScrollArea{border:none;}");
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        int scroll_h = srcHeight - overhead;
        int sclw = w - 18;
#ifdef Q_OS_DARWIN
        sclw -= 20;
#endif
        scroll->setFixedSize(sclw, scroll_h);
        scroll_panel->setFixedSize(w, scroll_h);
        dlg->setFixedSize(w + 20, srcHeight);
    } else {
        dlg->setFixedSize(w + 20, content_height + overhead);
    }

    connect(_start_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_region_set(int)));
    connect(_end_comboBox,   SIGNAL(currentIndexChanged(int)), this, SLOT(on_region_set(int)));
    connect(cancel_btn, SIGNAL(clicked()), dlg, SLOT(reject()));
    connect(ok_btn,     SIGNAL(clicked()), dlg, SLOT(on_accept()));
}

void DecoderOptionsDlg::load_decoder_forms(QWidget *container)
{
	using pv::data::decode::Decoder; 
	assert(container); 

    for(auto dec : _trace->decoder()->stack()) 
    { 
        QWidget *panel = new QWidget(container);
        QFormLayout *form = new QFormLayout();
        form->setContentsMargins(0,0,0,0);
        panel->setLayout(form);
        container->layout()->addWidget(panel);
       
        create_decoder_form(dec, panel, form); 

        _contentHeight += panel->sizeHint().height();
	} 
}
 

pv::SigSession* DecoderOptionsDlg::effectiveSession()
{
    if (_session) {
        for (auto s : _session->get_signals()) {
            if (s->signal_type() == SR_CHANNEL_LOGIC &&
                _session->is_channel_enabled(s->get_index()))
                return _session;
        }
    }
    return AppControl::Instance()->GetSession();
}

// Normalize a channel name for fuzzy auto-binding: lowercase, drop '#', '_',
// '-' and whitespace. This lets us match e.g. "CS#" <-> "cs", "I2C_SDA" <-> "sda".
static QString normalize_chname(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        if (c.isLetterOrNumber())
            out.append(c.toLower());
    }
    return out;
}

DsComboBox* DecoderOptionsDlg::create_probe_selector(
    QWidget *parent, const data::decode::Decoder *dec,
	const srd_channel *const pdch)
{
	assert(dec);
    
    pv::SigSession *eff = effectiveSession();
    const auto &sigs = eff->get_signals();

    data::decode::Decoder *decoder = const_cast<data::decode::Decoder*>(dec);

	DsComboBox *selector = new DsComboBox(parent);
    selector->addItem("-", QVariant::fromValue(-1));

    int dex = 0;
    const int binded_index = decoder->binded_probe_index(pdch);

    // Track the combo entry that fuzz-matches this decoder channel by name,
    // so we can auto-select it when the user hasn't bound this channel yet.
    int auto_match_combo_dex = -1;
    const QString want = pdch ? normalize_chname(QString::fromUtf8(pdch->name))
                              : QString();

	for(auto s : sigs)
    {
        dex++;

        if (s->signal_type() == SR_CHANNEL_LOGIC &&
            eff->is_channel_enabled(s->get_index())) {
			selector->addItem(s->get_name(),QVariant::fromValue(s->get_index()));

            if (binded_index == s->get_index()){
                selector->setCurrentIndex(dex);
            }

            if (binded_index == -1 && auto_match_combo_dex == -1 && !want.isEmpty()) {
                const QString have = normalize_chname(s->get_name());
                if (!have.isEmpty() && have == want)
                    auto_match_combo_dex = dex;
            }
		}
	}

    if (binded_index == -1){
        if (auto_match_combo_dex >= 0)
            selector->setCurrentIndex(auto_match_combo_dex);
        else
            selector->setCurrentIndex(0);
    }

	return selector;
}

void DecoderOptionsDlg::on_region_set(int index)
{
    (void)index;
    update_decode_range();
}

void DecoderOptionsDlg::update_decode_range()
{ 
    const uint64_t last_samples = AppControl::Instance()->GetSession()->cur_samplelimits() - 1;
    const int index1 = _start_comboBox->currentIndex();
    const int index2 = _end_comboBox->currentIndex();
    uint64_t decode_start, decode_end;

    auto view = _trace->get_view();

    if (index1 == 0) {
        decode_start = 0;
        _cursor1 = 0;

    } else if (view) {
        _cursor1 = _start_comboBox->itemData(index1).toULongLong();
        int cusrsor_index = view->get_cursor_index_by_key(_cursor1);
        if (cusrsor_index != -1){
            decode_start = view->get_cursor_samples(cusrsor_index);
        }
        else{
            decode_start = 0;
            _cursor1 = 0;
        }
    } else {
        decode_start = 0;
        _cursor1 = 0;
    }

    if (index2 == 0) {
        decode_end = last_samples;
        _cursor2 = 0;

    } else if (view) {
        _cursor2 = _end_comboBox->itemData(index2).toULongLong();
        int cusrsor_index = view->get_cursor_index_by_key(_cursor2);
        if (cusrsor_index != -1){
            decode_end = view->get_cursor_samples(cusrsor_index);
        }
        else{
            decode_end = last_samples;
            _cursor2 = 0;
        }
    } else {
        decode_end = last_samples;
        _cursor2 = 0;
    }

    if (decode_start > last_samples)
        decode_start = 0;
    if (decode_end > last_samples)
        decode_end = last_samples;

    if (decode_start > decode_end) {
        uint64_t tmp = decode_start;
        decode_start = decode_end;
        decode_end = tmp;
    }
  
    for(auto dec : _trace->decoder()->stack()) {
        dec->set_decode_region(decode_start, decode_end);
    }
}
 

void DecoderOptionsDlg::create_decoder_form(
    data::decode::Decoder *dec, QWidget *parent,
    QFormLayout *form)
{
	const GSList *l;

    assert(dec);     
	const srd_decoder *const decoder = dec->decoder();
	assert(decoder);

    QFont font = this->font();
    font.setPixelSize(qRound(AppConfig::Instance().appOptions.fontSize));

    QFormLayout *const decoder_form = new QFormLayout();
    decoder_form->setContentsMargins(0,0,0,0);
    decoder_form->setVerticalSpacing(4);
    decoder_form->setFormAlignment(Qt::AlignLeft);
    decoder_form->setLabelAlignment(Qt::AlignLeft);
    decoder_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    bool bLang = AppConfig::Instance().appOptions.transDecoderDlg;
    if (LangResource::Instance()->is_lang_en()){
        bLang = false;
    }
 
	// Add the mandatory channels
	for(l = decoder->channels; l; l = l->next) {
		const struct srd_channel *const pdch = (struct srd_channel *)l->data;
		DsComboBox *const combo = create_probe_selector(parent, dec, pdch);

        const char *desc_str = NULL;
        const char *lang_str = NULL;

        if (pdch->idn != NULL && LangResource::Instance()->is_lang_en() == false){
            lang_str = LangResource::Instance()->get_lang_text(STR_PAGE_DECODER, pdch->idn, pdch->desc);
        }

        if (lang_str != NULL && bLang){
            desc_str = lang_str;
        }
        else{
            desc_str = pdch->desc;
        }

        //tr
        decoder_form->addRow(QString("<b>%1</b> (%2) *")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(desc_str)), combo);

        const ProbeSelector s = {combo, dec, pdch};
    	_probe_selectors.push_back(s);
	} 

	// Add the optional channels
	for(l = decoder->opt_channels; l; l = l->next) {
		const struct srd_channel *const pdch = (struct srd_channel *)l->data;
		DsComboBox *const combo = create_probe_selector(parent, dec, pdch);
		
        const char *desc_str = NULL;
        const char *lang_str = NULL;
        
        if (pdch->idn != NULL && LangResource::Instance()->is_lang_en() == false){
            lang_str = LangResource::Instance()->get_lang_text(STR_PAGE_DECODER, pdch->idn, pdch->desc);
        }

        if (lang_str != NULL && bLang){
            desc_str = lang_str;
        }
        else{
            desc_str = pdch->desc;
        }

        //tr
        decoder_form->addRow(QString("<b>%1</b> (%2)")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(desc_str)), combo);

        const ProbeSelector s = {combo, dec, pdch};
        _probe_selectors.push_back(s);
	}

	// Add the options
    auto binding = new prop::binding::DecoderOptions(_trace->decoder(), dec);
    binding->add_properties_to_form(decoder_form, true, font);
	_bindings.push_back(binding);
  
    auto group = new pv::widgets::DecoderGroupBox(_trace->decoder(), 
                            dec, 
                            decoder_form, 
                            parent, font);

    if (group->_content_width > _content_width){
        _content_width = group->_content_width;
    }

	form->addRow(group);
}

void DecoderOptionsDlg::commit_probes()
{ 
    for(auto dec : _trace->decoder()->stack()){
		commit_decoder_probes(dec);
    }
}

void DecoderOptionsDlg::commit_decoder_probes(data::decode::Decoder *dec)
{
	assert(dec); 

    std::map<const srd_channel*, int> probe_map;
    const auto &sigs = effectiveSession()->get_signals();

    std::list<int> index_list;

	for(auto &p : _probe_selectors)
	{
		if(p._decoder != dec)
			break;

        const int selection = p._combo->itemData(p._combo->currentIndex()).value<int>();

        bool matched = false;
        for(auto s : sigs){
            if(s->get_index() == selection) {
                probe_map[p._pdch] = selection;
                index_list.push_back(selection);
                matched = true;
				break;
			}
        }
        dsv_info("DecoderOptionsDlg::apply_setting: pdch='%s' selection=%d matched=%d",
                 p._pdch ? p._pdch->name : "?", selection, matched ? 1 : 0);
	}

	dec->set_probes(probe_map);
    dsv_info("DecoderOptionsDlg::apply_setting: committed %zu probe bindings",
             probe_map.size());

    if (index_list.size())
        _trace->set_index_list(index_list);
}
 
void DecoderOptionsDlg::on_accept()
{ 
    if (_cursor1 > 0 && _cursor1 == _cursor2){
        MsgBox::Show(tr("error"), 
        tr("Invalid cursor index for sample range!"));
        return;
    }

    this->accept();
}

void DecoderOptionsDlg::on_trans_pramas()
{
    QCheckBox *ck_box = dynamic_cast<QCheckBox*>(sender());
    assert(ck_box);

    AppConfig::Instance().appOptions.transDecoderDlg = ck_box->isChecked();
    AppConfig::Instance().SaveApp();
    _is_reload_form = true;
    this->reject();
}

void DecoderOptionsDlg::apply_setting()
{
    commit_probes();
}

}//dialogs
}//pv
