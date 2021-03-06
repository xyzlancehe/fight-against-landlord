# 斗地主C++

2021.8.14

+ 稍微写了一点客户端
+ ~~隔壁叔叔七夕了还在一个人写代码，你可千万不能变成那样~~

---

2021.8.13

+ 完成了服务器对于准备和叫分消息的处理
  + 但是没有进行测试。比较重要的对于出牌消息的处理部分也没写。目前应当先开发一下客户端，然后边写边测试。
+ 创建了客户端的项目
  + 技术选型：C#，dotnet5.0，WPF
  + 简单写了一些基本类型的框架，简单添加了一些UI元素
  + 简单实现了一下输入用户名的功能，底层通过字符串的哈希值作为标识
+ Scene类型又添加了服务器自身的IP信息，这样客户端就可以直接从广播中获取服务器地址，不需要用户输入了

---

2021.8.12

+ 继续编写服务器
  + 主要是实现HandleMessage的流程
  + 也优化了一些之前写的内容

---

2021.8.11

+ 继续编写服务器
  + 实现了组播，并在本机、虚拟机、另一台设备同时进行接收，初步测试通过。
    + 对于有多张网卡（包括虚拟网卡）的设备，加入多播组时，必须指定网卡接口索引为连接局域网的那一个，否则将无法播送到。目前客户端是手动在powershell中使用命令查看，然后再修改源代码。测试中三个设备的接口索引均不同。正式的客户端中如何解决此问题？（研究一下有没有获取索引相关的库函数/从json或者其他文件动态地加载配置）
    + 客户端（C#）中，直接使用UdpClient类的加入多播组的方法（JoinMulticastGroup）无法指定网卡索引（使用不指定网卡索引的重载方法无法保证连接正确，使用指定索引的重载方法会报错），但是使用基本的Socket类，按照基本步骤加入多播组（Bind，SetSocketOpiton）就可以正确运行。原因？
  + 引入了jsoncpp库，实现了从json加载服务器配置（主要是各种地址）。
  + 重命名了一些字段、方法。
  + 其他方法编写中。

---

2021.8.9~10

+ 编写服务器，基于UDP通信
  + 初步设想：通过组播定期下发场景信息（包括各个玩家剩余手牌数量，地主牌，玩家行动状况等），每个玩家再独立查询自己的手牌以及提交抢地主、出牌等信息。

+ 定义了Scene结构体，用于描述场景信息
+ PokerCardCollection类增加了Serialize方法，用于将自身编码成字符数组（每个字符存储一张牌的id）

---

2021.8.7

+ 重写了飞机的构造函数，解决了先前所述的bug（大概吧，3+3，3+3+2，4+3+1，3+3+2+2，3+3+4测试通过，但也仅限这些样本，没有进行更多的测试）。
  + 虽然功能实现了，代码不是太规范：switch块部分的代码重复率太高，以及使用了goto语句跳出for循环中的switch。后续可以考虑通过封装成函数、调整流程的方式，降低重复率，并且不使用goto。
+ 调整了飞机类的私有字段，增加了飞机主干和翅膀的牌集cache（数据有些冗余了，但是后续计划覆写所有牌型的ToString方法，那时候会方便一些）
+ 添加了onlinegame源文件，但是还没写实际内容
  + 经过我对于TCP和UDP的进一步研究，可能要放弃之前的Player类型和游戏流程，重写一个流程（借此机会尝试事件驱动），具体如何实现还需要进一步考虑，可能两种协议都会用到。

---

2021.8.5

+ 实现了飞机，但是存在诸多bug，比如3+3+2或者4+3+1的飞机不能识别，更大的连续范围也存在类似bug，后续需要重写
+ 修复了将点数转换为3开头大小的方法(PokerCard::NormalPointTo3BasedNum)的bug
+ 修复了三张类型的bug：构造纯3张牌型时没有把牌加入到自身
+ 在Linux环境下生成了工程并通过测试

---

2021.8.4

+ 完成了王炸和连对的牌型
+ 修复了抢地主逻辑的一些bug
+ 修复了读取手牌的一些bug：牌被重复中以及打出的牌仍然在手中留存一份
+ 修复了三张类型构造函数的bug：传入同一点数的数量不等于3的牌时没有抛出异常，而是构造了空的牌集
+ 修复了一些类型的FormatCollection方法存在的内存泄漏

---

2021.8.3

+ 完成了顺子和炸弹的牌型
+ 新增了本地人工玩家，使用命令行交互
+ 修改了IdBasedCollection的operator[]，会抛出std::out_of_range异常
+ 实现了IdBasedCollection的PickOut(PokerCardCollection&)，（暂时使用，可能会改）
+ TypedCardCollection类增加了TryCast方法，尝试使用没有任何类型信息的牌构造一个有牌型的集合。
+ 修复了三张类型的bug

需要完成的：

+ 本地人工玩家需要进一步测试
+ 本地人工玩家在抢地主分数的显示上有bug需要修复
+ 本地人工玩家从命令行读取出牌输入的具体流程有待优化
+ IdBasedCollection的PickOut(PokerCardCollection&)的具体实现有待考虑和优化
+ TypedCardCollection的TryCast方法（或者该功能的实现手段）有待考虑

---

2021.8.1

+ 完成了对子和三张的类型
+ 修改了抢地主流程，不叫分也会抬高最低叫分的数值
+ CardEvent类更名为CardEventArgs类，TypedCardEvent同

---

2021.7.31

改动：

+ 将牌型相关的类型声明和定义单独移到了新的头/源文件中
+ 新增了TypedCardEvent类，继承自CardEvent，携带牌型信息，并且将Player类的CardResponse方法的参数改为此类型，相关的地方也做了相应调整
+ 源文件中加入了一些#pragma region块

需要完成的：

+ CardResponse方法目前的返回值仍是基本的CardEvent类型，但是逻辑上也需要携带牌型信息；实际程序中，返回的也都是Typed的类型，并在调用方进行了向下造型；如果返回的不是Typed的类型，可能会出错。后续应考虑修改返回值类型，或者考虑增加其他的接口。

---

2021.7.30

+ 一些使用裸指针的地方已经改成了智能指针
+ 重载了PokerCard类的ToString方法，可以返回图形风格的字符串表示，但是在本机的VS2019的调试控制台（GBK编码）下，特殊字符无法显示
+ PokerCardCollection类新增ToString方法，并在IdBasedCardCollection里给出了一个简单的实现。后续不同牌型预计要覆写此方法
+ 将GameProcess的主流程拆成了多个函数，并额外打印了一些提示信息
+ 对Player类的一些方法补充了noexcept声明

---

2021.7.28初次提交

已经实现了一些基本的类型和过程，并且实现了一个简单的测试案例。

接下来需要完成的：

+ 游戏规则方面

需要补完斗地主牌型方面的规则，大体思路是每一种牌型都继承自抽象类TypedCardCollection并且实现所有的抽象方法，

```C++
enum class GeneralCardType
{
    Dan, Dui, San, Zha, Shun, LianDui, FeiJi, LianZha, WangZha
};

class TypedCardCollection : public IdBasedCardCollection
{
    public:
    virtual GeneralCardType GetGeneralType() = 0;
    virtual bool IsSameType(TypedCardCollection& other) = 0;
    virtual bool IsLargerThan(TypedCardCollection& other) = 0;
    virtual ~TypedCardCollection(){}
};
```

这是一个用于专门表示某一种牌型的PokerCardCollection类，每一个具体实现对应一种牌型，可以判断是否与另一个牌集属于同一牌型，以及是否大于该牌集。

另外，构造函数也需要专门实现，以保证该类符合对应的牌型。

+ 平台方面

初步设想是联网功能的实现全部写在OnlinePlayer类里：

```C++
class OnlinePlayer : public Player
```

目前的流程是基于抽象类Player的，只要实现了相应的抽象方法就能运行。

在此基础上，OnlinePlayer类需要负责作为服务器，与客户端进行网络通信。另外，出牌的合法性暂时依靠Player类自身来保证。

+ 其他
  + 将一些使用裸指针的部分改为智能指针，防止内存泄漏
  + PokerCardCollection提供的接口可能需要调整（尤其是PickOut相关功能）
  + 目前把整个流程写在了一个大函数里，应当拆分成多个函数
  + 暂时没有考虑没人抢叫地主的情况，后续需要补充
  + 考虑将主流程改成由事件驱动，以便扩充更多内容

