/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include "protocoldock.h"
#include "../sigsession.h"
#include "../view/decodetrace.h"
#include "../data/decodermodel.h"
#include "../data/decoderstack.h"
#include "../dialogs/protocollist.h"
#include "../dialogs/protocolexp.h" 
#include "../view/view.h"
#include "c_decoder_registry.h"

#include <QObject>
#include <QHBoxLayout>
#include <QPainter>
#include <QFormLayout>
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QRegularExpression>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QSizePolicy>
#include <assert.h>
#include <map>
#include <string> 
#include <algorithm>
#include <QTableWidgetItem>
#include <QHeaderView>
#include "../ui/msgbox.h"
#include "../dsvdef.h"
#include "../config/appconfig.h"
#include "../data/decode/decoderstatus.h"
#include "../data/decode/decoder.h"
#include "../log.h"
#include "../appcontrol.h"
#include "../ui/fn.h"

using namespace std;

namespace pv {
namespace dock {

//-----------ProtocolDock
   
ProtocolDock::ProtocolDock(QWidget *parent, view::View &view, SigSession *session) :
    QScrollArea(parent),
    _view(view)
{
    _session = session;
    _cur_search_index = -1;
    _search_edited = false; 
    _pro_add_button = NULL;
    _engine_filter_sync = false;

    //-----------------------------get protocol list
    GSList *l = const_cast<GSList*>(srd_decoder_list());
    std::map<std::string, int> pro_key_table;
    QString repeatNammes;

    /* Build the protocol picker entries. For every srd decoder we always
     * create one entry; additionally, if a native C decoder is registered
     * for the same id (e.g. SPI), we split the entry in two:
     *   "<name> [C]"  -> _engine_hint = 1 (force C)
     *   "<name> [Py]" -> _engine_hint = 2 (force Python)
     * This lets the user pick the decoding engine directly from the protocol
     * list, instead of going through the per-trace context menu. Protocols
     * that don't have a C implementation keep their single, plain entry. */
    auto &c_reg = pv::cdecoders::CDecoderRegistry::instance();

    for(; l; l = l->next)
    {
        const srd_decoder *const d = (srd_decoder*)l->data;
        assert(d);

        srd_decoder *dec = (srd_decoder *)(l->data);
        const QString plain_name = QString::fromUtf8(dec->name);
        const bool has_c = c_reg.has_c_decoder_for_id(dec->id);

        if (has_c) {
            DecoderInfoItem *info_c = new DecoderInfoItem();
            info_c->_data_handle  = dec;
            info_c->_display_name = plain_name + " [C]";
            info_c->_engine_hint  = 1;
            _decoderInfoList.push_back(info_c);

            DecoderInfoItem *info_py = new DecoderInfoItem();
            info_py->_data_handle  = dec;
            info_py->_display_name = plain_name + " [Py]";
            info_py->_engine_hint  = 2;
            _decoderInfoList.push_back(info_py);
        } else {
            DecoderInfoItem *info = new DecoderInfoItem();
            info->_data_handle  = dec;
            info->_display_name = plain_name;
            info->_engine_hint  = 0;
            _decoderInfoList.push_back(info);
        }

        std::string prokey(dec->id);
        if (pro_key_table.find(prokey) != pro_key_table.end())
        {
            if (repeatNammes != "")
                repeatNammes += ",";
            repeatNammes += QString(dec->id);
        }
        else
        {
            pro_key_table[prokey] = 1;
        }
    }
    g_slist_free(l);
  
    sort(_decoderInfoList.begin(), _decoderInfoList.end(), ProtocolDock::protocol_sort_callback);
  
    if (repeatNammes != ""){
        QString err = tr("Any decoder have repeated id or name:");
        err += repeatNammes;
        MsgBox::Show(tr("error"), err.toUtf8().data());
    }

    //-----------------------------top panel
    QWidget *top_panel = new QWidget();
    top_panel->setMinimumHeight(70);
    _top_panel = top_panel;
    QWidget* bot_panel = new QWidget();

    _pro_add_button = new QPushButton(top_panel);
    _pro_add_button->setObjectName("decode_add_btn");
    _pro_add_button->setFixedSize(28, 28);
    _del_all_button = new QPushButton(top_panel);
    _del_all_button->setCheckable(true);
    _del_all_button->setObjectName("decode_del_btn");
    _del_all_button->setFixedSize(28, 28);
    const int decode_toolbar_btn_h = 28;

    _pro_keyword_edit = new KeywordLineEdit(top_panel, this);
    _pro_keyword_edit->setReadOnly(true);
    _pro_keyword_edit->setObjectName("decode_keyword_edit");
    _pro_keyword_edit->setFixedHeight(decode_toolbar_btn_h);

    _engine_both_cb = new QCheckBox(tr("Both"), top_panel);
    _engine_both_cb->setObjectName("decode_engine_both_cb");
    _engine_c_cb = new QCheckBox(tr("C"), top_panel);
    _engine_c_cb->setObjectName("decode_engine_c_cb");
    _engine_py_cb = new QCheckBox(tr("Py"), top_panel);
    _engine_py_cb->setObjectName("decode_engine_py_cb");
    _engine_both_cb->setChecked(true);

    QHBoxLayout *pro_search_lay = new QHBoxLayout();
    pro_search_lay->setSpacing(6);
    pro_search_lay->setAlignment(Qt::AlignVCenter);
    pro_search_lay->addWidget(_pro_add_button, 0, Qt::AlignVCenter);
    pro_search_lay->addWidget(_del_all_button, 0, Qt::AlignVCenter);
    pro_search_lay->addWidget(_pro_keyword_edit, 1, Qt::AlignVCenter);
    pro_search_lay->addWidget(_engine_both_cb, 0, Qt::AlignVCenter);
    pro_search_lay->addWidget(_engine_c_cb, 0, Qt::AlignVCenter);
    pro_search_lay->addWidget(_engine_py_cb, 0, Qt::AlignVCenter);
  
    // Scroll area that holds decoder item rows — allows scrolling when >4 items
    _items_container = new QWidget();
    _items_container->setObjectName("decode_items_container");
    _items_layout = new QVBoxLayout(_items_container);
    _items_layout->setContentsMargins(0, 0, 0, 12);
    _items_layout->setSpacing(6);
    _items_layout->addStretch(1);

    _items_scroll_area = new QScrollArea();
    _items_scroll_area->setObjectName("decode_items_scroll");
    _items_scroll_area->setFrameShape(QFrame::NoFrame);
    _items_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _items_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _items_scroll_area->setWidgetResizable(true);
    _items_scroll_area->setWidget(_items_container);

    _top_layout = new QVBoxLayout();
    _top_layout->setContentsMargins(8, 8, 8, 8);
    _top_layout->setSpacing(6);
    _top_layout->addLayout(pro_search_lay);
    _top_layout->addWidget(_items_scroll_area, 1);
    top_panel->setLayout(_top_layout); 
 
    //-----------------------------bottom panel
    _bot_set_button = new QPushButton(bot_panel);
    _bot_set_button->setObjectName("decode_settings_btn");
    _bot_set_button->setFixedSize(28, 28);
    _bot_save_button = new QPushButton(bot_panel);
    _bot_save_button->setObjectName("decode_save_btn");
    _bot_save_button->setFixedSize(28, 28);
    _dn_nav_button = new QPushButton(bot_panel);
    _dn_nav_button->setObjectName("decode_nav_btn");
    _dn_nav_button->setFixedSize(28, 28);
    _bot_title_label = new QLabel(bot_panel);
    _bot_title_label->setObjectName("decode_title_label");

    QHBoxLayout *bot_title_layout = new QHBoxLayout();
    bot_title_layout->setSpacing(6);
    bot_title_layout->addWidget(_bot_set_button);
    bot_title_layout->addWidget(_bot_save_button);
    bot_title_layout->addWidget(_bot_title_label, 1);
    bot_title_layout->addWidget(_dn_nav_button);
    
    _pre_button = new QPushButton(bot_panel);
    _pre_button->setObjectName("decode_prev_btn");
    _pre_button->setFixedSize(24, 24);
    _nxt_button = new QPushButton(bot_panel);
    _nxt_button->setObjectName("decode_next_btn");
    _nxt_button->setFixedSize(24, 24);

    // Search icon inside the input container (matches web's bg-[#2a2a2a] container)
    _ann_search_button = new QPushButton(bot_panel);
    _ann_search_button->setObjectName("decode_ann_search_icon");
    _ann_search_button->setFixedSize(14, 14);
    _ann_search_button->setDisabled(true);
    _ann_search_edit = new PopupLineEdit(bot_panel);
    _ann_search_edit->setObjectName("decode_ann_search_edit");

    QWidget *search_container = new QWidget(bot_panel);
    search_container->setObjectName("decode_search_container");
    auto *sc_layout = new QHBoxLayout(search_container);
    sc_layout->setContentsMargins(6, 2, 6, 2);
    sc_layout->setSpacing(4);
    sc_layout->addWidget(_ann_search_button);
    sc_layout->addWidget(_ann_search_edit, 1);

    QHBoxLayout *ann_search_layout = new QHBoxLayout();
    ann_search_layout->setSpacing(6);
    ann_search_layout->addWidget(_pre_button);
    ann_search_layout->addWidget(search_container, 1);
    ann_search_layout->addWidget(_nxt_button);

    _table_view = new QTableView(bot_panel);
    _table_view->setModel(_session->get_decoder_model());
    _table_view->setAlternatingRowColors(true);
    _table_view->setShowGrid(false);
    _table_view->horizontalHeader()->setStretchLastSection(true);
    _table_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _table_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); 

    _matchs_title_label = new QLabel();
    _matchs_title_label->setObjectName("decode_matchs_title");
    _matchs_label = new QLabel();
    _matchs_label->setObjectName("decode_matchs_count");

    // Matching items row wrapped in a widget for border-top styling
    QWidget *match_row = new QWidget(bot_panel);
    match_row->setObjectName("decode_match_row");
    QHBoxLayout *match_layout = new QHBoxLayout(match_row);
    match_layout->setContentsMargins(4, 4, 4, 4);
    match_layout->addWidget(_matchs_title_label, 0, Qt::AlignLeft);
    match_layout->addWidget(_matchs_label, 0, Qt::AlignLeft);
    match_layout->addStretch(1);

    QVBoxLayout *bot_layout = new QVBoxLayout();
    bot_layout->setContentsMargins(4, 4, 4, 4);
    bot_layout->setSpacing(6);
    bot_layout->addLayout(bot_title_layout);
    bot_layout->addLayout(ann_search_layout);
    bot_layout->addWidget(match_row);
    bot_layout->addWidget(_table_view);
    bot_panel->setLayout(bot_layout); 

    QSplitter *split_widget = new QSplitter(this);
    split_widget->insertWidget(0, top_panel);
    split_widget->insertWidget(1, bot_panel);
    split_widget->setOrientation(Qt::Vertical);
    split_widget->setCollapsible(0, false);
    split_widget->setCollapsible(1, false);

    this->setWidgetResizable(true);
    this->setWidget(split_widget);
    split_widget->setObjectName("protocolWidget");

    connect(_dn_nav_button, SIGNAL(clicked()),this, SLOT(nav_table_view()));
    connect(_bot_save_button, SIGNAL(clicked()),this, SLOT(export_table_view()));
    connect(_bot_set_button, SIGNAL(clicked()),this, SLOT(set_model()));
    connect(_pre_button, SIGNAL(clicked()),this, SLOT(search_pre()));
    connect(_nxt_button, SIGNAL(clicked()),this, SLOT(search_nxt()));
    connect(_pro_add_button, SIGNAL(clicked()),this, SLOT(on_add_protocol()));
    connect(_del_all_button, SIGNAL(clicked()),this, SLOT(on_del_all_protocol())); 

    connect(this, SIGNAL(protocol_updated()), this, SLOT(update_model()));
    connect(_table_view, SIGNAL(clicked(QModelIndex)), this, SLOT(item_clicked(QModelIndex)));

    connect(_table_view->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), 
                    this, SLOT(column_resize(int, int, int)));

    connect(_ann_search_edit, SIGNAL(editingFinished()), this, SLOT(search_changed()));

    connect(_engine_both_cb, SIGNAL(stateChanged(int)), this, SLOT(onEngineFilterChanged()));
    connect(_engine_c_cb, SIGNAL(stateChanged(int)), this, SLOT(onEngineFilterChanged()));
    connect(_engine_py_cb, SIGNAL(stateChanged(int)), this, SLOT(onEngineFilterChanged()));

    ADD_UI(this);
}

ProtocolDock::~ProtocolDock()
{
   disconnect_decode_progress_signals();

    //destroy protocol item layers
   for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++){
       DESTROY_QT_LATER(*it);
   }
   _protocol_lay_items.clear();

   //clear protocol infos list
   RELEASE_ARRAY(_decoderInfoList);

   REMOVE_UI(this);
}

void ProtocolDock::retranslateUi()
{
    _ann_search_edit->setPlaceholderText(tr("search"));
    _matchs_title_label->setText(tr("Matching Items:"));
    _bot_title_label->setText(tr("Protocol List Viewer"));
    _engine_both_cb->setText(tr("Both"));
    _engine_c_cb->setText(tr("C"));
    _engine_py_cb->setText(tr("Py"));
    _pro_keyword_edit->ResetText();
}

void ProtocolDock::reStyle()
{
    if (_pro_add_button == NULL)
        return;

    QSize iconSz(14, 14);
    _pro_add_button->setIcon(QIcon(":/icons/sidebar/plus.svg"));
    _pro_add_button->setIconSize(iconSz);
    _del_all_button->setIcon(QIcon(":/icons/sidebar/x.svg"));
    _del_all_button->setIconSize(iconSz);
    _bot_set_button->setIcon(QIcon(":/icons/sidebar/settings.svg"));
    _bot_set_button->setIconSize(iconSz);
    _bot_save_button->setIcon(QIcon(":/icons/sidebar/save.svg"));
    _bot_save_button->setIconSize(iconSz);
    _dn_nav_button->setIcon(QIcon(":/icons/sidebar/navigation.svg"));
    _dn_nav_button->setIconSize(iconSz);
    _pre_button->setIcon(QIcon(":/icons/sidebar/chevron-left.svg"));
    _pre_button->setIconSize(iconSz);
    _nxt_button->setIcon(QIcon(":/icons/sidebar/chevron-right.svg"));
    _nxt_button->setIconSize(iconSz);
    _ann_search_button->setIcon(QIcon(":/icons/sidebar/search.svg"));
    _ann_search_button->setIconSize(iconSz);

    for (auto item : _protocol_lay_items){
        item->ResetStyle();
    }
}

void ProtocolDock::disconnect_decode_progress_signals()
{
    if (_session == NULL)
        return;

    const auto &decode_sigs = _session->get_decode_signals();
    for (auto d : decode_sigs) {
        disconnect(d, SIGNAL(decoded_progress(int)), this, SLOT(decoded_progress(int)));
    }
}

void ProtocolDock::clear_protocol_list_ui()
{
    disconnect_decode_progress_signals();

    for (auto lay : _protocol_lay_items){
        _items_layout->removeItem(lay);
        DESTROY_QT_LATER(lay);
    }
    _protocol_lay_items.clear();

    while (_items_layout->count() > 1) {
        QLayoutItem *item = _items_layout->takeAt(0);
        delete item;
    }
}

void ProtocolDock::sync_protocol_list_from_session()
{
    if (_session == NULL)
        return;

    const auto &decode_sigs = _session->get_decode_signals();

    for (auto trace : decode_sigs) {
        auto stack = trace->decoder();
        if (stack == NULL || stack->stack().empty())
            continue;

        auto dec = stack->stack().front()->decoder();
        QString protocolName = trace->get_name();
        if (protocolName.isEmpty())
            protocolName = QString(dec->name);
        QString protocolId(dec->id);

        ProtocolItemLayer *layer = new ProtocolItemLayer(_items_container, protocolName, this);
        _protocol_lay_items.push_back(layer);
        _items_layout->insertLayout(_protocol_lay_items.size() - 1, layer);
        layer->m_decoderStatus = static_cast<DecoderStatus*>(stack->get_key_handel());
        layer->m_protocolId = protocolId;
        layer->_trace = trace;

        string fmt = AppConfig::Instance().GetProtocolFormat(protocolId.toStdString());
        if (fmt != "")
        {
            layer->SetProtocolFormat(fmt.c_str());
            if (layer->m_decoderStatus != NULL)
                layer->m_decoderStatus->m_format = DecoderDataFormat::Parse(fmt.c_str());
        }

        QString err;
        int pg = trace->get_progress();
        if (stack->out_of_memory())
            err = tr("Out of Memory");
        layer->SetProgress(pg, err);
        if (pg == 100 && layer->m_decoderStatus != NULL)
            layer->enable_format(layer->m_decoderStatus->m_bNumeric);

        connect(trace, SIGNAL(decoded_progress(int)), this, SLOT(decoded_progress(int)), Qt::UniqueConnection);
    }
}

pv::view::View* ProtocolDock::active_view() const
{
    if (_session == NULL)
        return NULL;

    const auto &decode_sigs = _session->get_decode_signals();
    for (auto d : decode_sigs) {
        if (d->get_view())
            return d->get_view();
    }

    for (auto s : _session->get_signals()) {
        if (s->get_view())
            return s->get_view();
    }

    return NULL;
}

void ProtocolDock::setSession(SigSession *session)
{
    if (session == NULL || session == _session)
        return;

    dsv_info("ProtocolDock::setSession %p -> %p", (void*)_session, (void*)session);

    clear_protocol_list_ui();
    _session = session;

    _table_view->setModel(_session->get_decoder_model());
    _model_proxy.setSourceModel(_session->get_decoder_model());

    sync_protocol_list_from_session();
    update_model();
    update_view_status();
    adjustPannelSize();

    _pro_keyword_edit->ResetText();
    _selected_protocol_id.clear();
    _selected_info = nullptr;

    emit protocol_updated();
}

int ProtocolDock::decoder_name_cmp(const void *a, const void *b)
{
    return strcmp(((const srd_decoder*)a)->name,
        ((const srd_decoder*)b)->name);
}

int ProtocolDock::get_protocol_index_by_id(QString id)
{
    int dex = 0;
    for (auto info : _decoderInfoList){
        srd_decoder *dec = (srd_decoder *)(info->_data_handle);
        QString proid(dec->id);
        if (id == proid){
            return dex;
        }
        ++dex;
    }
    return -1;
} 

void ProtocolDock::on_add_protocol()
{ 
     if (_decoderInfoList.size() == 0){
        MsgBox::Show(NULL, tr("Decoder list is empty!"));
        return;
    }
    if (_selected_protocol_id == ""){
        MsgBox::Show(NULL, tr("Please select a decoder!"));
        return;
    }

    /* Prefer the exact list entry the user clicked (set in OnItemClick).
     * That preserves the [C]/[Py] choice when the same id appears twice in
     * the list. Fall back to a name-based lookup if we somehow get called
     * without a recorded selection (e.g. legacy keyword flow). */
    srd_decoder *dec = nullptr;
    int engine_hint = 0;
    if (_selected_info != nullptr) {
        dec = (srd_decoder *)(_selected_info->_data_handle);
        engine_hint = _selected_info->_engine_hint;
    } else {
        int dex = this->get_protocol_index_by_id(_selected_protocol_id);
        assert(dex >= 0);
        dec = (srd_decoder *)(_decoderInfoList[dex]->_data_handle);
    }
    QString pro_id(dec->id);
    std::list<data::decode::Decoder*> sub_decoders;
    
    assert(dec->inputs);

    QString input_id = parse_protocol_id((char *)dec->inputs->data);

    if (input_id != "logic")
    {
        pro_id = ""; //reset base protocol

        int base_dex = get_output_protocol_by_id(input_id);
        sub_decoders.push_front(new data::decode::Decoder(dec));

        while (base_dex != -1)
        {
            srd_decoder *base_dec = (srd_decoder *)(_decoderInfoList[base_dex]->_data_handle);
            pro_id = QString(base_dec->id); //change base protocol           

            assert(base_dec->inputs);

            input_id = parse_protocol_id((char *)base_dec->inputs->data);

            if (input_id == "logic")
            {
                break;
            }

            sub_decoders.push_front(new data::decode::Decoder(base_dec));
            pro_id = ""; //reset base protocol
            base_dex = get_output_protocol_by_id(input_id);
        }
    }

    if (pro_id == ""){
        MsgBox::Show(tr("error"), 
                     tr("find the base decoder error!"));

        for(auto sub: sub_decoders){
            delete sub;
        }
        sub_decoders.clear();

        return;
    }

    add_protocol_by_id(pro_id, false, sub_decoders, engine_hint);
    /* one-shot: clear the cached pointer so a stale entry can't bleed into
     * the next invocation (the SearchComboBox is destroyed after a pick). */
    _selected_info = nullptr;
}

bool ProtocolDock::add_protocol_by_id(QString id, bool silent,
                                      std::list<pv::data::decode::Decoder*> &sub_decoders,
                                      int engine_hint)
{
    if (_session->get_device()->get_work_mode() != LOGIC) {
        dsv_info("Protocol Analyzer\nProtocol Analyzer is only valid in Digital Mode!");
        return false;
    }

    int dex = this->get_protocol_index_by_id(id);
    if (dex == -1){
        dsv_err("Protocol not exists! id:%s", id.toUtf8().data());
        return false;
    }

    srd_decoder *const decoder = (srd_decoder *)(_decoderInfoList[dex]->_data_handle);
    DecoderStatus *dstatus = new DecoderStatus();
    dstatus->m_format = (int)DecoderDataFormat::hex;

    QString protocolName(decoder->name);
    QString protocolId(decoder->id);

    if (sub_decoders.size()){
        auto it = sub_decoders.end();
        it--;
        protocolName = QString((*it)->decoder()->name);
        protocolId = QString((*it)->decoder()->id); 
    }

    pv::view::Trace *trace = NULL;

    if (_session->add_decoder(decoder, silent, dstatus, sub_decoders, trace) == false){
        return false;
    }

    /* Apply the forced engine hint, if any, to the decoder stack that
     * SigSession::add_decoder just created. The constructor of DecoderStack
     * defaults to "use C if registered", so explicit Python (hint == 2)
     * needs to override that. A hint of 1 (force C) is a no-op when the C
     * decoder exists, but we still set it explicitly so the contract is
     * obvious. */
    if (engine_hint != 0) {
        pv::view::DecodeTrace *dt = dynamic_cast<pv::view::DecodeTrace*>(trace);
        if (dt && dt->decoder()
            && pv::cdecoders::CDecoderRegistry::instance()
                   .has_c_decoder_for_id(decoder->id)) {
            dt->decoder()->set_use_c_decoder(engine_hint == 1);
        }
    }

    // create item layer — parent is _items_container so child widgets live there
    ProtocolItemLayer *layer = new ProtocolItemLayer(_items_container, protocolName, this);
    _protocol_lay_items.push_back(layer);
    // insert before the trailing stretch (stretch is always the last item)
    _items_layout->insertLayout(_protocol_lay_items.size() - 1, layer);
    layer->m_decoderStatus = dstatus; 
    layer->m_protocolId = protocolId;
    layer->_trace = trace;

    // set current protocol format
    string fmt = AppConfig::Instance().GetProtocolFormat(protocolId.toStdString());
    if (fmt != "")
    {
        layer->SetProtocolFormat(fmt.c_str());
        dstatus->m_format = DecoderDataFormat::Parse(fmt.c_str());
    }

    // progress connection
    const auto &decode_sigs = _session->get_decode_signals();   
    protocol_updated();
    connect(decode_sigs.back(), SIGNAL(decoded_progress(int)), this, SLOT(decoded_progress(int)), Qt::UniqueConnection);

    adjustPannelSize();

    return true;
}
 
 void ProtocolDock::on_del_all_protocol(){
     if (_protocol_lay_items.size() == 0){
        MsgBox::Show(NULL, tr("Have no decoder to remove!"), this);
        return;
     }

    QString strMsg(tr("Are you sure to remove all decoder?"));
    if (MsgBox::Confirm(strMsg,  this)){
        del_all_protocol();
    }
 } 

void ProtocolDock::del_all_protocol()
{  
    if (_protocol_lay_items.size() > 0)
    {
        clear_protocol_list_ui();
        _session->clear_all_decoder();
        this->update();
        protocol_updated();

        adjustPannelSize();
    }
}

void ProtocolDock::del_protocols_using_channels(const QSet<int> &disabled_channel_indices)
{
    // Policy: NEVER auto-delete the user's decoders just because some channel
    // went away. Even if a decoder now references a disabled channel, the
    // decoder will surface a "required channels not specified" error on the
    // next Start, at which point the user can reconfigure or remove it
    // intentionally. Silent destruction of user state is worse than a clear
    // error.
    //
    // We keep this slot wired to DeviceOptionsDock::sig_channels_applied so
    // any future per-decoder "channel binding became stale" handling has a
    // single funnel point. For now we only log a hint when the set is
    // non-empty, so the user can correlate "decoder later complained" with
    // "I disabled channel N" in the log.
    if (disabled_channel_indices.isEmpty() || _session == nullptr)
        return;

    QStringList parts;
    for (int idx : disabled_channel_indices)
        parts << QString::number(idx);
    const QString joined = parts.join(",");

    const auto &decode_sigs = _session->get_decode_signals();
    for (auto sig : decode_sigs) {
        if (!sig || !sig->decoder())
            continue;
        const auto &stack = sig->decoder()->stack();
        for (auto dec : stack) {
            if (!dec) continue;
            for (const srd_channel *pdch : dec->binded_probe_list()) {
                const int sig_idx = dec->binded_probe_index(pdch);
                if (sig_idx >= 0 && disabled_channel_indices.contains(sig_idx)) {
                    dsv_info("Protocol decoder '%s' references channel %d which was "
                             "just disabled; the decoder is preserved and will report "
                             "'required channels not specified' on next Start unless "
                             "reconfigured. (disabled channels: %s)",
                             dec->get_dec_handel() ? dec->get_dec_handel()->id : "?",
                             sig_idx,
                             joined.toUtf8().constData());
                    break;
                }
            }
        }
    }
}

void ProtocolDock::decoded_progress(int progress)
{
    const auto &decode_sigs = _session->get_decode_signals();
    unsigned int index = 0;

    for(auto d : decode_sigs) {
        int pg = d->get_progress();
        QString err;

        if (d->decoder()->out_of_memory())
            err = tr("Out of Memory");

        if (index < _protocol_lay_items.size())
        {
            ProtocolItemLayer &lay =  *(_protocol_lay_items.at(index));
            lay.SetProgress(pg, err);
            
            // have custom data format
            if (pg == 100 && lay.m_decoderStatus != NULL){
                lay.enable_format(lay.m_decoderStatus->m_bNumeric);
            }
        }

        index++;
    }

    if (progress == 0 || progress % 10 == 1){
        update_model();
    }  
}

void ProtocolDock::set_model()
{
    pv::dialogs::ProtocolList *protocollist_dlg = new pv::dialogs::ProtocolList(this, _session);
    protocollist_dlg->exec();
    resize_table_view(_session->get_decoder_model());
    _model_proxy.setSourceModel(_session->get_decoder_model());
    search_done();

    // clear mark_index of all DecoderStacks
    const auto &decode_sigs = _session->get_decode_signals();
        
    for(auto d : decode_sigs) {
        d->decoder()->set_mark_index(-1);
    }
}

void ProtocolDock::update_model()
{
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    const auto &decode_sigs = _session->get_decode_signals();

    if (decode_sigs.size() == 0)
        decoder_model->setDecoderStack(NULL);
    else if (!decoder_model->getDecoderStack())
        decoder_model->setDecoderStack(decode_sigs.at(0)->decoder());
    else {
        unsigned int index = 0;
        for(auto d : decode_sigs) {
            if (d->decoder() == decoder_model->getDecoderStack()) {
                decoder_model->setDecoderStack(d->decoder());
                break;
            }
            index++;
        }
        if (index >= decode_sigs.size())
            decoder_model->setDecoderStack(decode_sigs.at(0)->decoder());
    }
    _model_proxy.setSourceModel(decoder_model);
    search_done();
    resize_table_view(decoder_model);
}

void ProtocolDock::resize_table_view(data::DecoderModel* decoder_model)
{
    if (decoder_model->getDecoderStack()) {
        for (int i = 0; i < decoder_model->columnCount(QModelIndex()) - 1; i++) {
            _table_view->resizeColumnToContents(i);
            if (_table_view->columnWidth(i) > 200)
                _table_view->setColumnWidth(i, 200);
        }
        int top_row = _table_view->rowAt(0);
        int bom_row = _table_view->rowAt(_table_view->height());
        if (bom_row >= top_row && top_row >= 0) {
            for (int i = top_row; i <= bom_row; i++)
                _table_view->resizeRowToContents(i);
        }
    }
}

void ProtocolDock::item_clicked(const QModelIndex &index)
{
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();

    if (decoder_stack) {
        pv::data::decode::Annotation ann;
        if (decoder_stack->list_annotation(&ann, index.column(), index.row())) {
            const auto &decode_sigs = _session->get_decode_signals();

            for(auto d : decode_sigs) {
                d->decoder()->set_mark_index(-1);
            }

            decoder_stack->set_mark_index((ann.start_sample()+ann.end_sample())/2);
            _session->show_region(ann.start_sample(), ann.end_sample(), false);
        }
    }

    _table_view->resizeRowToContents(index.row());
    if (index.column() != _model_proxy.filterKeyColumn()) {
        _model_proxy.setFilterKeyColumn(index.column());
        _model_proxy.setSourceModel(decoder_model);
        search_done();
    }
    QModelIndex filterIndex = _model_proxy.mapFromSource(index);
    if (filterIndex.isValid()) {
        _cur_search_index = filterIndex.row();
    } else {
        if (_model_proxy.rowCount() == 0) {
            _cur_search_index = -1;
        } else {
            uint64_t up = 0;
            uint64_t dn = _model_proxy.rowCount() - 1;
            do {
                uint64_t md = (up + dn)/2;
                QModelIndex curIndex = _model_proxy.mapToSource(_model_proxy.index(md,_model_proxy.filterKeyColumn()));
                if (index.row() == curIndex.row()) {
                    _cur_search_index = md;
                    break;
                } else if (md == up) {
                    if (curIndex.row() < index.row() && up < dn) {
                        QModelIndex nxtIndex = _model_proxy.mapToSource(_model_proxy.index(md+1,_model_proxy.filterKeyColumn()));
                        if (nxtIndex.row() < index.row())
                            md++;
                    }
                    _cur_search_index = md + ((curIndex.row() < index.row()) ? 0.5 : -0.5);
                    break;
                } else if (curIndex.row() < index.row()) {
                    up = md;
                } else if (curIndex.row() > index.row()) {
                    dn = md;
                }
            }while(1);
        }
    }
}

void ProtocolDock::column_resize(int index, int old_size, int new_size)
{
    (void)index;
    (void)old_size;
    (void)new_size;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    if (decoder_model->getDecoderStack()) {
        int top_row = _table_view->rowAt(0);
        int bom_row = _table_view->rowAt(_table_view->height());
        if (bom_row >= top_row && top_row >= 0) {
            for (int i = top_row; i <= bom_row; i++)
                _table_view->resizeRowToContents(i);
        }
    }
}

void ProtocolDock::export_table_view()
{
    pv::dialogs::ProtocolExp *protocolexp_dlg = new pv::dialogs::ProtocolExp(this, _session);
    protocolexp_dlg->exec();
}

void ProtocolDock::nav_table_view()
{
    uint64_t row_index = 0;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();
    if (decoder_stack) {
        auto view = active_view();
        if (view == NULL)
            return;

        uint64_t offset = view->offset() * (decoder_stack->samplerate() * view->scale());
        std::map<const pv::data::decode::Row, bool> rows = decoder_stack->get_rows_lshow();
        int column = _model_proxy.filterKeyColumn();
        for (std::map<const pv::data::decode::Row, bool>::const_iterator i = rows.begin();
            i != rows.end(); i++) {
            if ((*i).second && column-- == 0) {
                row_index = decoder_stack->get_annotation_index((*i).first, offset);
                break;
            }
        }
        QModelIndex index = _model_proxy.mapToSource(_model_proxy.index(row_index, _model_proxy.filterKeyColumn()));

        if(index.isValid()){         

            pv::data::decode::Annotation ann;

            if (decoder_stack->list_annotation(&ann, index.column(), index.row()))
            {
                _table_view->scrollTo(index);
                _table_view->setCurrentIndex(index);

                 const auto &decode_sigs = _session->get_decode_signals();

                for(auto d : decode_sigs) {
                    d->decoder()->set_mark_index(-1);
                }

                decoder_stack->set_mark_index((ann.start_sample()+ann.end_sample())/2);
                view->set_all_update(true);
                view->update();
            }           
        }
    }
}

void ProtocolDock::search_pre()
{
    search_update();
    // now the proxy only contains rows that match the name
    // let's take the pre one and map it to the original model
    if (_model_proxy.rowCount() == 0) {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
        return;
    }
    int i = 0;
    uint64_t rowCount = _model_proxy.rowCount();
    QModelIndex matchingIndex;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();
    do {
        _cur_search_index--;
        if (_cur_search_index <= -1 || _cur_search_index >= _model_proxy.rowCount())
            _cur_search_index = _model_proxy.rowCount() - 1;

        matchingIndex = _model_proxy.mapToSource(_model_proxy.index(ceil(_cur_search_index),_model_proxy.filterKeyColumn()));
        if (!decoder_stack || !matchingIndex.isValid())
            break;
        i = 1;
        uint64_t row = matchingIndex.row() + 1;
        uint64_t col = matchingIndex.column();
        pv::data::decode::Annotation ann;
        bool ann_valid = false;

        while(i < _str_list.size()) {
            QString nxt = _str_list.at(i);

            do {
                ann_valid = decoder_stack->list_annotation(&ann, col, row);
                row++;
            }
            while(ann_valid && !ann.is_numberic());

            if (ann_valid){
                QString source = ann.annotations().at(0);
                if (source.contains(nxt))
                    i++;
                else
                    break;
            }
            else{
                break;
            }
        }
    }
    while(i < _str_list.size() && --rowCount);

    if(i >= _str_list.size() && matchingIndex.isValid()){
        _table_view->scrollTo(matchingIndex);
        _table_view->setCurrentIndex(matchingIndex);
        _table_view->clicked(matchingIndex);
    } else {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
    }
}

void ProtocolDock::search_nxt()
{
    search_update();
    // now the proxy only contains rows that match the name
    // let's take the pre one and map it to the original model
    if (_model_proxy.rowCount() == 0) {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
        return;
    }

    int i = 0;
    uint64_t rowCount = _model_proxy.rowCount();
    QModelIndex matchingIndex;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    auto decoder_stack = decoder_model->getDecoderStack();

    if (decoder_stack == NULL){ 
        dsv_err("decoder_stack is null");
        return;
    }  

    do {
        _cur_search_index++;
        if (_cur_search_index < 0 || _cur_search_index >= _model_proxy.rowCount())
            _cur_search_index = 0;

        matchingIndex = _model_proxy.mapToSource(_model_proxy.index(floor(_cur_search_index),_model_proxy.filterKeyColumn()));
        
        if (!matchingIndex.isValid())
            break;

        i = 1;
        uint64_t row = matchingIndex.row() + 1;
        uint64_t col = matchingIndex.column();
        pv::data::decode::Annotation ann;
        bool ann_valid = false;

        while(i < _str_list.size()) {
            QString nxt = _str_list.at(i);

            do {
                ann_valid = decoder_stack->list_annotation(&ann, col, row);
                row++;
            }
            while(ann_valid && !ann.is_numberic());

            if (ann_valid){
                QString source = ann.annotations().at(0);
                if (source.contains(nxt))
                    i++;
                else
                    break;
                }
            else{
                break;
            }            
        }
    }while(i < _str_list.size() && --rowCount);

    if(i >= _str_list.size() && matchingIndex.isValid()){
        _table_view->scrollTo(matchingIndex);
        _table_view->setCurrentIndex(matchingIndex);
        _table_view->clicked(matchingIndex);
    } else {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
    }
}

void ProtocolDock::search_done()
{
    QString str = _ann_search_edit->text().trimmed();
    QRegularExpression rx("(-)");
    _str_list = str.split(rx);
    _model_proxy.setFilterFixedString(_str_list.first());
    if (_str_list.size() > 1)
        _matchs_label->setText("...");
    else
        _matchs_label->setText(QString::number(_model_proxy.rowCount()));
}

void ProtocolDock::search_changed()
{
    _search_edited = true;
    _matchs_label->setText("...");
}

void ProtocolDock::search_update()
{
    if (!_search_edited)
        return;

    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();
    if (!decoder_stack)
        return;

    if (decoder_stack->list_annotation_size(_model_proxy.filterKeyColumn()) > ProgressRows) {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            search_done();
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Searching..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);

        dlg.exec();
    } else {
        search_done();
    }
    _search_edited = false;
}

 //-------------------IProtocolItemLayerCallback
void ProtocolDock::OnProtocolSetting(void *handle){
  
    for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++){
       if ((*it) == handle){ 
             void *key_handel = (*it)->get_protocol_key_handel();
            _session->rst_decoder_by_key_handel(key_handel);
            protocol_updated();
           break;
       } 
   }
}

void ProtocolDock::OnProtocolDelete(void *handle){
    QString strMsg(tr("Are you sure to remove this decoder?"));

    if (!MsgBox::Confirm(strMsg, this)){
        return;
    } 

    for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++)
    {
        if ((*it) == handle)
        {
            auto lay = (*it); 
            void *key_handel = lay->get_protocol_key_handel();
            _protocol_lay_items.erase(it);
            DESTROY_QT_LATER(lay);
            _session->remove_decoder_by_key_handel(key_handel);     
            protocol_updated();
            break;
        } 
    }

    adjustPannelSize();
}

void ProtocolDock::OnProtocolFormatChanged(QString format, void *handle){
    for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++){
       if ((*it) == handle){
           auto lay = (*it); 
           AppConfig::Instance().SetProtocolFormat(lay->m_protocolId.toStdString(), format.toStdString());

           if (lay->m_decoderStatus != NULL)
           {
                  lay->m_decoderStatus->m_format = DecoderDataFormat::Parse(format.toStdString().c_str());
                  protocol_updated();
           }
        
           break;
       }
   }
} 
  

bool ProtocolDock::protocol_sort_callback(const DecoderInfoItem *o1, const DecoderInfoItem *o2)
{
    /* Sort by display name (which already carries the [C]/[Py] suffix when
     * applicable) so the two engine variants of the same protocol stay next
     * to each other in the list. Fall back to the raw decoder name if the
     * display name is somehow empty. */
    QByteArray b1 = o1->_display_name.isEmpty()
        ? QByteArray(((srd_decoder *)(o1->_data_handle))->name)
        : o1->_display_name.toUtf8();
    QByteArray b2 = o2->_display_name.isEmpty()
        ? QByteArray(((srd_decoder *)(o2->_data_handle))->name)
        : o2->_display_name.toUtf8();
    const char *s1 = b1.constData();
    const char *s2 = b2.constData();
    char c1 = 0;
    char c2 = 0;

    while (*s1 && *s2)
    {
        c1 = *s1;
        c2 = *s2;

        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;                               

        if (c1 > c2)
            return false;
        else if (c1 < c2)
            return true;
        
        s1++;
        s2++;
    }

    if (*s1)
        return false;
    else if (*s2)
        return true;

    return true;
}

 QString ProtocolDock::parse_protocol_id(const char *id)
 {
     if (id == NULL || *id == 0){
         assert(false);
     }
     char buf[25];
     strncpy(buf, id, sizeof(buf)-1);
     char *rd = buf;
     char *start = NULL;
     unsigned int len = 0;

     while (*rd && len - 1 < sizeof(buf))
     { 
         if (*rd == '['){
             start = rd++;
         }
         else if (*rd == ']'){
             *rd = 0;
             break;
         }
         ++rd;
         len++;
     }
     if (start == NULL){
         start = const_cast<char*>(id);
     }

     return QString(start);
 }

  int ProtocolDock::get_output_protocol_by_id(QString id)
  {
      int dex = 0;

      for (auto info : _decoderInfoList)
      {
          srd_decoder *dec = (srd_decoder *)(info->_data_handle);
          if (dec->outputs)
          {
              QString output_id = parse_protocol_id((char*)dec->outputs->data);
              if (output_id == id)
              {
                  QString proid(dec->id);
                  if (!proid.startsWith("0:") || output_id == proid){
                      return dex;
                  } 
              }
          }
         
          ++dex;
      }

      return -1;
  }

  void ProtocolDock::BeginEditKeyword()
  {
      show_protocol_select();
  }

  void ProtocolDock::show_protocol_select()
  {
      // Top-level so move() uses screen coordinates (ProtocolDock is a QScrollArea).
      SearchComboBox *panel = new SearchComboBox(nullptr);

      for (auto info : _decoderInfoList)
      {
          srd_decoder *dec = (srd_decoder *)(info->_data_handle);
          /* Use the suffixed display name (e.g. "SPI [C]" / "SPI [Py]")
           * for the visible label so the engine choice is part of the list
           * itself. Keep the raw id for keyword search matching. */
          panel->AddDataItem(QString(dec->id), info->_display_name, info);
      }

      QFont font = this->font();
      font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
      ui::set_form_font(panel, font);

      panel->SetItemClickHandle(this);
      panel->SetEngineFilter([this](void *handle) {
          return passEngineFilter(static_cast<DecoderInfoItem*>(handle));
      });

      _active_picker = panel;
      connect(panel, &QObject::destroyed, this, [this]() {
          _active_picker = nullptr;
      });

      QWidget *anchor = _pro_keyword_edit;
      panel->ShowDlg(anchor);
      panel->RefreshFilter();
  }

 bool ProtocolDock::passEngineFilter(const DecoderInfoItem *info) const
 {
     if (info == NULL || _engine_both_cb == NULL)
         return true;

     const bool both = _engine_both_cb->isChecked();
     const bool c    = _engine_c_cb->isChecked();
     const bool py   = _engine_py_cb->isChecked();

     if (both || (!c && !py))
         return true;

     switch (info->_engine_hint) {
     case 1:
         return c;
     case 2:
         return py;
     default:
         return py;
     }
 }

 void ProtocolDock::onEngineFilterChanged()
 {
     if (_engine_filter_sync || _engine_both_cb == NULL)
         return;

     QCheckBox *sender_cb = qobject_cast<QCheckBox*>(sender());
     _engine_filter_sync = true;

     if (sender_cb == _engine_both_cb && _engine_both_cb->isChecked()) {
         _engine_c_cb->setChecked(false);
         _engine_py_cb->setChecked(false);
     } else if (sender_cb != _engine_both_cb) {
         if (_engine_c_cb->isChecked() || _engine_py_cb->isChecked())
             _engine_both_cb->setChecked(false);
     }

     if (!_engine_both_cb->isChecked()
         && !_engine_c_cb->isChecked()
         && !_engine_py_cb->isChecked()) {
         _engine_both_cb->setChecked(true);
     }

     _engine_filter_sync = false;

     if (_active_picker)
         _active_picker->RefreshFilter();
 }

 void ProtocolDock::OnItemClick(void *sender, void *data_handle)
 {
    (void)sender;

     if (data_handle != NULL){
         DecoderInfoItem *info = (DecoderInfoItem*)data_handle;
         srd_decoder *dec = (srd_decoder *)(info->_data_handle); 
         this->_pro_keyword_edit->SetInputText(info->_display_name);
         _selected_protocol_id = QString(dec->id);
         /* Remember the exact list entry the user clicked so on_add_protocol
          * can read its _engine_hint; the id alone no longer disambiguates
          * once the C/Py split puts two entries with the same id in the list. */
         _selected_info = info;
         this->on_add_protocol();       
     }
 }

 void ProtocolDock::reset_view()
 {
    decoded_progress(0);
    update();
 }

 void ProtocolDock::update_view_status()
 {
    bool bEnable = _session->is_working() == false;
    _pro_keyword_edit->setEnabled(bEnable);
    _pro_add_button->setEnabled(bEnable);
    if (_engine_both_cb != NULL) {
        _engine_both_cb->setEnabled(bEnable);
        _engine_c_cb->setEnabled(bEnable);
        _engine_py_cb->setEnabled(bEnable);
    }
 }

 void ProtocolDock::update_deocder_item_name(void *trace_handel, const char *name)
 {
    for(auto p : _protocol_lay_items){
        if (p->_trace == trace_handel){
            p->set_label_name(QString(name));
            break;
        }
    }
 }

void ProtocolDock::UpdateLanguage()
{
    retranslateUi();
}

void ProtocolDock::UpdateTheme()
{
    reStyle();
}

void ProtocolDock::UpdateFont()
 {
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    ui::set_form_font(this, font);
    _table_view->setFont(font);

    for(auto lay : _protocol_lay_items){
        lay->update_font();
    }

    font.setPointSizeF(font.pointSizeF() + 1);
    this->parentWidget()->setFont(font);
    _table_view->horizontalHeader()->setFont(font);
    _table_view->verticalHeader()->setFont(font);

    
    _table_view->setObjectName("DecodedDataView");
    QString style = "#DecodedDataView QHeaderView{font-size: %1pt}";
    style = style.arg(AppConfig::Instance().appOptions.fontSize);
    _table_view->setStyleSheet(style);

    adjustPannelSize(); 
 }

 void ProtocolDock::adjustPannelSize()
 {
    QString str = "DECODER";
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    QFontMetrics fm(font);
    QRect rc = fm.boundingRect(str); 

    int lineHeight = rc.height() + 15;
    const int toolbarRowH = 28;
    _pro_keyword_edit->setFixedHeight(toolbarRowH);

    // Scroll area must always show at least 4 items; more items scroll.
    const int minVisibleItems = 4;
    int minScrollH = minVisibleItems * lineHeight;
    _items_scroll_area->setMinimumHeight(minScrollH);

    // Top panel minimum = search bar row + scroll area min + layout margins + spacing
    // margins: contentsMargins top(8) + bottom(8), spacing between rows(6)
    int topPanelMin = toolbarRowH + minScrollH + 8 + 8 + 6;
    _top_panel->setMinimumHeight(topPanelMin);
 }

} // namespace dock
} // namespace pv
