/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */

/**:
 * @brief : sync msg engine test
 * @author: catli
 * @date: 2018-10-25
 */

#include <libsync/DownloadingTxsQueue.h>
#include <libsync/SyncMsgEngine.h>
#include <libsync/SyncMsgPacket.h>
#include <test/tools/libutils/TestOutputHelper.h>
#include <test/unittests/libsync/FakeSyncToolsSet.h>
#include <boost/test/unit_test.hpp>
#include <memory>

using namespace std;
using namespace dev;
using namespace dev::test;
using namespace dev::p2p;
using namespace dev::sync;

namespace dev
{
namespace test
{
class SyncMsgEngineFixture : TestOutputHelperFixture
{
public:
    SyncMsgEngineFixture()
      : fakeSyncToolsSet(),
        fakeStatusPtr(make_shared<SyncMasterStatus>(h256(0x1024))),
        fakeTxQueuePtr(make_shared<DownloadingTxsQueue>(66, NodeID(0xabcd))),
        fakeMsgEngine(fakeSyncToolsSet.getServicePtr(), fakeSyncToolsSet.getTxPoolPtr(),
            fakeSyncToolsSet.getBlockChainPtr(), fakeStatusPtr, fakeTxQueuePtr, 0, NodeID(0xabcd),
            h256(0xcdef)),
        fakeException()
    {
        SyncPeerInfo newPeer{NodeID(), 0, h256(0x1024), h256(0x1024)};
        fakeStatusPtr->newSyncPeerStatus(newPeer);
    }
    FakeSyncToolsSet fakeSyncToolsSet;
    shared_ptr<SyncMasterStatus> fakeStatusPtr;
    shared_ptr<DownloadingTxsQueue> fakeTxQueuePtr;
    SyncMsgEngine fakeMsgEngine;
    NetworkException fakeException;
};

class FakeServiceForDownloadBlocksContainer : public P2PInterface
{
public:
    using Ptr = std::shared_ptr<FakeServiceForDownloadBlocksContainer>;
    FakeServiceForDownloadBlocksContainer() : P2PInterface(), sentCnt(0) {}

    ~FakeServiceForDownloadBlocksContainer(){};

    void asyncSendMessageByNodeID(NodeID, std::shared_ptr<P2PMessage>, CallbackFuncWithSession,
        dev::network::Options) override
    {
        sentCnt++;
    };

    size_t sentCnt = 0;

public:
    // not used
    NodeID id() const override { return NodeID(); };

    std::shared_ptr<P2PMessage> sendMessageByNodeID(NodeID, std::shared_ptr<P2PMessage>) override
    {
        return nullptr;
    };

    std::shared_ptr<P2PMessage> sendMessageByTopic(
        std::string, std::shared_ptr<P2PMessage>) override
    {
        return nullptr;
    };
    void asyncSendMessageByTopic(std::string, std::shared_ptr<P2PMessage>, CallbackFuncWithSession,
        dev::network::Options) override{};

    void asyncMulticastMessageByTopic(std::string, std::shared_ptr<P2PMessage>) override{};

    void asyncMulticastMessageByNodeIDList(NodeIDs, std::shared_ptr<P2PMessage>) override{};

    void asyncBroadcastMessage(std::shared_ptr<P2PMessage>, dev::network::Options) override{};

    void registerHandlerByProtoclID(PROTOCOL_ID, CallbackFuncWithSession) override{};

    void registerHandlerByTopic(std::string, CallbackFuncWithSession) override{};

    P2PSessionInfos sessionInfos() override { return P2PSessionInfos(); };
    P2PSessionInfos sessionInfosByProtocolID(PROTOCOL_ID) const override
    {
        return P2PSessionInfos();
    };

    bool isConnected(NodeID const&) const override { return true; };

    std::vector<std::string> topics() override { return std::vector<std::string>(); };

    dev::h512s getNodeListByGroupID(GROUP_ID) override { return dev::h512s(); };
    void setGroupID2NodeList(std::map<GROUP_ID, dev::h512s>) override{};
    void setNodeListByGroupID(GROUP_ID, dev::h512s) override{};

    void setTopics(std::shared_ptr<std::vector<std::string>>) override{};

    std::shared_ptr<P2PMessageFactory> p2pMessageFactory() override { return nullptr; };
};

BOOST_FIXTURE_TEST_SUITE(SyncMsgEngineTest, SyncMsgEngineFixture)

BOOST_AUTO_TEST_CASE(SyncStatusPacketTest)
{
    auto statusPacket = SyncStatusPacket();
    statusPacket.encode(0x1, h256(0xcdef), h256(0xcd));
    auto msgPtr = statusPacket.toMessage(0x01);
    auto fakeSessionPtr = fakeSyncToolsSet.createSession();
    fakeMsgEngine.messageHandler(fakeException, fakeSessionPtr, msgPtr);

    BOOST_CHECK(fakeStatusPtr->hasPeer(NodeID()));
    fakeMsgEngine.messageHandler(fakeException, fakeSessionPtr, msgPtr);
    BOOST_CHECK_EQUAL(fakeStatusPtr->peerStatus(NodeID())->number, 0x1);
    BOOST_CHECK_EQUAL(fakeStatusPtr->peerStatus(NodeID())->genesisHash, h256(0xcdef));
    BOOST_CHECK_EQUAL(fakeStatusPtr->peerStatus(NodeID())->latestHash, h256(0xcd));
}

BOOST_AUTO_TEST_CASE(SyncTransactionPacketTest)
{
    auto txPacket = SyncTransactionsPacket();
    auto txPtr = fakeSyncToolsSet.createTransaction(0);
    vector<bytes> txRLPs;
    txRLPs.emplace_back(txPtr->rlp());
    txPacket.encode(txRLPs);
    auto msgPtr = txPacket.toMessage(0x02);
    auto fakeSessionPtr = fakeSyncToolsSet.createSession();
    fakeMsgEngine.messageHandler(fakeException, fakeSessionPtr, msgPtr);
    auto txPoolPtr = fakeSyncToolsSet.getTxPoolPtr();
    fakeTxQueuePtr->pop2TxPool(txPoolPtr);
    auto topTxs = txPoolPtr->topTransactions(1);
    BOOST_CHECK(topTxs.size() == 1);
    BOOST_CHECK_EQUAL(topTxs[0].sha3(), txPtr->sha3());
}

BOOST_AUTO_TEST_CASE(SyncBlocksPacketTest)
{
    SyncBlocksPacket blocksPacket;
    vector<bytes> blockRLPs;
    auto& fakeBlock = fakeSyncToolsSet.getBlock();
    fakeBlock.header().setNumber(INT64_MAX - 1);
    blockRLPs.push_back(fakeBlock.rlp());
    blocksPacket.encode(blockRLPs);
    auto msgPtr = blocksPacket.toMessage(0x03);
    auto fakeSessionPtr = fakeSyncToolsSet.createSession();
    fakeMsgEngine.messageHandler(fakeException, fakeSessionPtr, msgPtr);

    BOOST_CHECK(fakeStatusPtr->bq().size() == 1);
    auto block = fakeStatusPtr->bq().top(true);
    BOOST_CHECK(block->equalAll(fakeBlock));
}

BOOST_AUTO_TEST_CASE(SyncReqBlockPacketTest)
{
    SyncReqBlockPacket reqBlockPacket;
    reqBlockPacket.encode(int64_t(0x01), 0x02);
    auto msgPtr = reqBlockPacket.toMessage(0x03);
    auto fakeSessionPtr = fakeSyncToolsSet.createSession();

    fakeStatusPtr->newSyncPeerStatus({h512(0), 0, h256(), h256()});
    fakeMsgEngine.messageHandler(fakeException, fakeSessionPtr, msgPtr);

    BOOST_CHECK(fakeStatusPtr->hasPeer(h512(0)));
    BOOST_CHECK(!fakeStatusPtr->peerStatus(h512(0))->reqQueue.empty());
}  // namespace test

BOOST_AUTO_TEST_CASE(BatchSendTest)
{
    size_t maxPayloadSize =
        dev::p2p::P2PMessage::MAX_LENGTH - 2048;  // should be the same as syncMsgEngine.cpp
    size_t quarterPayloadSize = maxPayloadSize / 4;

    FakeServiceForDownloadBlocksContainer::Ptr service =
        make_shared<FakeServiceForDownloadBlocksContainer>();
    shared_ptr<DownloadBlocksContainer> batchSender =
        make_shared<DownloadBlocksContainer>(service, PROTOCOL_ID(0), NodeID(0));

    shared_ptr<bytes> maxRlp = make_shared<bytes>(maxPayloadSize + 1);
    shared_ptr<bytes> quarterRlp = make_shared<bytes>(quarterPayloadSize);
    shared_ptr<bytes> zeroRlp = make_shared<bytes>(0);

    BOOST_CHECK_EQUAL(service->sentCnt, 0);

    // size <= 1/4
    batchSender->batchAndSend(zeroRlp);
    BOOST_CHECK_EQUAL(service->sentCnt, 0);

    // size <= 1/4
    batchSender->batchAndSend(quarterRlp);
    BOOST_CHECK_EQUAL(service->sentCnt, 0);

    // size <= 2/4
    batchSender->batchAndSend(quarterRlp);
    BOOST_CHECK_EQUAL(service->sentCnt, 0);

    // size <= 3/4
    batchSender->batchAndSend(quarterRlp);
    BOOST_CHECK_EQUAL(service->sentCnt, 0);

    // size <= 4/4
    batchSender->batchAndSend(quarterRlp);
    BOOST_CHECK_EQUAL(service->sentCnt, 0);

    // size <= 5/4 -> 1/4    cnt -> 1
    batchSender->batchAndSend(quarterRlp);
    BOOST_CHECK_EQUAL(service->sentCnt, 1);

    // size <= 1/4  big  cnt -> 2
    batchSender->batchAndSend(maxRlp);
    BOOST_CHECK_EQUAL(service->sentCnt, 2);

    // de construct cnt -> 3
    batchSender = nullptr;
    BOOST_CHECK_EQUAL(service->sentCnt, 3);
}


BOOST_AUTO_TEST_SUITE_END()
}  // namespace test
}  // namespace dev
