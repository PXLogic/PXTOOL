/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "row.h"

#include <libsigrokdecode.h>
#include <assert.h>
#include "c_decoder_registry.h"

namespace pv {
namespace data {
namespace decode {

namespace {

QString decoder_base_name(const srd_decoder *decoder)
{
    QString name = QString::fromUtf8((decoder && decoder->name) ? decoder->name : "");
    name = name.trimmed();

    if (name.endsWith("(C)", Qt::CaseInsensitive))
        name.chop(3);
    else if (name.endsWith(" [C]", Qt::CaseInsensitive))
        name.chop(4);

    return name.trimmed();
}

bool has_native_c_decoder_counterpart(const srd_decoder *decoder)
{
    if (!decoder || decoder->is_c_decoder)
        return false;

    const QString base_name = decoder_base_name(decoder);
    if (base_name.isEmpty())
        return false;

    for (const GSList *l = srd_decoder_list(); l; l = l->next) {
        const srd_decoder *candidate = static_cast<const srd_decoder *>(l->data);
        if (!candidate || !candidate->is_c_decoder)
            continue;

        if (decoder_base_name(candidate).compare(base_name, Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

QString decoder_display_name(const srd_decoder *decoder)
{
    QString name = QString::fromUtf8((decoder && decoder->name) ? decoder->name : "");
    if (decoder && pv::cdecoders::CDecoderRegistry::instance().has_c_decoder_for_id(decoder->id))
        return name;

    if (decoder && !decoder->is_c_decoder && has_native_c_decoder_counterpart(decoder))
        return name + "(Py)";

    return name;
}

} // namespace

Row::Row() :
	_decoder(NULL),
    _row(NULL),
    _order(-1)
{
}

Row::Row(const srd_decoder *decoder, const srd_decoder_annotation_row *row, const int order) :
	_decoder(decoder),
    _row(row),
    _order(order)
{
} 

Row::Row(const Row &o)
{
	_decoder = o._decoder;
	_row = o._row;
	_order = o._order;
}

Row& Row::operator=(const Row &o)
{
	_decoder = o._decoder;
	_row = o._row;
	_order = o._order;
	return (*this);
}

QString Row::title() const
{
	if (_decoder && _decoder->name && _row && _row->desc)
		return QString("%1: %2")
			.arg(decoder_display_name(_decoder))
			.arg(QString::fromUtf8(_row->desc));

	if (_decoder && _decoder->name)
		return decoder_display_name(_decoder);

	if (_row && _row->desc)
		return QString::fromUtf8(_row->desc);

	return QString();
}

QString Row::title_id() const
{
	if (_decoder && _decoder->id && _row && _row->desc)
		return QString("%1: %2")
			.arg(QString::fromUtf8(_decoder->id))
			.arg(QString::fromUtf8(_row->desc));

	if (_decoder && _decoder->id)
		return QString::fromUtf8(_decoder->id);

	if (_row && _row->desc)
		return QString::fromUtf8(_row->desc);

	return QString();
}

bool Row::operator<(const Row &other) const
{
    assert(_decoder);

    return (_decoder < other._decoder) ||
        (_decoder == other._decoder && _order < other._order);
}

} // decode
} // data
} // pv
