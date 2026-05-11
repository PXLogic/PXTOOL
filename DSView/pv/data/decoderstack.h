/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef DSVIEW_PV_DATA_DECODERSTACK_H
#define DSVIEW_PV_DATA_DECODERSTACK_H

#include <libsigrokdecode.h>
#include <list>
#include <thread>
#include <boost/optional.hpp>
#include <QObject>
#include <QString>
#include <mutex>
#include <atomic>

#include "decode/row.h" 
#include "../data/signaldata.h"
#include "decode/decoderstatus.h"
#include "c_decoder_registry.h"
 

namespace DecoderStackTest {
class TwoDecoderStack;
}
 
namespace pv {

class SigSession;

namespace view {
class LogicSignal;
}

namespace data {

class LogicSnapshot;

namespace decode {
class Annotation;
class Decoder;
class RowData;
}

class DecoderStack;

struct decode_task_status
{  
    volatile bool _bStop;
    DecoderStack *_decoder;
};

 //a torotocol have a DecoderStack, destroy by DecodeTrace
class DecoderStack : public QObject, public SignalData
{
	Q_OBJECT

private:
	static const double DecodeMargin;
	static const double DecodeThreshold;
	static const int64_t DecodeChunkLength;
	static const unsigned int DecodeNotifyPeriod;
    static const uint64_t MaxChunkSize = 1024 * 16;

public:
    enum decode_state {
        Stopped,
        Running
    };

public:
   	DecoderStack(pv::SigSession *_session,
		const srd_decoder *const decoder, DecoderStatus *decoder_status);

public:

	virtual ~DecoderStack();

    inline std::list<decode::Decoder*>& stack(){
        return _stack;
    }

    const char* get_root_decoder_id();

	void add_sub_decoder(decode::Decoder *decoder);
    void remove_sub_decoder(decode::Decoder *decoder);
    void remove_decoder_by_handel(const srd_decoder *dec);
    
    void build_row();

	int64_t samples_decoded();

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		std::vector<pv::data::decode::Annotation*> &dest,
		const decode::Row &row, uint64_t start_sample,
		uint64_t end_sample);

    uint64_t get_annotation_index(
        const decode::Row &row, uint64_t start_sample);
    uint64_t get_max_annotation(const decode::Row &row);
    uint64_t get_min_annotation(const decode::Row &row); // except instant(end=start) annotation

    std::map<const decode::Row, bool> get_rows_gshow();
    std::map<const decode::Row, bool> get_rows_lshow();
    void set_rows_gshow(const decode::Row row, bool show);
    void set_rows_lshow(const decode::Row row, bool show);
    bool has_annotations(const decode::Row &row);
    uint64_t list_annotation_size();
    uint64_t list_annotation_size(uint16_t row_index);


    bool list_annotation(decode::Annotation *ann,
                        uint16_t row_index, uint64_t col_index);


    bool list_row_title(int row, QString &title);
	 
	void clear();
    void init();
	uint64_t get_max_sample_count();

    inline bool IsRunning(){
        return _decode_state == Running;
    }
 
	void begin_decode_work();
    
    void stop_decode_work();  
    int list_rows_size();
    bool options_changed();
    void set_options_changed(bool changed);

    uint64_t sample_count();
    uint64_t sample_rate();
    bool out_of_memory();
    void set_mark_index(int64_t index);
    int64_t get_mark_index();
    void frame_ended();

    inline QString error_message(){ 
	    return _error_message;
    }

    inline void *get_key_handel(){
        return _decoder_status;
    }

    inline bool is_capture_end(){
        return _is_capture_end;
    }

    inline void set_capture_end_flag(bool isEnd){
        _is_capture_end = isEnd;
        if (!isEnd){
            _progress = 0;
            _is_decoding = false;
        }
    }

    inline int get_progress(){
        //if (!_is_decoding && _progress == 0)
          //  return -1;
        return _progress;
    }

    bool check_required_probes();

    inline uint64_t get_result_count(){
        return _result_count;
    }

    inline void set_task_active(bool v) {
        _task_active.store(v);
    }

    // Atomically clears _task_active and returns its previous value.
    // The caller that gets true "owns" the matching _running_decoder_count decrement.
    inline bool clear_task_active() {
        return _task_active.exchange(false);
    }

    inline void set_use_c_decoder(bool use_c) { _use_c_decoder = use_c; }
    inline bool use_c_decoder() const { return _use_c_decoder; }

public:
    // Called from c_put_annotation_cb (free function in pv::data namespace)
    static void annotation_callback(srd_proto_data *pdata, void *self);

private:
    void decode_data(const uint64_t decode_start, const uint64_t decode_end, srd_session *const session);
	void execute_decode_stack();
    void execute_c_decode_stack();
    void do_decode_work();
    void join_own_thread();
  
signals:
	void new_decode_data();
    void decode_done();
  
private: 
	std::list<decode::Decoder*> _stack;
	pv::data::LogicSnapshot *_snapshot;
  
    std::map<const decode::Row, decode::RowData*>   _rows;
    std::map<const decode::Row, bool>       _rows_gshow;
    std::map<const decode::Row, bool>       _rows_lshow;
    std::map<std::pair<const srd_decoder*, int>, decode::Row> _class_rows;
  
    SigSession      *_session;
    std::atomic<decode_state> _decode_state;
    // Tracks whether this decoder's run was counted in SigSession::_running_decoder_count.
    // Set to true in add_decode_task, cleared atomically by either the completion
    // lambda or remove_decode_task — whichever runs first.
    std::atomic<bool> _task_active{false};
    volatile bool   _options_changed;
    volatile bool   _no_memory;
    int64_t         _mark_index;

    DecoderStatus   *_decoder_status;
    QString         _error_message;
    int64_t	        _samples_decoded;
    uint64_t        _sample_count; 
 
    decode_task_status  *_stask_stauts;    
    mutable std::mutex _output_mutex;
    std::thread _own_thread;
    bool            _is_capture_end;
    int             _progress;
    bool            _is_decoding;
    uint64_t        _result_count;

    std::atomic<bool> _use_c_decoder{false};

	friend class DecoderStackTest::TwoDecoderStack;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DECODERSTACK_H
