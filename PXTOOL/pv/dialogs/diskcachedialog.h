#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include "dsdialog.h"
#include "../utility/diskcachesettings.h"

namespace pv {
namespace dialogs {

class DiskCacheDialog : public DSDialog {
    Q_OBJECT
public:
    explicit DiskCacheDialog(QWidget* parent = nullptr);
    void get_settings(pv::utility::DiskCacheSettings& s) const;
    void set_settings(const pv::utility::DiskCacheSettings& s);

private slots:
    void on_browse_dir();
    void on_enabled_changed(int state);

private:
    QCheckBox    *_chk_enabled;
    QComboBox    *_cmb_ram;
    QComboBox    *_cmb_disk;
    QLineEdit    *_edt_dir;
    QPushButton  *_btn_browse;
};

} // namespace dialogs
} // namespace pv
