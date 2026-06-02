#include "diskcachedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QBoxLayout>

namespace pv {
namespace dialogs {

static const uint64_t RAM_GB[]  = {1, 2, 4, 8, 16};
static const uint64_t DISK_GB[] = {64, 128, 256, 512, 1024, 0};

DiskCacheDialog::DiskCacheDialog(QWidget* parent)
    : DSDialog(parent, false, false)
{
    setObjectName("diskCacheSettingsDialog");
    setMinimumWidth(500);
    setTitle(tr("Disk Cache Settings"));
    setTitleTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    SetTitleSpace(8);
    layout()->setSpacing(0);
    layout()->setDirection(QBoxLayout::TopToBottom);
    layout()->setAlignment(Qt::AlignTop);
    layout()->setContentsMargins(0, 5, 0, 0);

    auto *top_sep = new QWidget(this);
    top_sep->setObjectName("device_options_divider");
    top_sep->setFixedHeight(1);
    layout()->addWidget(top_sep);

    _chk_enabled = new QCheckBox(tr("Enable disk cache (extends capture depth beyond RAM)"));
    _cmb_ram = new QComboBox;
    _cmb_disk = new QComboBox;
    _cmb_ram->setObjectName("disk_cache_combo");
    _cmb_disk->setObjectName("disk_cache_combo");
    _edt_dir = new QLineEdit;
    _btn_browse = new QPushButton(tr("Browse..."));
    _btn_browse->setObjectName("device_cancel_btn");
    _btn_browse->setMinimumWidth(90);

    _cmb_ram->addItem(QString("32 MB"), QVariant((qulonglong)0));
    for (uint64_t gb : RAM_GB)
        _cmb_ram->addItem(QString("%1 GB").arg(gb), QVariant((qulonglong)gb));
    for (uint64_t gb : DISK_GB) {
        if (gb == 0) _cmb_disk->addItem(tr("Unlimited"), QVariant((qulonglong)0));
        else if (gb >= 1024) _cmb_disk->addItem(QString("%1 TB").arg(gb / 1024), QVariant((qulonglong)gb));
        else _cmb_disk->addItem(QString("%1 GB").arg(gb), QVariant((qulonglong)gb));
    }

    auto *form = new QFormLayout;
    form->addRow(tr("RAM hot window:"), _cmb_ram);
    form->addRow(tr("Disk total depth:"), _cmb_disk);

    auto *dir_row = new QHBoxLayout;
    dir_row->setSpacing(8);
    dir_row->setContentsMargins(0, 0, 0, 0);
    dir_row->addWidget(_edt_dir);
    dir_row->addWidget(_btn_browse);
    form->addRow(tr("Cache directory:"), dir_row);

    auto *grp = new QGroupBox;
    grp->setLayout(form);

    connect(_chk_enabled, &QCheckBox::stateChanged, this, &DiskCacheDialog::on_enabled_changed);
    connect(_btn_browse, &QPushButton::clicked, this, &DiskCacheDialog::on_browse_dir);

    auto *main_layout = new QVBoxLayout;
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(12);
    main_layout->addWidget(_chk_enabled);
    main_layout->addWidget(grp);
    layout()->addLayout(main_layout);

    auto *bot_sep = new QWidget(this);
    bot_sep->setObjectName("device_options_divider");
    bot_sep->setFixedHeight(1);
    layout()->addWidget(bot_sep);

    auto *cancel_btn = new QPushButton(tr("Cancel"), this);
    cancel_btn->setObjectName("device_cancel_btn");
    auto *ok_btn = new QPushButton(tr("OK"), this);
    ok_btn->setObjectName("device_ok_btn");

    auto *footer_lay = new QHBoxLayout;
    footer_lay->setContentsMargins(12, 10, 12, 10);
    footer_lay->setSpacing(6);
    footer_lay->addStretch();
    footer_lay->addWidget(ok_btn);
    footer_lay->addWidget(cancel_btn);
    layout()->addLayout(footer_lay);

    connect(ok_btn, &QPushButton::clicked, this, &DiskCacheDialog::slotAccept);
    connect(cancel_btn, &QPushButton::clicked, this, &DiskCacheDialog::slotReject);

    _edt_dir->setPlaceholderText(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    on_enabled_changed(Qt::Unchecked);
}

void DiskCacheDialog::on_enabled_changed(int state)
{
    bool en = (state == Qt::Checked);
    _cmb_ram->setEnabled(en);
    _cmb_disk->setEnabled(en);
    _edt_dir->setEnabled(en);
    _btn_browse->setEnabled(en);
}

void DiskCacheDialog::on_browse_dir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select cache directory"), _edt_dir->text());
    if (!dir.isEmpty())
        _edt_dir->setText(dir);
}

void DiskCacheDialog::get_settings(pv::utility::DiskCacheSettings& s) const
{
    s.enabled = _chk_enabled->isChecked();
    s.ram_limit_gb = _cmb_ram->currentData().toULongLong();
    s.disk_limit_gb = _cmb_disk->currentData().toULongLong();
    s.spill_dir = _edt_dir->text().trimmed();
}

void DiskCacheDialog::set_settings(const pv::utility::DiskCacheSettings& s)
{
    _chk_enabled->setChecked(s.enabled);
    for (int i = 0; i < _cmb_ram->count(); ++i)
        if (_cmb_ram->itemData(i).toULongLong() == s.ram_limit_gb) { _cmb_ram->setCurrentIndex(i); break; }
    for (int i = 0; i < _cmb_disk->count(); ++i)
        if (_cmb_disk->itemData(i).toULongLong() == s.disk_limit_gb) { _cmb_disk->setCurrentIndex(i); break; }
    _edt_dir->setText(s.spill_dir);
    on_enabled_changed(s.enabled ? Qt::Checked : Qt::Unchecked);
}

} // namespace dialogs
} // namespace pv
