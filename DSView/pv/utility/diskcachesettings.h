#pragma once

#include <cstdint>

#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

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

    static QString format_ram_limit_text(uint64_t gb)
    {
        return gb == 0 ? QStringLiteral("32 MB") : format_gb_text(gb);
    }

    static QString format_disk_limit_text(uint64_t gb)
    {
        return gb == 0 ? QStringLiteral("Unlimited") : format_gb_text(gb);
    }

    static bool parse_ram_limit_text(const QString& text, uint64_t& out_gb)
    {
        QString t = normalized_size_text(text);
        if (t == QStringLiteral("32mb")) {
            out_gb = 0;
            return true;
        }
        return parse_positive_gb(t, out_gb);
    }

    static bool parse_disk_limit_text(const QString& text, uint64_t& out_gb)
    {
        QString t = normalized_size_text(text);
        if (t == QStringLiteral("unlimited")) {
            out_gb = 0;
            return true;
        }
        return parse_positive_gb(t, out_gb);
    }

private:
    static QString normalized_size_text(const QString& text)
    {
        QString t = text.trimmed().toLower();
        t.remove(QLatin1Char(' '));
        return t;
    }

    static bool parse_positive_gb(const QString& normalized, uint64_t& out_gb)
    {
        QString digits = normalized;
        if (digits.endsWith(QStringLiteral("gb")))
            digits.chop(2);
        if (digits.isEmpty())
            return false;

        bool ok = false;
        qulonglong value = digits.toULongLong(&ok);
        if (!ok || value == 0)
            return false;

        out_gb = static_cast<uint64_t>(value);
        return true;
    }

    static QString format_gb_text(uint64_t gb)
    {
        if (gb >= 1024 && gb % 1024 == 0)
            return QStringLiteral("%1 TB").arg(gb / 1024);
        return QStringLiteral("%1 GB").arg(gb);
    }
};

} // namespace utility
} // namespace pv
