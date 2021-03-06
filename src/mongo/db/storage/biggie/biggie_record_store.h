// biggie_record_store.h

/**
*    Copyright (C) 2018 MongoDB Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*    As a special exception, the copyright holders give permission to link the
*    code of portions of this program with the OpenSSL library under certain
*    conditions as described in each individual source file and distribute
*    linked combinations including the program with the OpenSSL library. You
*    must comply with the GNU Affero General Public License in all respects for
*    all of the code used other than as permitted herein. If you modify file(s)
*    with this exception, you may extend this exception to your version of the
*    file(s), but you are not obligated to do so. If you do not wish to do so,
*    delete this exception statement from your version. If you delete this
*    exception statement from all source files in the program, then also delete
*    it in the license file.
*/

#pragma once

#include <atomic>
#include <boost/shared_array.hpp>
#include <map>

#include "mongo/db/concurrency/d_concurrency.h"
#include "mongo/db/storage/biggie/store.h"
#include "mongo/db/storage/capped_callback.h"
#include "mongo/db/storage/record_store.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/mutex.h"

namespace mongo {
namespace biggie {
/**
 * A RecordStore that stores all data in-memory.
 */
class RecordStore : public ::mongo::RecordStore {
    const bool _isCapped;
    const int64_t _cappedMaxSize;
    const int64_t _cappedMaxDocs;
    std::string _identStr;
    StringData _ident;
    std::string _prefix;
    std::string _postfix;
    CappedCallback* _cappedCallback;

public:
    explicit RecordStore(StringData ns,
                         StringData ident,
                         bool isCapped = false,
                         int64_t cappedMaxSize = -1,
                         int64_t cappedMaxDocs = -1,
                         CappedCallback* cappedCallback = nullptr);

    virtual const char* name() const;
    virtual const std::string& getIdent() const;
    virtual long long dataSize(OperationContext* opCtx) const;
    virtual long long numRecords(OperationContext* opCtx) const;
    virtual bool isCapped() const;
    virtual int64_t storageSize(OperationContext* opCtx,
                                BSONObjBuilder* extraInfo = NULL,
                                int infoLevel = 0) const;

    virtual RecordData dataFor(OperationContext* opCtx, const RecordId& loc) const;

    virtual bool findRecord(OperationContext* opCtx, const RecordId& loc, RecordData* rd) const;

    virtual void deleteRecord(OperationContext* opCtx, const RecordId& dl);

    virtual StatusWith<RecordId> insertRecord(OperationContext* opCtx,
                                              const char* data,
                                              int len,
                                              Timestamp);


    virtual Status insertRecordsWithDocWriter(OperationContext* opCtx,
                                              const DocWriter* const* docs,
                                              const Timestamp*,
                                              size_t nDocs,
                                              RecordId* idsOut);

    virtual Status updateRecord(OperationContext* opCtx,
                                const RecordId& oldLocation,
                                const char* data,
                                int len,
                                UpdateNotifier* notifier);

    virtual bool updateWithDamagesSupported() const;

    virtual StatusWith<RecordData> updateWithDamages(OperationContext* opCtx,
                                                     const RecordId& loc,
                                                     const RecordData& oldRec,
                                                     const char* damageSource,
                                                     const mutablebson::DamageVector& damages);

    std::unique_ptr<SeekableRecordCursor> getCursor(OperationContext* opCtx,
                                                    bool forward) const final;

    virtual Status truncate(OperationContext* opCtx);

    virtual void cappedTruncateAfter(OperationContext* opCtx, RecordId end, bool inclusive);

    virtual Status validate(OperationContext* opCtx,
                            ValidateCmdLevel level,
                            ValidateAdaptor* adaptor,
                            ValidateResults* results,
                            BSONObjBuilder* output);

    virtual void appendCustomStats(OperationContext* opCtx,
                                   BSONObjBuilder* result,
                                   double scale) const;

    virtual Status touch(OperationContext* opCtx, BSONObjBuilder* output) const;

    void waitForAllEarlierOplogWritesToBeVisible(OperationContext* opCtx) const;

    virtual void updateStatsAfterRepair(OperationContext* opCtx,
                                        long long numRecords,
                                        long long dataSize);

private:
    AtomicInt64 _highest_record_id{1};
    std::string generateKey(const uint8_t* key, size_t key_len) const;
    /*
     * This gets the next (guaranteed) unique record id.
     */
    inline int64_t nextRecordId() {
        return _highest_record_id.fetchAndAdd(1);
    }
    class Cursor final : public SeekableRecordCursor {
        OperationContext* opCtx;
        StringData _ident;
        std::string _prefix;
        std::string _postfix;
        StringStore::const_iterator it;
        boost::optional<std::string> _savedPosition;
        bool _needFirstSeek{true};

    public:
        Cursor(OperationContext* opCtx, const RecordStore& rs);
        boost::optional<Record> next() final;
        boost::optional<Record> seekExact(const RecordId& id) final override;
        void save() final;
        void saveUnpositioned() final override;
        bool restore() final;
        void detachFromOperationContext() final;
        void reattachToOperationContext(OperationContext* opCtx) final;

    private:
        bool inPrefix(const std::string& key_string);
    };
    class ReverseCursor final : public SeekableRecordCursor {
        OperationContext* opCtx;
        StringData _ident;
        std::string _prefix;
        std::string _postfix;
        StringStore::const_reverse_iterator it;
        boost::optional<std::string> _savedPosition;
        bool _needFirstSeek{true};

    public:
        ReverseCursor(OperationContext* opCtx, const RecordStore& rs);
        boost::optional<Record> next() final;
        boost::optional<Record> seekExact(const RecordId& id) final override;
        void save() final;
        void saveUnpositioned() final override;
        bool restore() final;
        void detachFromOperationContext() final;
        void reattachToOperationContext(OperationContext* opCtx) final;

    private:
        bool inPrefix(const std::string& key_string);
    };
};
}  // namespace biggie
}  // namespace mongo
