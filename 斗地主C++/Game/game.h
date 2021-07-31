﻿#pragma once
#include "poker.h"
#include "cardtype.h"
#include <memory>
#include <string>

namespace PokerGame
{
	namespace FAL
	{
		

		class CardEvent
		{
		public:
			enum class MotionType {
				Active, Follow, Pass
			};
			virtual MotionType GetType() const;
			virtual std::shared_ptr<PokerCardCollection> GetContent() const;
			CardEvent(MotionType type);
			CardEvent(MotionType type, std::shared_ptr<PokerCardCollection>& content);
		protected:
			MotionType type;
			std::shared_ptr<PokerCardCollection> content;
		};

		class TypedCardEvent : public CardEvent
		{
		public:
			//using CardEvent::GetType;
			virtual std::shared_ptr<PokerCardCollection> GetContent() const;
			virtual std::shared_ptr<TypedCardCollection> GetTypedContent() const;
			TypedCardEvent(MotionType type);
			TypedCardEvent(MotionType type, std::shared_ptr<TypedCardCollection>& content);
		};


		class Player
		{
		public:
			virtual int PrepareResponse(int leastPoint) noexcept = 0;
			virtual CardEvent CardResponse(TypedCardEvent e) noexcept = 0;
			virtual PokerCardCollection& GetCards() = 0;
			virtual std::string GetName() = 0;
			virtual void Reset() = 0;
		};

		class GameProcess
		{
		public:
			static const int PlayerNum = 3;
			GameProcess(std::shared_ptr<Player>& p1,
				std::shared_ptr<Player>& p2,
				std::shared_ptr<Player>& p3);
			~GameProcess();
			void Run();
		private:
			int Magnification;
			int LandlordIndex;
			bool LandlordSelected;
			std::shared_ptr<Player> players[PlayerNum];
			std::unique_ptr<PokerCardCollection> LordCards;
			void ResetGame();
			void HandoutInitCards();
			void DetermineLandlord();
			void MainCardLoop();
		};


		class OnlinePlayer : public Player
		{
		public:
			OnlinePlayer();
			OnlinePlayer(int port);
		};

		class LocalPlayer : public Player
		{

		};

		class StupidLocalPlayerForDebugging : public LocalPlayer
		{
		public:
			virtual int PrepareResponse(int leastPoint) noexcept;
			virtual CardEvent CardResponse(TypedCardEvent e) noexcept;
			virtual PokerCardCollection& GetCards();
			virtual std::string GetName();
			virtual void Reset();
			StupidLocalPlayerForDebugging(std::string name);
		private:
			SortedCardCollection cards;
			std::string name;
		};




	}



}