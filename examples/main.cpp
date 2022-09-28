#include <iostream>

#include <cryptoTools/Common/Defines.h>
#include "cryptoTools/Crypto/RandomOracle.h"
using namespace osuCrypto;

#include <string.h>
#include <stdio.h>

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/IOService.h>
#include <numeric>
#include <cryptoTools/Common/Timer.h>
#include <cryptoTools/Common/BitVector.h>

#include <iomanip>
#include "libOTe/Tools/LDPC/LdpcImpulseDist.h"
#include "libOTe/Tools/LDPC/Util.h"
#include "cryptoTools/Crypto/RandomOracle.h"
#include "cryptoTools/Crypto/PRNG.h"

static const std::vector<std::string>
    simple{"s", "simplestOt", "simplest_ot", "simplestot", "simple"},
    kkrt{"kk", "kkrt"};

void Bot_Simplest_Test()
{
    PRNG prng0(block(12343,12341));
    PRNG prng1(block(12343,12433));

    u64 numOTs = 50;

    std::vector<block> recvMsg(numOTs);
    std::vector<std::array<block,2>> sendMsg(numOTs);
    BitVector choices(numOTs);
    choices.randomize(prng0);
}

int main(int argc, char **argv)
{

    std::cout << "argc: " << argc << " argv: " << argv << std::endl;
    for (int i = 0; i < argc; i++)
    {
        std::cout << "argv[" << i << "]:" << argv[i] << std::endl;
    }

    CLP cmd;
    cmd.parse(argc, argv);
    bool flagSet = false;

    std::cout << "kkrt: " << cmd.isSet(kkrt) << std::endl;
    
    auto t = cmd.getOr("t", 1);
    
    std::cout << "t: " << cmd.getOr("t", 1) << std::endl;

    auto r = cmd.getOr("r", 1);
    std::cout << "r: " << r << std::endl;
}