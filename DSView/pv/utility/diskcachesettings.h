#pragma once

#include <cstdint>

#include <QString>
#include <QSettings>
#include <QStandardPaths>

namespace pv {
namespace utility {

struct DiskCacheSettings {
    bool enabled = false;
    uint64_t ram_limit_gb = 4; // 0 = 32 MB test window
    uint64_t disk_limit_gb = 256; // 0 = unlimited
    QString spill_dir;

    void load()
    {
        QSettings s;
        enabled = s.value("diskcache/enabled", enabled).toBool();
        ram_limit_gb = s.value("diskcache/ram_limit_gb", (qulonglong)ram_limit_gb).toULongLong();
        disk_limit_gb = s.value("diskcache/disk_limit_gb", (qulonglong)disk_limit_gb).toULongLong();
        spill_dir = s.value("diskcache/spill_dir",
                            QStandardPaths::writableLocation(QStandardPaths::TempLocation)).toString();
    }

    void save() const
    {
        QSettings s;
        s.setValue("diskcache/enabled", enabled);
        s.setValue("diskcache/ram_limit_gb", QVariant::fromValue((qulonglong)ram_limit_gb));
        s.setValue("diskcache/disk_limit_gb", QVariant::fromValue((qulonglong)disk_limit_gb));
        s.setValue("diskcache/spill_dir", spill_dir);
    }
};

} // namespace utility
} // namespace pv
