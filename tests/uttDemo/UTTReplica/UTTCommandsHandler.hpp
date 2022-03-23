// Concord
//
// Copyright (c) 2018-2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include "Logger.hpp"
#include "block_metadata.hpp"
#include "KVBCInterfaces.h"
#include "SharedTypes.hpp"
#include "categorization/kv_blockchain.h"

#include "utt_messages.cmf.hpp"
#include "app_state.hpp"

static const std::string VERSIONED_KV_CAT_ID{concord::kvbc::categorization::kExecutionPrivateCategory};

class UTTCommandsHandler : public concord::kvbc::ICommandsHandler {
 public:
  UTTCommandsHandler(concord::kvbc::IReader *storage,
                     concord::kvbc::IBlockAdder *blocksAdder,
                     concord::kvbc::IBlockMetadata *blockMetadata,
                     logging::Logger &logger,
                     concord::kvbc::categorization::KeyValueBlockchain *kvbc)
      : storage_(storage), blockAdder_(blocksAdder), blockMetadata_(blockMetadata), logger_(logger), kvbc_{kvbc} {
    ConcordAssertNE(kvbc_, nullptr);
    (void)blockMetadata_;
  }

  void execute(ExecutionRequestsQueue &requests,
               std::optional<bftEngine::Timestamp> timestamp,
               const std::string &batchCid,
               concordUtils::SpanWrapper &parent_span) override;

  void preExecute(IRequestsHandler::ExecutionRequest &req,
                  std::optional<bftEngine::Timestamp> timestamp,
                  const std::string &batchCid,
                  concordUtils::SpanWrapper &parent_span) override{};

  void setPerformanceManager(std::shared_ptr<concord::performance::PerformanceManager> perfManager) override {}

 private:
  utt::messages::TxReply handleRequest(const utt::messages::TxRequest &req);
  utt::messages::GetLastBlockReply handleRequest(const utt::messages::GetLastBlockRequest &req);
  utt::messages::GetBlockDataReply handleRequest(const utt::messages::GetBlockDataRequest &req);

  std::string getLatest(const std::string &key) const;

  void syncAppState();

  AppState state_;

  concord::kvbc::IReader *storage_;
  concord::kvbc::IBlockAdder *blockAdder_;
  concord::kvbc::IBlockMetadata *blockMetadata_;
  logging::Logger &logger_;
  std::shared_ptr<concord::performance::PerformanceManager> perfManager_;
  concord::kvbc::categorization::KeyValueBlockchain *kvbc_{nullptr};
};