﻿#include "onlinegame.h"
#include <algorithm>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include "json.h"

using namespace std::string_literals;

namespace PokerGame
{
	namespace FAL
	{
		OnlineServer::OnlineServer()
		{
			this->multicastPort = 6666;
			this->multicastLocalBindPort = 6660;
			this->multicastIP = "239.0.1.10"s;
			this->multicastLocalBindIP = "192.168.1.102"s;
			this->serviceListenPort = 6665;
		}

		OnlineServer::OnlineServer(std::string configPath)
		{
			Json::CharReaderBuilder readerBuilder;
			Json::Value config;
			std::unique_ptr<Json::CharReader> const reader(readerBuilder.newCharReader());

			std::ifstream fileIn(configPath.c_str(), std::ios::binary);
			if (!fileIn.is_open())
			{
				throw std::exception();
			}
			std::stringstream configSS;
			std::string configJsonStr;
			std::string parseErr;
			configSS << fileIn.rdbuf();
			configJsonStr = configSS.str();

			reader->parse(configJsonStr.c_str(), configJsonStr.c_str() + configJsonStr.length(), &config, &parseErr);

			this->multicastPort = config["MulticastPort"].asInt();
			this->multicastIP = config["MulticastIP"].asString();
			this->multicastLocalBindPort = config["MulticastLocalBindPort"].asInt();
			this->multicastLocalBindIP = config["MulticastLocalBindIP"].asString();
			this->serviceListenPort = config["ServiceListenPort"].asInt();
		}

		void OnlineServer::Init()
		{
			try
			{
				this->NetInit();
				this->GameReset();
				this->PlayerReset();
			}
			catch (ServerInitFailedException&)
			{
				throw;
			}
		}

		void OnlineServer::NetInit()
		{

			// 初始化组播套接字
			this->broadcastSocketFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
			if (this->broadcastSocketFD == INVALID_SOCKET)
			{
				std::cout << WSAGetLastError();
				throw ServerInitFailedException();
			}
			this->clientBroadCastAddr.sin_family = AF_INET;
			this->clientBroadCastAddr.sin_port = htons(this->multicastPort);
			sockaddr_in localBind;
			localBind.sin_family = AF_INET;
			inet_pton(AF_INET, this->multicastLocalBindIP.c_str(), &localBind.sin_addr.s_addr);
			localBind.sin_port = htons(this->multicastLocalBindPort);
			int bindRet = bind(this->broadcastSocketFD, reinterpret_cast<sockaddr*>(&localBind), sizeof(sockaddr_in));
			if (bindRet != 0)
			{
				std::cout << WSAGetLastError();
				throw ServerInitFailedException();
			}
			inet_pton(AF_INET, this->multicastIP.c_str(), &this->clientBroadCastAddr.sin_addr.s_addr);

			// 初始化收听套接字
			this->serviceSocketFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
			if (this->serviceSocketFD == INVALID_SOCKET)
			{
				throw ServerInitFailedException();
			}
			sockaddr_in serviceListenAddr;
			serviceListenAddr.sin_family = AF_INET;
			serviceListenAddr.sin_addr.s_addr = INADDR_ANY;
			serviceListenAddr.sin_port = htons(this->serviceListenPort);
			bindRet = bind(this->serviceSocketFD, reinterpret_cast<sockaddr*>(&serviceListenAddr), sizeof(sockaddr_in));
			if (bindRet != 0)
			{
				std::cout << WSAGetLastError();
				throw ServerInitFailedException();
			}
		}

		void OnlineServer::Start()
		{
			auto broadcastSceneThread = std::thread(&OnlineServer::BroadCastSceneThread, this);
			broadcastSceneThread.detach();
			auto serviceListenThread = std::thread(&OnlineServer::ListenThread, this);
			broadcastSceneThread.detach();
		}

		void OnlineServer::Reset()
		{
			this->resetFlag = 0b11;
			while (this->resetFlag)
			{
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(1s);
			}
			this->GameReset();
			this->PlayerReset();
		}

		void OnlineServer::GameReset()
		{
			this->gameStage = STAGE_PREPARING;
			ClientID playerIDs[3];
			this->isLandlordDetermined = false;
			std::fill(std::begin(this->landlordWillingness), std::end(this->landlordWillingness), '\0');
			this->landlordIndex = 0;
			this->nextActPlayerIndex = -1;
			this->nextActType = ACTIVE_TYPE_ACTIVE;
			this->lastActType = ACTIVE_TYPE_PASS;
			this->secondLastActType = ACTIVE_TYPE_PASS;
			this->winnerFlag = 0b000;
			this->playerReadyFlag = 0b000;// 这里可能不需要重新准备
			this->lastAct.reset(new IdBasedCardCollection());
			this->secondLastAct.reset(new IdBasedCardCollection());
			for (int i = 0; i < 3; i++)
			{
				this->playerCards[i].reset(new SortedCardCollection());
			}
			this->lordCards.reset(new IdBasedCardCollection());
		}

		void OnlineServer::PlayerReset()
		{
			for (int i = 0; i < 3; i++)
			{
				this->playerIDs[i] = NullPlayer;
			}
		}

		void OnlineServer::BroadCastSceneThread()
		{
			while (true)
			{
				int reset = this->resetFlag & 0b01;
				if (reset)
				{
					this->resetFlag &= 0b10;
					return;
				}

				Scene current = this->FormCurrentScene();
				int sentlen = sendto(this->broadcastSocketFD,
					reinterpret_cast<char*>(&current),
					sizeof(Scene),
					0,
					reinterpret_cast<sockaddr*>(&this->clientBroadCastAddr),
					sizeof(sockaddr_in));
				using namespace std::chrono_literals;
				if (sentlen == -1)
				{
					std::cout << "broadcast thread dead"s << std::endl;
					break;
				}
				std::this_thread::sleep_for(0.5s);
			}
		}

		void OnlineServer::ListenThread()
		{
			while (true)
			{
				int reset = this->resetFlag & 0b10;
				if (reset)
				{
					this->resetFlag &= 0b01;
					return;
				}

				#pragma message("注意回收内存")
				char* buffer = new char[100];
				sockaddr_in clientIP = {0};
				int nameSize = sizeof(sockaddr_in);
				int rcvdLen = 
					recvfrom(this->serviceSocketFD, 
						buffer, 
						100, 
						0, 
						reinterpret_cast<sockaddr*>(&clientIP), 
						&nameSize);
				if (rcvdLen == -1)
				{
					int err = WSAGetLastError();
					//TODO : 考虑更多错误代码的处理方式
					switch (err)
					{
					case WSAEMSGSIZE:
						break;
					default:
						delete[] buffer;
						buffer = nullptr;
						std::cout << "service thread dead:" << err;
						return;
					}
				}
				// HandleMessage函数负责回收本次申请的内存
				auto msgProcessThread = std::thread(&OnlineServer::HandleMessage, this, buffer, rcvdLen, clientIP);
				msgProcessThread.detach();
			}
		}

		// 该函数负责回收ListenThread申请的内存, 内部不要开启新的线程, 以免指针悬空
		void OnlineServer::HandleMessage(char* buf, int bufLen, sockaddr_in addr)
		{
			constexpr int TwoSizeOfInt = 2 * sizeof(int);
			if (bufLen < TwoSizeOfInt)
			{
				delete[] buf;
				buf = nullptr;
				return;
			}
			int* pIntBuf = reinterpret_cast<int*>(buf);
			int nameHash = pIntBuf[0];
			int operationType = pIntBuf[1];
			int playerIndex = this->FindPlayerIndex(nameHash, addr);
			if (playerIndex == -1)
			{
				switch (operationType)
				{
				case CLIENT_DATAGRAM_TYPE_JOIN:
				{
					int availIndex = this->GetAvailableIndex();
					if (availIndex != -1)
					{
						this->playerIDs[availIndex].nameHashCode = nameHash;
						this->playerIDs[availIndex].ip = addr;
						std::string joinSuccess = std::to_string(availIndex);
						this->WriteBack(addr, joinSuccess);
					}
					else
					{
						std::string joinFail("-1");
						this->WriteBack(addr, joinFail);
					}
					break;
				}
				default:
					// 待定:客户端未加入时，是否也返回一条信息，以指示客户端人满/旁观?
					break;
				}
			}
			else
			{
				switch (operationType)
				{
				case CLIENT_DATAGRAM_TYPE_JOIN:
				{
					std::string alreadyIn = std::to_string(playerIndex);
					// 考虑到客户端重启的情况，不论该客户端是否已经加入，都返回索引
					this->WriteBack(addr, alreadyIn);
					break;
				}
				case CLIENT_DATAGRAM_TYPE_READY:
				{
					this->HandlePlayerPrepare(playerIndex);
					break;
				}
				case CLIENT_DATAGRAM_TYPE_GETCARD: // 该环节是最有可能并发的环节, 需慎重考虑
				{
					int count = this->playerCards[playerIndex]->Count();
					char* cardsBuf = new char[count];
					this->playerCards[playerIndex]->Serialize(cardsBuf, count);
					this->WriteBack(addr, cardsBuf, count);
					delete[] cardsBuf;
					cardsBuf = nullptr;
					break;
				}
				case CLIENT_DATAGRAM_TYPE_POSTPOINT:
				{
					int bufByteLeft = bufLen - TwoSizeOfInt;
					int willingness = 0;
					if (bufByteLeft >= 4)
					{
						willingness = pIntBuf[2];
					}
					this->HandlePlayerDeterminLandlord(playerIndex, willingness);
					break;
				}
				case CLIENT_DATAGRAM_TYPE_POSTCARD:
				{
					int cardBufLen = bufLen - TwoSizeOfInt;
					char* cardBuf = nullptr;
					if (cardBufLen != 0)
					{
						cardBuf = buf + TwoSizeOfInt;
					}
					this->HandlePlayerCardAct(playerIndex, cardBuf, cardBufLen);
					break;
				}
				default:

					break;
				}
			}
			delete[] buf;
			buf = nullptr;
		}

		void OnlineServer::WriteBack(sockaddr_in addr, std::string content)
		{
			int sentLen = sendto(this->serviceSocketFD, content.c_str(), content.length(), 0,
				reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr));
			if (sentLen == -1)
			{
				// TODO : 考虑是否需要重发或者进行其他处理
				return;
			}
		}

		// 该函数不要回收buf
		void OnlineServer::WriteBack(sockaddr_in addr, char* buf, int len)
		{
			int sentLen = sendto(this->serviceSocketFD, buf, len, 0,
				reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr));
			if (sentLen == -1)
			{
				// TODO : 考虑是否需要重发或者进行其他处理
				return;
			}
		}

		// 设置玩家为已准备, 并在所有玩家均已准备时推进游戏阶段
		// 此函数无需向客户端写回, 客户端从组播的Scene中判断是否成功准备
		void OnlineServer::HandlePlayerPrepare(int playerIndex)
		{
			this->handlePrepareMutex.lock();
			if (this->gameStage == STAGE_PREPARING)
			{
				this->playerReadyFlag |= 1 << playerIndex;
				if (this->IsAllPrepared())
				{
					//TODO : 这里还需要完成分发手牌以及修改nextActiceIndex等
					this->gameStage = STAGE_DETERMINE_LANDLORD;
				}
			}
			else
			{

			}
			this->handlePrepareMutex.unlock();
		}

		// 处理玩家的叫分请求
		// 此函数无需向客户端写回, 客户端从组播的Scene中判断是否成功叫分
		void OnlineServer::HandlePlayerDeterminLandlord(int playerIndex, int willingNess)
		{

		}

		// 处理玩家的出牌请求
		// 此函数不要回收cardAct
		// 此函数需要向客户端写回, 以在第一时间指示出牌是否成功 (虽然客户端也可以从组播的Scene判断, 但对于出牌来说延迟不应太大)
		void OnlineServer::HandlePlayerCardAct(int playerIndex, char* cardAct, int len)
		{

		}

		int OnlineServer::FindPlayerIndex(int nameHash, sockaddr_in addr) noexcept
		{
			ClientID clientID;
			clientID.nameHashCode = nameHash;
			clientID.ip = addr;
			for (int index = 0; index < 3; index++)
			{
				auto joinedClient = this->playerIDs[index];
				if (clientID == joinedClient)
				{
					return index;
				}
			}
			return -1;
		}

		Scene OnlineServer::FormCurrentScene() noexcept
		{
			Scene currentScene;
			currentScene.CurrentStage = static_cast<char>(this->gameStage);
			this->lastAct->Serialize(currentScene.LastCardDrop, 20);
			this->secondLastAct->Serialize(currentScene.SecondLastCardDrop, 20);
			currentScene.ActiveIndex = static_cast<char>(this->nextActPlayerIndex);
			switch (this->gameStage)
			{
			case STAGE_DETERMINE_LANDLORD:
			{
				auto maxWill = std::max_element(std::begin(this->landlordWillingness), std::end(this->landlordWillingness));
				currentScene.ActiveParam = *maxWill + 1;
				break;
			}
			case STAGE_MAINLOOP_ONGOING:
			{
				currentScene.ActiveParam = this->nextActType;
				break;
			}
			default:
				currentScene.ActiveParam = '\0';
				break;
			}
			currentScene.ActiveParam = this->nextActType;
			for (int i = 0; i < 3; i++)
			{
				currentScene.CardLeftCount[i] = static_cast<char>(this->playerCards[i]->Count());
			}
			if (this->isLandlordDetermined)
			{
				currentScene.LandlordIndex = static_cast<char>(this->landlordIndex);
				this->lordCards->Serialize(currentScene.LandlordCards, 3);
			}
			else
			{
				currentScene.LandlordIndex = static_cast<char>(-1);
				// 直到决定地主之前， 地主牌都不公布
				std::fill(std::begin(currentScene.LandlordCards), std::end(currentScene.LandlordCards), '\0');
			}
			std::copy(std::begin(this->landlordWillingness), 
				std::end(this->landlordWillingness), 
				std::begin(currentScene.LandlordWillingness));
			currentScene.LastActType = this->lastActType;
			currentScene.SecondLastActType = this->secondLastActType;
			currentScene.WinnerFlag = static_cast<char>(this->winnerFlag);
			currentScene.ReadyFlag = static_cast<char>(this->playerReadyFlag);
			return currentScene;
		}

		inline int OnlineServer::GetAvailableIndex()
		{
			for (int i = 0; i < 3; i++)
			{
				if (this->playerIDs[i] != NullPlayer)
				{
					return i;
				}
			}
			return -1;
		}

		inline int OnlineServer::IsAllPrepared()
		{
			return this->playerReadyFlag == 0b111;
		}
	}
}