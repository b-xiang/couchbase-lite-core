//
//  DBActor.hh
//  LiteCore
//
//  Created by Jens Alfke on 2/21/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "ReplActor.hh"
#include "c4Base.h"
#include "slice.hh"
#include <string>
#include <vector>

namespace litecore { namespace repl {
    using slice = fleece::slice;
    using alloc_slice = fleece::alloc_slice;
    class Pusher;

    
    /** Metadata of a document revision. */
    struct Rev {
        alloc_slice docID;
        alloc_slice revID;
        C4SequenceNumber sequence {0};
        bool deleted {false};

        Rev() { }

        Rev(const C4DocumentInfo &info)
        :sequence(info.sequence), docID(info.docID), revID(info.revID)
        ,deleted((info.flags &kDeleted) != 0)
        { }
    };
    typedef std::vector<Rev> RevList;


    struct RevRequest : public Rev {
        std::vector<alloc_slice> ancestorRevIDs;
        unsigned maxHistory;

        RevRequest(const Rev &rev, unsigned maxHistory_)
        :Rev(rev)
        ,maxHistory(maxHistory_)
        { }
    };


    struct RequestedRev : public Rev {
        alloc_slice remoteSequence;

        RequestedRev() { }
    };
    
    
    /** Actor that manages database access for the replicator. */
    class DBActor : public ReplActor {
    public:
        DBActor(blip::Connection *connection,
                C4Database *db,
                const websocket::Address &remoteAddress,
                Options options);

        using CheckpointCallback = std::function<void(alloc_slice checkpointID,
                                                      alloc_slice data,
                                                      C4Error err)>;

        void getCheckpoint(CheckpointCallback cb) {
            enqueue(&DBActor::_getCheckpoint, cb);
        }

        void setCheckpoint(const alloc_slice &data) {
            enqueue(&DBActor::_setCheckpoint, data);
        }

        void getChanges(C4SequenceNumber since, unsigned limit, bool continuous, Pusher*);

        void findOrRequestRevs(Retained<blip::MessageIn> req,
                               std::function<void(std::vector<alloc_slice>)> callback) {
            enqueue(&DBActor::_findOrRequestRevs, req, callback);
        }

        void sendRevision(const RevRequest &request,
                          std::function<void(Retained<blip::MessageIn>)> onReply) {
            enqueue(&DBActor::_sendRevision, request, onReply);
        }

        void insertRevision(Rev rev, slice history, alloc_slice body,
                            std::function<void(C4Error)> callback) {
            enqueue(&DBActor::_insertRevision, rev, alloc_slice(history), body, callback);
        }
        
    private:
        void handleGetCheckpoint(Retained<blip::MessageIn>);
        void handleSetCheckpoint(Retained<blip::MessageIn>);
        bool getPeerCheckpointDoc(blip::MessageIn* request, bool getting,
                                  fleece::slice &checkpointID, c4::ref<C4RawDocument> &doc);

        slice effectiveRemoteCheckpointDocID();
        void _getCheckpoint(CheckpointCallback);
        void _setCheckpoint(alloc_slice data);
        void _getChanges(C4SequenceNumber since, unsigned limit, bool continuous,
                         Retained<Pusher>);
        void _findOrRequestRevs(Retained<blip::MessageIn> req,
                                std::function<void(std::vector<alloc_slice>)> callback);
        void _sendRevision(RevRequest request,
                           std::function<void(Retained<blip::MessageIn>)> onReply);
        void _insertRevision(Rev, alloc_slice history, alloc_slice body,
                             std::function<void(C4Error)> callback);

            void dbChanged();

        bool findAncestors(slice docID, slice revID,
                           std::vector<alloc_slice> &ancestors);

        static const size_t kMaxPossibleAncestors = 10;

        C4Database* const _db;
        const websocket::Address _remoteAddress;
        std::string _remoteCheckpointDocID;
        c4::ref<C4DatabaseObserver> _changeObserver;
    };

} }
