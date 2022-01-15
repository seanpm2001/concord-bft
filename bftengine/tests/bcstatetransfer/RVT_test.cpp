// Concord
//
// Copyright (c) 2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#include <iostream>
#include <string>
#include <random>
#include <vector>
#include <string>
#include <cmath>

#include <cryptopp/integer.h>
#include "gtest/gtest.h"

#include "RangeValidationTree.hpp"
#include "Logger.hpp"

#define HASH_SIZE 32

using namespace std;
using namespace CryptoPP;
using namespace concord::kvbc;

static constexpr uint32_t RVT_K = 3;

namespace bftEngine::bcst::impl {

using NodeInfo = RangeValidationTree::NodeInfo;
using RVTNode = RangeValidationTree::RVTNode;

static std::string randomString(size_t length) {
  static auto& chrs = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  std::mt19937 rg{std::random_device{}()};
  // String literals are null terminated and index starts from 0
  std::uniform_int_distribution<std::string::size_type> pick(1, sizeof(chrs) - 2);
  std::string s;
  s.reserve(length);
  while (length--) s += chrs[pick(rg)];

  return s;
}

static uint32_t randomNum(uint32_t min = 3, uint32_t max = UINT32_MAX / 1000, uint32_t in_multiple_of = 0) {
  std::mt19937 rg{std::random_device{}()};
  std::uniform_int_distribution<std::string::size_type> pick(min, max);
  auto num = pick(rg);
  if (in_multiple_of) {
    return (num - (num % in_multiple_of));
  }
  return num;
}

struct InputValues {
  InputValues(const string& l1, const string& l2, const string& p)
      : leaf1(l1.c_str()), leaf2(l2.c_str()), parent(p.c_str()) {}
  Integer leaf1;   // existing leaf val, shld size be 128 bytes?
  Integer leaf2;   // newly added leaf val
  Integer parent;  // mod by parent val (shld size be any different than leaf), shld be 256 byte?
};

class BcStTestDelegator {
 public:
  BcStTestDelegator(RangeValidationTree& rvt) : rvt_(rvt){};
  BcStTestDelegator() = delete;
  bool validateRVBGroupId(const RVBGroupId rvb_group_id) const { return rvt_.validateRVBGroupId(rvb_group_id); }

 private:
  RangeValidationTree& rvt_;
};

class RVTTest : public ::testing::Test {
 public:
  void init(RangeValidationTree& rvt) { delegator_ = std::make_unique<BcStTestDelegator>(rvt); }
  InputValues values_{randomString(HASH_SIZE), randomString(HASH_SIZE), randomString(HASH_SIZE)};
  std::shared_ptr<BcStTestDelegator> delegator_;
};

/////////////////////// starting of maths properties validation test for cryptoPP::Integer class  /////////////////////

TEST_F(RVTTest, basicAdditionSubtraction) {
  auto a = values_.leaf1;
  auto b = values_.leaf2;
  auto c = values_.leaf1 + values_.leaf2;
  ASSERT_EQ(a + b, c);
  ASSERT_EQ(c - b, a);
}

TEST_F(RVTTest, cumulativeAssociativeProperty) {
  auto a = values_.leaf1;
  auto b = values_.leaf2;
  auto c = values_.parent;
  ASSERT_EQ(a + b + c, c + a + b);
  ASSERT_NE(a - b, b - a);
  ASSERT_EQ(a + (b + c), (c + a) + b);
  ASSERT_NE(a + (b + c), (c + a) - b);
}

class RVTHashTestParamFixture : public RVTTest, public testing::WithParamInterface<InputValues> {};
TEST_P(RVTHashTestParamFixture, basicSumAndModOps) {
  auto hash = GetParam();
  auto a = values_.leaf1;
  auto b = values_.leaf2;
  auto c = values_.parent;

  Integer mod_res = (a + b) % c;
  Integer div_res = (a + b) / c;
  ASSERT_EQ(c * div_res + mod_res, a + b);
}

INSTANTIATE_TEST_CASE_P(
    basicSumAndModOps,
    RVTHashTestParamFixture,
    ::testing::Values(InputValues(randomString(HASH_SIZE), randomString(HASH_SIZE), randomString(HASH_SIZE)),
                      InputValues(randomString(HASH_SIZE), randomString(HASH_SIZE), randomString(HASH_SIZE)),
                      InputValues(randomString(HASH_SIZE), randomString(HASH_SIZE), randomString(HASH_SIZE))), );

class RVTTestParamFixture : public RVTTest, public testing::WithParamInterface<std::string> {};
TEST_P(RVTTestParamFixture, validateRawValue) {
  std::string str = GetParam();
  Integer input(reinterpret_cast<unsigned char*>(str.data()), str.size());

  ASSERT_EQ(input.MinEncodedSize(), str.size());
  // Encode to string doesn't work
  vector<char> enc_input(input.MinEncodedSize());
  input.Encode(reinterpret_cast<CryptoPP::byte*>(enc_input.data()), enc_input.size());
  ostringstream oss_en;
  for (auto& c : enc_input) {
    oss_en << c;
  }
  ASSERT_EQ(oss_en.str(), str);
}
INSTANTIATE_TEST_CASE_P(validateRawValue,
                        RVTTestParamFixture,
                        ::testing::Values(randomString(HASH_SIZE),
                                          randomString(HASH_SIZE),
                                          randomString(HASH_SIZE),
                                          randomString(HASH_SIZE)), );

/////////////////// ending  of maths properties validation test for cryptoPP::Integer class ///////////////////////

TEST_F(RVTTest, constructTreeWithSingleFirstNode) {
  static constexpr uint32_t fetch_range_size = 4;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  for (auto i = fetch_range_size; i <= fetch_range_size; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  ASSERT_EQ(rvt.totalNodes(), 1);
  ASSERT_EQ(rvt.empty(), false);
  ASSERT_EQ(rvt.totalLevels(), 1);
}

TEST_F(RVTTest, constructTreeWithSingleMiddleNode) {
  static constexpr uint32_t fetch_range_size = 4;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  for (auto i = fetch_range_size * 2; i <= fetch_range_size * 2; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  ASSERT_EQ(rvt.totalNodes(), 1);
  ASSERT_EQ(rvt.totalLevels(), 1);
}

TEST_F(RVTTest, constructTreeWithSingleLastNode) {
  static constexpr uint32_t fetch_range_size = 4;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  for (auto i = fetch_range_size * RVT_K; i <= fetch_range_size * RVT_K; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  ConcordAssertEQ(rvt.totalNodes(), 1);
  ConcordAssertEQ(rvt.totalLevels(), 1);
}

TEST_F(RVTTest, constructTreeWithTwoNodes) {
  static constexpr uint32_t fetch_range_size = 4;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  for (auto i = fetch_range_size; i <= fetch_range_size * RVT_K + fetch_range_size; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  rvt.printToLog(false);
  ConcordAssertEQ(rvt.totalLevels(), 2);
  ConcordAssertEQ(rvt.totalNodes(), 3);
}

TEST_F(RVTTest, TreeNodeRemovalBasic) {
  static constexpr uint32_t fetch_range_size = 4;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  for (auto i = fetch_range_size; i <= fetch_range_size * RVT_K + fetch_range_size; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  for (auto i = fetch_range_size; i <= fetch_range_size * RVT_K + fetch_range_size; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.removeNode(i, digest);
  }
  ASSERT_EQ(rvt.totalNodes(), 0);
  ASSERT_EQ(rvt.empty(), true);
}

class RVTTestserializeDeserializeFixture : public RVTTest,
                                           public testing::WithParamInterface<std::pair<uint32_t, uint32_t>> {};
TEST_P(RVTTestserializeDeserializeFixture, serializeDeserialize) {
  auto inputs = GetParam();
  uint32_t RVT_K = inputs.first;
  uint32_t fetch_range_size = inputs.second;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  size_t random_num_of_nodes_added = randomNum(1, 1000 * RVT_K);

  std::cout << KVLOG(random_num_of_nodes_added, RVT_K, fetch_range_size) << std::endl;
  for (auto i = fetch_range_size; i <= fetch_range_size * random_num_of_nodes_added; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  // TODO - move this into ctor on product in RVTNode
  auto root_hash = rvt.getRootHashVal();
  auto total_levels = rvt.totalLevels();
  auto total_nodes = rvt.totalNodes();

  std::ostringstream oss;
  oss = rvt.getSerializedRvbData();
  std::istringstream iss(oss.str());
  rvt.setSerializedRvbData(iss);

  ASSERT_EQ(root_hash, rvt.getRootHashVal());
  ASSERT_EQ(total_nodes, rvt.totalNodes());
  ASSERT_EQ(total_levels, rvt.totalLevels());
}
INSTANTIATE_TEST_CASE_P(serializeDeserialize,
                        RVTTestserializeDeserializeFixture,
                        ::testing::Values(std::make_pair(randomNum(3, 10), randomNum(4, 20)),
                                          std::make_pair(randomNum(3, 10), randomNum(4, 20)),
                                          std::make_pair(randomNum(3, 10), randomNum(4, 20))), );

class RVTTestRandomFRSAndRVT_KFixture : public RVTTest,
                                        public testing::WithParamInterface<std::pair<uint32_t, uint32_t>> {};
TEST_P(RVTTestRandomFRSAndRVT_KFixture, validateRandomFRSAndRVT_K) {
  auto inputs = GetParam();
  uint32_t RVT_K = inputs.first;
  uint32_t fetch_range_size = inputs.second;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  uint32_t n_nodes = fetch_range_size * randomNum(1024, 1024 * 1024);
  for (uint32_t i = fetch_range_size; i <= n_nodes; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  // TODO Find formula to validate total nodes
  auto min_rvb = 1;
  auto n_rvb_groups = (((n_nodes / fetch_range_size) - min_rvb) / (RVT_K)) + 1;
  auto max_level_by_formula = ceil(log(n_rvb_groups) / log(RVT_K)) + 1;
  auto max_level_from_tree = rvt.totalLevels();
  ASSERT_EQ(max_level_by_formula, max_level_from_tree);
}
INSTANTIATE_TEST_CASE_P(validateRandomFRSAndRVT_K,
                        RVTTestRandomFRSAndRVT_KFixture,
                        ::testing::Values(std::make_pair(randomNum(3, 10), randomNum(4, 20)),
                                          std::make_pair(randomNum(3, 10), randomNum(4, 20)),
                                          std::make_pair(randomNum(3, 10), randomNum(4, 20))), );

class RVTTestvalidateTreeFixture : public RVTTest, public testing::WithParamInterface<std::pair<uint32_t, uint32_t>> {};
TEST_P(RVTTestvalidateTreeFixture, validateTree) {
  auto inputs = GetParam();
  uint32_t RVT_K = inputs.first;
  uint32_t fetch_range_size = inputs.second;
  uint32_t n_nodes = fetch_range_size * randomNum(1024, 1024 * 1024);

  fetch_range_size = 4;
  n_nodes = 30;
  RVT_K = 3;
  std::cout << KVLOG(fetch_range_size, n_nodes, RVT_K) << std::endl;

  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  auto addNode = [&](uint64_t rvb_id) {
    // std::cout << "add:" << KVLOG(rvb_id) << std::endl;
    STDigest digest(std::to_string(rvb_id).c_str());
    rvt.addNode(rvb_id, digest);
  };

#if (0)
  auto removeNode = [&](uint64_t rvb_id) {
    // std::cout << "remove:" << KVLOG(rvb_id) << std::endl;
    STDigest digest(std::to_string(rvb_id).c_str());
    rvt.removeNode(rvb_id, digest);
  };
#endif

  for (uint32_t i = 1; i < 1000; ++i) {
    addNode(i * fetch_range_size);
  }

  // add, remove nodes randomly.
  for (uint32_t i = fetch_range_size; i <= n_nodes; i = i + fetch_range_size) {
    addNode(rvt.getMaxRvbId() + fetch_range_size);

    /*
    auto num = randomNum(1, 200);
    ((num % 2) || rvt.empty()) ? addNode(rvt.getMaxRvbId() + fetch_range_size)
                               : removeNode(rvt.getMinRvbId() + fetch_range_size);
                               */
    // TODO
    // ASSERT_EQ(rvt.validateTree(), true);
  }
}
INSTANTIATE_TEST_CASE_P(validateTree,
                        RVTTestvalidateTreeFixture,
                        ::testing::Values(std::make_pair(randomNum(3, 10), randomNum(4, 20)),
                                          std::make_pair(randomNum(3, 10), randomNum(4, 20)),
                                          std::make_pair(randomNum(3, 10), randomNum(4, 20))), );

// TODO Need to be improved to have random RVT_K and validation logic
TEST_F(RVTTest, validateRvbGroupIds) {
  uint32_t RVT_K = 4;
  uint32_t fetch_range_size = 5;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  init(rvt);

  for (auto i = fetch_range_size; i <= fetch_range_size * RVT_K * 2 + fetch_range_size; i = i + fetch_range_size) {
    STDigest digest(std::to_string(i).c_str());
    rvt.addNode(i, digest);
  }
  std::vector<RVBGroupId> rvb_group_ids;
  rvb_group_ids = rvt.getRvbGroupIds(5, 5);
  ASSERT_EQ(rvb_group_ids.size(), 1);
  auto hash_val_1 = rvt.getDirectParentHashVal(randomNum(5, 10, 5));
  auto hash_val_2 = rvt.getDirectParentHashVal(randomNum(15, 20, 5));
  ASSERT_EQ(hash_val_1, hash_val_2);

  rvb_group_ids = rvt.getRvbGroupIds(5, 45);
  ASSERT_EQ(rvb_group_ids.size(), 3);
  ASSERT_NE(rvt.getDirectParentHashVal(randomNum(5, 20, 5)), rvt.getDirectParentHashVal(randomNum(25, 40, 5)));
  ASSERT_NE(rvt.getDirectParentHashVal(randomNum(5, 20, 5)), rvt.getDirectParentHashVal(45));
  ASSERT_NE(rvt.getDirectParentHashVal(randomNum(25, 40, 5)), rvt.getDirectParentHashVal(45));

  std::vector<BlockId> rvb_block_ids = rvt.getRvbIds(rvb_group_ids[0]);
  if (not delegator_->validateRVBGroupId(rvb_group_ids[0])) {
    ASSERT_EQ(rvb_block_ids.size(), 0);
  } else {
    ASSERT_EQ(rvb_block_ids.size(), 4);
  }
}

// TODO - convert to parametrized
TEST_F(RVTTest, simpleAddRemoveWithRootValidation) {
  uint64_t fetch_range_size = 4;
  uint64_t RVT_K = 3;
  RangeValidationTree rvt(logging::getLogger("concord.bft.st.rvt"), RVT_K, fetch_range_size);
  size_t add_i{1}, rem_i{1};

  for (; add_i < 1000; ++add_i) {
    // std::cout << "add:" << KVLOG(add_i * fetch_range_size) << std::endl;
    rvt.addNode(add_i * fetch_range_size, {std::to_string(add_i * fetch_range_size).c_str()});
  }
  for (; rem_i < 1000; ++rem_i) {
    // std::cout << "remove:" << KVLOG(rem_i * fetch_range_size) << std::endl;
    rvt.removeNode(rem_i * fetch_range_size, {std::to_string(rem_i * fetch_range_size).c_str()});
  }

  for (; add_i < 500; ++add_i) {
    // std::cout << "add:" << KVLOG(add_i * fetch_range_size) << std::endl;
    rvt.addNode(add_i * fetch_range_size, {std::to_string(add_i * fetch_range_size).c_str()});
  }
  for (; rem_i < 300; ++rem_i) {
    // std::cout << "remove:" << KVLOG(rem_i * fetch_range_size) << std::endl;
    rvt.removeNode(rem_i * fetch_range_size, {std::to_string(rem_i * fetch_range_size).c_str()});
  }

  for (; add_i < 500; ++add_i) {
    // std::cout << "add:" << KVLOG(add_i * fetch_range_size) << std::endl;
    rvt.addNode(add_i * fetch_range_size, {std::to_string(add_i * fetch_range_size).c_str()});
  }
  for (; rem_i < 700; ++rem_i) {
    // std::cout << "remove:" << KVLOG(rem_i * fetch_range_size) << std::endl;
    rvt.removeNode(rem_i * fetch_range_size, {std::to_string(rem_i * fetch_range_size).c_str()});
  }
  ASSERT_TRUE(rvt.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// TODO
// validate concurrent addition & removal of nodes from RVT
// validate hash val against addition & removal of RVB nodes

}  // namespace bftEngine::bcst::impl