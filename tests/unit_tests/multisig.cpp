// Copyright (c) 2017-2019, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gtest/gtest.h"

#include <cstdint>

#include "wallet/wallet2.h"

static const struct
{
  const char *address;
  const char *spendkey;
} test_addresses[] =
{
  {
    "TNzeQGqvv4UWksh1YwFYRfjGvp3tB2dYDLmC12b58nnkAYWxVwaV9mVhGDDHEemhLmLhSS9dtYbRBNNDBSumGMFFAPSsJR4j58",
    "69b02ad31d5c9feeb97f27357c39617fe3d0a2c9303eec65ba8956e72387dc04"
  },
  {
    "TNzeBJPeSFyNKieaTpSS5HGRfRi64HqxEiJinV1TAAvyH7SZHnUWJ445sjHo38ffYW3fd6Aknic2yDMZzaxA8J451SiEZk2uW4",
    "aebdad16f64eb9b0c1c30daeb7296ebfb3cc7d17f9a97752cc4d4df997b9a501"
  },
  {
    "TNzePpjXTnoJGLShApynb89JqSoQS4icjKZAHnhpz8AGaCbey2VgydUDvi4pABDHWPhZdp8JUppqtgUTuDMBj57A8icbDjrekp",
    "639e09ccda9dec0daf8c52e3f0194e7b7f59f3c5baf5d9e2e81e176850478905"
  },
  {
    "TNzebrkq8AL7nTzB7iDfrz87KcvJa3fdr5doQyLmE5THQ5WTqNmCXopMk64btTzz81LbwzzyXWM2wCwftTaC1qDq2pviDfFiWN",
    "95826bd0b9b21ea37ae548d7b1e39a62b4cb64436c18ef568366c30481eae906"
  },
  {
    "TNzeL8tpob1BMaYUCambpN9aVDnnqjJGeQNT7vaBw67oGzKbWGrqjisNDFH3BNM4cbj3s12CHCcUc9Ypm2gwhcTV6V4xiGvHbr",
    "aed576e7a275c32ca49f286b958d6bbd6f192869dd980dc7914358f343a00104"
  }
};

static const size_t KEYS_COUNT = 5;

static void make_wallet(unsigned int idx, tools::wallet2 &wallet)
{
  ASSERT_TRUE(idx < sizeof(test_addresses) / sizeof(test_addresses[0]));

  crypto::secret_key spendkey;
  epee::string_tools::hex_to_pod(test_addresses[idx].spendkey, spendkey);

  try
  {
    wallet.init("", boost::none, boost::asio::ip::tcp::endpoint{}, 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
    wallet.set_subaddress_lookahead(1, 1);
    wallet.generate("", "", spendkey, true, false);
    ASSERT_TRUE(test_addresses[idx].address == wallet.get_account().get_public_address_str(cryptonote::TESTNET));
    wallet.decrypt_keys("");
    ASSERT_TRUE(test_addresses[idx].spendkey == epee::string_tools::pod_to_hex(wallet.get_account().get_keys().m_spend_secret_key));
    wallet.encrypt_keys("");
  }
  catch (const std::exception &e)
  {
    MFATAL("Error creating test wallet: " << e.what());
    ASSERT_TRUE(0);
  }
}

static std::vector<std::string> exchange_round(std::vector<tools::wallet2>& wallets, const std::vector<std::string>& mis)
{
  std::vector<std::string> new_infos;
  for (size_t i = 0; i < wallets.size(); ++i) {
      new_infos.push_back(wallets[i].exchange_multisig_keys("", mis));
  }

  return new_infos;
}

static void make_wallets(std::vector<tools::wallet2>& wallets, unsigned int M)
{
  ASSERT_TRUE(wallets.size() > 1 && wallets.size() <= KEYS_COUNT);
  ASSERT_TRUE(M <= wallets.size());

  std::vector<std::string> mis(wallets.size());

  for (size_t i = 0; i < wallets.size(); ++i) {
    make_wallet(i, wallets[i]);

    wallets[i].decrypt_keys("");
    mis[i] = wallets[i].get_multisig_info();
    wallets[i].encrypt_keys("");
  }

  for (auto& wallet: wallets) {
    ASSERT_FALSE(wallet.multisig());
  }

  std::vector<std::string> mxis;
  for (size_t i = 0; i < wallets.size(); ++i) {
    // it's ok to put all of multisig keys in this function. it throws in case of error
    mxis.push_back(wallets[i].make_multisig("", mis, M));
  }

  while (!mxis[0].empty()) {
    mxis = exchange_round(wallets, mxis);
  }

  for (size_t i = 0; i < wallets.size(); ++i) {
    ASSERT_TRUE(mxis[i].empty());
    bool ready;
    uint32_t threshold, total;
    ASSERT_TRUE(wallets[i].multisig(&ready, &threshold, &total));
    ASSERT_TRUE(ready);
    ASSERT_TRUE(threshold == M);
    ASSERT_TRUE(total == wallets.size());

    if (i != 0) {
      // "equals" is transitive relation so we need only to compare first wallet's address to each others' addresses. no need to compare 0's address with itself.
      ASSERT_TRUE(wallets[0].get_account().get_public_address_str(cryptonote::TESTNET) == wallets[i].get_account().get_public_address_str(cryptonote::TESTNET));
    }
  }
}

TEST(multisig, make_2_2)
{
  std::vector<tools::wallet2> wallets(2);
  make_wallets(wallets, 2);
}

TEST(multisig, make_3_3)
{
  std::vector<tools::wallet2> wallets(3);
  make_wallets(wallets, 3);
}

TEST(multisig, make_2_3)
{
  std::vector<tools::wallet2> wallets(3);
  make_wallets(wallets, 2);
}

TEST(multisig, make_2_4)
{
  std::vector<tools::wallet2> wallets(4);
  make_wallets(wallets, 2);
}

TEST(multisig, make_2_5)
{
  std::vector<tools::wallet2> wallets(5);
  make_wallets(wallets, 2);
}
