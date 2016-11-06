#include "KosOtExtSender.h"

#include "OT/Tools/Tools.h"
#include "Common/Log.h"
#include "Common/ByteStream.h"
#include "Crypto/Commit.h"

namespace osuCrypto
{
    //#define KOS_DEBUG

    using namespace std;




    std::unique_ptr<OtExtSender> KosOtExtSender::split()
    {

        std::unique_ptr<OtExtSender> ret(new KosOtExtSender());

        std::array<block, gOtExtBaseOtCount> baseRecvOts;

        for (u64 i = 0; i < mGens.size(); ++i)
        {
            baseRecvOts[i] = mGens[i].get<block>();
        }

        ret->setBaseOts(baseRecvOts, mBaseChoiceBits);

        return std::move(ret);
    }

    void KosOtExtSender::setBaseOts(ArrayView<block> baseRecvOts, const BitVector & choices)
    {
        if (baseRecvOts.size() != gOtExtBaseOtCount || choices.size() != gOtExtBaseOtCount)
            throw std::runtime_error("not supported/implemented");


        mBaseChoiceBits = choices;
        for (int i = 0; i < gOtExtBaseOtCount; i++)
        {
            mGens[i].SetSeed(baseRecvOts[i]);
        }
    }

    void KosOtExtSender::send(
        ArrayView<std::array<block, 2>> messages,
        PRNG& prng,
        Channel& chl)
    {

        const u8 superBlkSize(8);


        // round up
        u64 numOtExt = roundUpTo(messages.size(), 128);
        u64 numSuperBlocks = (numOtExt / 128 + superBlkSize) / superBlkSize;
        u64 numBlocks = numSuperBlocks * superBlkSize;

        // a temp that will be used to transpose the sender's matrix
        std::array<std::array<block, superBlkSize>, 128> t, u, t0;
        std::array<block, 128> choiceMask;
        block delta = *(block*)mBaseChoiceBits.data();

        for (u64 i = 0; i < 128; ++i)
        {
            if (mBaseChoiceBits[i]) choiceMask[i] = AllOneBlock;
            else choiceMask[i] = ZeroBlock;
        }

        std::array<block, 128> extraBlocks;
        block* xIter = extraBlocks.data();


        Commit theirSeedComm;
        chl.recv(theirSeedComm.data(), theirSeedComm.size());

        std::array<block, 2>* mIter = messages.data();


        for (u64 superBlkIdx = 0; superBlkIdx < numSuperBlocks; ++superBlkIdx)
        {

            block * tIter = (block*)t.data();
            block * uIter = (block*)u.data();
            block * cIter = choiceMask.data();

            chl.recv(u.data(), superBlkSize * 128 * sizeof(block));

            // transpose 128 columns at at time. Each column will be 128 * superBlkSize = 1024 bits long.
            for (u64 colIdx = 0; colIdx < 128; ++colIdx)
            {
                // generate the columns using AES-NI in counter mode.
                mGens[colIdx].mAes.ecbEncCounterMode(mGens[colIdx].mBlockIdx, superBlkSize, tIter);
                mGens[colIdx].mBlockIdx += superBlkSize;

                uIter[0] = uIter[0] & *cIter;
                uIter[1] = uIter[1] & *cIter;
                uIter[2] = uIter[2] & *cIter;
                uIter[3] = uIter[3] & *cIter;
                uIter[4] = uIter[4] & *cIter;
                uIter[5] = uIter[5] & *cIter;
                uIter[6] = uIter[6] & *cIter;
                uIter[7] = uIter[7] & *cIter;

                tIter[0] = tIter[0] ^ uIter[0];
                tIter[1] = tIter[1] ^ uIter[1];
                tIter[2] = tIter[2] ^ uIter[2];
                tIter[3] = tIter[3] ^ uIter[3];
                tIter[4] = tIter[4] ^ uIter[4];
                tIter[5] = tIter[5] ^ uIter[5];
                tIter[6] = tIter[6] ^ uIter[6];
                tIter[7] = tIter[7] ^ uIter[7];

                ++cIter;
                uIter += 8;
                tIter += 8;
            }

            // transpose our 128 columns of 1024 bits. We will have 1024 rows, 
            // each 128 bits wide.
            sse_transpose128x1024(t);


            std::array<block, 2>* mStart = mIter;
            std::array<block, 2>* mEnd = std::min(mIter + 128 * superBlkSize, (std::array<block, 2>*)messages.end());

            // compute how many rows are unused.
            u64 unusedCount = (mIter + 128 * superBlkSize) - mEnd;

            // compute the begin and end index of the extra rows that 
            // we will compute in this iters. These are taken from the 
            // unused rows what we computed above.
            block* xEnd = std::min(xIter + unusedCount, extraBlocks.data() + 128);

            tIter = (block*)t.data();
            block* tEnd = (block*)t.data() + 128 * superBlkSize;

            while (mIter != mEnd)
            {
                while (mIter != mEnd && tIter < tEnd)
                {
                    (*mIter)[0] = *tIter;
                    (*mIter)[1] = *tIter ^ delta;

                    //u64 tV = tIter - (block*)t.data();
                    //u64 tIdx = tV / 8 + (tV % 8) * 128;
                    //Log::out << "midx " << (mIter - messages.data()) << "   tIdx " << tIdx << Log::endl;

                    tIter += superBlkSize;
                    mIter += 1;
                }

                tIter = tIter - 128 * superBlkSize + 1;
            }


            if (tIter < (block*)t.data())
            {
                tIter = tIter + 128 * superBlkSize - 1;
            }

            while (xIter != xEnd)
            {
                while (xIter != xEnd && tIter < tEnd)
                {
                    *xIter = *tIter;

                    //u64 tV = tIter - (block*)t.data();
                    //u64 tIdx = tV / 8 + (tV % 8) * 128;
                    //Log::out << "xidx " << (xIter - extraBlocks.data()) << "   tIdx " << tIdx << Log::endl;

                    tIter += superBlkSize;
                    xIter += 1;
                }

                tIter = tIter - 128 * superBlkSize + 1;
            }

            //Log::out << "blk end " << Log::endl;

#ifdef KOS_DEBUG
            BitVector choice(128 * superBlkSize);
            chl.recv(u.data(), superBlkSize * 128 * sizeof(block));
            chl.recv(choice.data(), sizeof(block) * superBlkSize);

            u64 doneIdx = mStart - messages.data();
            u64 xx = std::min(i64(128 * superBlkSize), (messages.data() + messages.size()) - mEnd);
            for (u64 rowIdx = doneIdx,
                j = 0; j < xx; ++rowIdx, ++j)
            {
                if (neq(((block*)u.data())[j], messages[rowIdx][choice[j]]))
                {
                    Log::out << rowIdx << Log::endl;
                    throw std::runtime_error("");
                }
            }
#endif
            //doneIdx = (mEnd - messages.data());
        }


#ifdef KOS_DEBUG
        BitVector choices(128);
        std::vector<block> xtraBlk(128);

        chl.recv(xtraBlk.data(), 128 * sizeof(block));
        choices.resize(128);
        chl.recv(choices);

        for (u64 i = 0; i < 128; ++i)
        {
            if (neq(xtraBlk[i] , choices[i] ? extraBlocks[i] ^ delta : extraBlocks[i] ))
            {
                Log::out << "extra " << i << Log::endl;
                Log::out << xtraBlk[i] << "  " << (u32)choices[i] << Log::endl;
                Log::out << extraBlocks[i] << "  " << (extraBlocks[i] ^ delta) << Log::endl;

                throw std::runtime_error("");
            }
        }
#endif


        block seed = prng.get<block>();
        chl.asyncSend(&seed, sizeof(block));
        block theirSeed;
        chl.recv(&theirSeed, sizeof(block));

        if (Commit(theirSeed) != theirSeedComm)
            throw std::runtime_error("bad commit " LOCATION);


        PRNG commonPrng(seed ^ theirSeed);

        block  chii, qi, qi2;
        block q2 = ZeroBlock;
        block q1 = ZeroBlock;

        SHA1 sha;
        u8 hashBuff[20];
        u64 doneIdx = 0;
        //Log::out << Log::lock;

        for (; doneIdx < messages.size(); ++doneIdx)
        {
            chii = commonPrng.get<block>();
            //Log::out << "sendIdx' " << doneIdx << "   " << messages[doneIdx][0] << "   " << chii << Log::endl;

            mul128(messages[doneIdx][0], chii, qi, qi2);
            q1 = q1  ^ qi;
            q2 = q2 ^ qi2;

            // hash the message without delta
            sha.Reset();
            sha.Update((u8*)&messages[doneIdx][0], sizeof(block));
            sha.Final(hashBuff);
            messages[doneIdx][0] = *(block*)hashBuff;

            // hash the message with delta
            sha.Reset();
            sha.Update((u8*)&messages[doneIdx][1], sizeof(block));
            sha.Final(hashBuff);
            messages[doneIdx][1] = *(block*)hashBuff;
        }


        u64 xtra = 0;
        for (auto& blk : extraBlocks)
        {
            chii = commonPrng.get<block>();

            //Log::out << "sendIdx' " << xtra++ << "   " << blk << "   " << chii << Log::endl;


            mul128(blk, chii, qi, qi2);
            q1 = q1  ^ qi;
            q2 = q2 ^ qi2;
        }

        //Log::out << Log::unlock;

        block t1, t2;
        std::vector<char> data(sizeof(block) * 3);

        chl.recv(data.data(), data.size());

        block& received_x = ((block*)data.data())[0];
        block& received_t = ((block*)data.data())[1];
        block& received_t2 = ((block*)data.data())[2];

        // check t = x * Delta + q 
        mul128(received_x, delta, t1, t2);
        t1 = t1 ^ q1;
        t2 = t2 ^ q2;

        if (eq(t1, received_t) && eq(t2, received_t2))
        {
            //Log::out << "\tCheck passed\n";
        }
        else
        {
            Log::out << "OT Ext Failed Correlation check failed" << Log::endl;
            Log::out << "rec t = " << received_t << Log::endl;
            Log::out << "tmp1  = " << t1 << Log::endl;
            Log::out << "q  = " << q1 << Log::endl;
            throw std::runtime_error("Exit");;
        }

        static_assert(gOtExtBaseOtCount == 128, "expecting 128");
    }


}
