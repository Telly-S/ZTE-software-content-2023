//#include <iostream>
//#include <iomanip>
#include <string>
#include <fstream>
#include <vector>
#include <queue>
#include <deque>
#include <map>
#include<algorithm>

using namespace std;
//#define DEBUG_COUT
//#define ITERA_COUT

class Flow
{
public:
	int id = -1;
	int needWidth = 999999;
	int sendNeedTime = 0;
	int sendRemainTime = -1;  // 发送过程还剩多少时间

	int enterDevTime = -1;
	bool isSending = false;

	bool operator ==(const Flow &a) { return this->id == a.id; };
};

class Port
{
public:
	int id = -1;
	int width = 0;

	int remainWidth = 0;       // 剩余带宽
	vector<Flow> sendingFlow;  // 正在发送的流

	queue<Flow> waitQueue;
};

bool getFileData(int fileIndex, vector<Port> & ports, vector<deque<Flow>> & _flowsVectorSortT);
double width_Weight = 1.5e-5;   // 带宽权重
//double width_Weight = 0.0;      // 带宽权重

unsigned long timeCounter;      // 计时用
unsigned int calcSumTimePoint;  // 计算分数用
unsigned int discardFlowCount;  // 丢弃的流数量

vector<Port> portVector;        // 端口 序列

vector<deque<Flow>> flowVectorSortT;  // 按照进入设备时间进行整理的flow组
unsigned int maxDispatchAreaSize;     // 最大调度区流数量
deque<Flow> dispatchAreaFlowDeque;    // 调度区中的流

int main(void)
{
	//for (width_Weight = 0; width_Weight <= 1; width_Weight += 0.000001)
	{
		int fileIndex = -1;
		while (true)
		{
			calcSumTimePoint = 0;
			timeCounter = 0;
			discardFlowCount = 0;
			portVector.clear();
			flowVectorSortT.clear();
			unsigned int sendFlow2PortSize = 0;  // 全部流的数量

			++fileIndex;
			if (!getFileData(fileIndex, portVector, flowVectorSortT))
			{
#ifdef DEBUG_COUT
				cout << "Open File Error, Exit" << endl;
#endif
				break;
			}

			timeCounter = 0;
			ofstream resultFile;
			resultFile.open(R"(../data/)" + to_string(fileIndex) + "/result.txt", ios::out);
			for_each(flowVectorSortT.begin(), flowVectorSortT.end(), [&sendFlow2PortSize](const deque<Flow>& flows) {sendFlow2PortSize += flows.size(); });
			unsigned int startFlowNum = 0, endFlowNum = 0;  // 记录 开始发送 和 发送完成 的流的数量
			unsigned int firstNotEmpty_i = 0;               // 记录第一个在 flowVectorSortT 中size不为 0 的索引
			for (; ; timeCounter++)   // 递增时间
			{
				/* ① 先把该时刻的所有流存入调度区 */
				for (unsigned int i = firstNotEmpty_i; i <= timeCounter && (i < flowVectorSortT.size()); i++)
				{
					deque<Flow>& flowVec = flowVectorSortT[i];
					if (!flowVec.empty())
					{
						dispatchAreaFlowDeque.insert(dispatchAreaFlowDeque.end(), flowVec.begin(), flowVec.end());
						flowVec.clear();
						firstNotEmpty_i = i;
					}
				}

				if (timeCounter <= flowVectorSortT.size())
				{
					// 把调度区的流按照权重算法排序
					double _width_Weight = width_Weight;
					sort(dispatchAreaFlowDeque.begin(), dispatchAreaFlowDeque.end(),
						[_width_Weight](const Flow& a, const Flow& b)
						{
							return (a.needWidth * _width_Weight + (1.0 - _width_Weight) / a.sendNeedTime < b.needWidth * _width_Weight + (1.0 - _width_Weight) / b.sendNeedTime);
						}
					);
				}

				/* ② 先尽量把按权重排序的流塞入无排队队列的port */
				while (!dispatchAreaFlowDeque.empty())
				{
					auto& _dispatchAreaFlowDq = dispatchAreaFlowDeque;
					auto noWaitQueuePortIt = find_if(portVector.begin(), portVector.end(), [&_dispatchAreaFlowDq](const Port& aPort) {return aPort.waitQueue.empty() && aPort.remainWidth >= _dispatchAreaFlowDq.front().needWidth; });
					if (noWaitQueuePortIt != portVector.end())
					{
						// 这里可以直接插入端口的发送队列
						noWaitQueuePortIt->sendingFlow.push_back(dispatchAreaFlowDeque.front());
						noWaitQueuePortIt->remainWidth -= dispatchAreaFlowDeque.front().needWidth;
						resultFile << dispatchAreaFlowDeque.front().id << "," << noWaitQueuePortIt->id << "," << timeCounter << endl;
						dispatchAreaFlowDeque.pop_front();
					}
					else
					{
						break;
					}
				}
				
				/* ③ 如果仍有无等待队列的port有带宽空隙，可安排小的流进入 */
				// if(找到无等待队列的ports最大剩余的带宽>=调度区内最小的流带宽)，在这个 带宽内<= 找调度区内流最接近这个大小的流
				bool enoughFewWidth = true;
				while (enoughFewWidth)
				{
					Flow minWidthFlow;
					int minWidthFlowVec_i = -1;

					deque<Flow>& _flowVec = dispatchAreaFlowDeque;
					if (!_flowVec.empty())
					{
						auto minWidthFlowIt = min_element(_flowVec.begin(), _flowVec.end(), [](Flow a, Flow b) {return a.needWidth < b.needWidth; });
						if (minWidthFlowIt->needWidth < minWidthFlow.needWidth)
						{
							minWidthFlow = *minWidthFlowIt;
							minWidthFlowVec_i = minWidthFlowIt - _flowVec.begin();
						}
					}

					if (minWidthFlowVec_i >= 0)  // 当前时间段内是否还有需要发送的流
					{
						// 找到ports最大剩余的带宽及其位置
						int maxRemainPortWidth = -1, maxRemainPortWidth_i_atPortVector = -1;
						// 找到无等待队列的
						auto maxRemainPortWidthIt = max_element(portVector.begin(), portVector.end(),
							[](const Port& a, const Port& b) {return (b.waitQueue.empty() && a.remainWidth < b.remainWidth); });
						maxRemainPortWidth = (*maxRemainPortWidthIt).remainWidth;
						maxRemainPortWidth_i_atPortVector = maxRemainPortWidthIt - portVector.begin();
						
						// 最大剩余的带宽是否大于调取区内最小的流带宽
						if (maxRemainPortWidthIt->waitQueue.empty() && maxRemainPortWidth >= minWidthFlow.needWidth)
						{
							Flow bestInsertFlow;
							bestInsertFlow.needWidth = -1;
							auto bestInsertFlow_dequeIt = dispatchAreaFlowDeque.end();
							if (!dispatchAreaFlowDeque.empty())
							{
								// dispatchAreaFlowDeque中小于maxRemainPortWidth的但占用带宽最大的流
								for (auto flowIt = dispatchAreaFlowDeque.begin(); flowIt != dispatchAreaFlowDeque.end(); ++flowIt)
								{
									if (flowIt->needWidth <= maxRemainPortWidth && flowIt->needWidth > bestInsertFlow.needWidth)
									{
										bestInsertFlow = *flowIt;
										bestInsertFlow_dequeIt = flowIt;
									}
								}
							}
							if (bestInsertFlow.needWidth > 0)
							{
								portVector[maxRemainPortWidth_i_atPortVector].sendingFlow.push_back(bestInsertFlow);
								portVector[maxRemainPortWidth_i_atPortVector].remainWidth -= bestInsertFlow.needWidth;
								resultFile << bestInsertFlow.id << "," << portVector[maxRemainPortWidth_i_atPortVector].id << "," << timeCounter << endl;
								dispatchAreaFlowDeque.erase(bestInsertFlow_dequeIt);
							}
						}
						else
						{
							enoughFewWidth = false;  // ports 带宽已经完全不够用了
						}
					}
					else
					{
						enoughFewWidth = false;  // 当前时间下已经没有需要发送的流了
					}
				}

				unsigned int portsWaitQueueAvaliableSize = 0;  // 所有可用的排队区空闲大小
				for (const auto& port : portVector)
				{
					if (port.waitQueue.size() < 30)
					{
						portsWaitQueueAvaliableSize += (30 - port.waitQueue.size());
						break;
					}
				}
				bool _insertFlow2PortOrWaitQueue = true;
				for (; _insertFlow2PortOrWaitQueue && portsWaitQueueAvaliableSize > 0 && dispatchAreaFlowDeque.size() > maxDispatchAreaSize;
					portsWaitQueueAvaliableSize = 0, for_each(portVector.begin(), portVector.end(), [&portsWaitQueueAvaliableSize](const Port& port)
						{if (port.waitQueue.size() < 30) portsWaitQueueAvaliableSize += (30 - port.waitQueue.size()); }))  // 说明还有端口等待队列有空余
				{
					auto minWidthFlowIt = min_element(dispatchAreaFlowDeque.begin(), dispatchAreaFlowDeque.end(), [](const Flow& a, const Flow& b) {return a.needWidth < b.needWidth; });
					//auto minWidthFlowIt = min_element(dispatchAreaFlowDeque.begin(), dispatchAreaFlowDeque.begin() + maxDispatchAreaSize,
					//	[](const Flow& a, const Flow& b) {return a.needWidth < b.needWidth; });

					// 查找可用端口(插入其等待队列中)
					unsigned int _fullPortWaitQueueCount = 0;  // 排队区已满的端口数量
					for (auto& aPort : portVector)
					{
						if (aPort.width >= (*minWidthFlowIt).needWidth && aPort.waitQueue.size() < 30)
						{
							aPort.waitQueue.push(*minWidthFlowIt);
							resultFile << (*minWidthFlowIt).id << "," << aPort.id << "," << timeCounter << endl;
							dispatchAreaFlowDeque.erase(minWidthFlowIt);
							break;
						}
						_fullPortWaitQueueCount++;
						if (_fullPortWaitQueueCount == portVector.size())
							// 说明此时的 dispatchAreaFlowDeque.front() 已经无处可插了
							_insertFlow2PortOrWaitQueue = false;
					}					
				}

				// 找排队区已满的port丢弃多余的
				while (dispatchAreaFlowDeque.size() > maxDispatchAreaSize)
				{
					for (const auto& port : portVector)
					{
						if (port.waitQueue.size() >= 30 && port.width >= dispatchAreaFlowDeque.back().needWidth)
						{
							int minSendRemainTime = (*min_element(port.sendingFlow.begin(), port.sendingFlow.end(), [](const Flow& a, const Flow& b) {return a.sendRemainTime < b.sendRemainTime; })).sendRemainTime;
							if (minSendRemainTime > 1)
							{
								discardFlowCount++;
								calcSumTimePoint += dispatchAreaFlowDeque.back().sendNeedTime * 2;
								resultFile << dispatchAreaFlowDeque.back().id << "," << port.id << "," << timeCounter << endl;
								dispatchAreaFlowDeque.pop_back();
								sendFlow2PortSize--;
								break;
							}
						}
					}
				}

				/* 打印端口带宽使用效率 */
#ifdef DEBUG_COUT				
				for (const auto& port_ : portVector)
				{
					float percent = (port_.width - port_.remainWidth) / float(port_.width);
					if (percent > 0.95)
					{
						cout << port_.id << ":" << setw(2) << "  " << "%>" << setw(2) << port_.waitQueue.size() << "  ";
					}
					else
					{
						cout << port_.id << ":" << setw(2) << static_cast<int>(percent * 100) << "%>" << setw(2) << port_.waitQueue.size() << "  ";
					}
				}
				cout << "  ----- " << timeCounter << endl;
#endif
				/* ④ 每个端口的发送队列状态更新 */
				for (auto& port : portVector)
				{
					vector<Flow> needDelete;    // 发送结束需要删除的流
 					for (auto& flow : port.sendingFlow)
					{
						if (flow.sendNeedTime == flow.sendRemainTime && flow.isSending == false)
						{
							flow.isSending = true;
							startFlowNum++;
#ifdef DEBUG_COUT
							cout.setf(ios_base::left, ios_base::adjustfield);
#endif
							//resultFile << flow.id << "," << port.id << "," << timeCounter << endl;
						}
						flow.sendRemainTime--;
						if (flow.sendRemainTime == 0)
						{
							endFlowNum++;
							needDelete.push_back(flow);  // 发送结束需要删除的流
						}
					}
					for (auto& del : needDelete)
					{
						port.sendingFlow.erase(find(port.sendingFlow.begin(), port.sendingFlow.end(), del));
						port.remainWidth += del.needWidth;
					}

					// port waitQueue进入端口发送队列 更新
					while (!port.waitQueue.empty() && port.remainWidth >= port.waitQueue.front().needWidth)
					{ // 删除和放入port是同时的吗? 
						port.sendingFlow.push_back(port.waitQueue.front());
						port.remainWidth -= port.waitQueue.front().needWidth;
						port.waitQueue.pop();
					}
				}

				if (endFlowNum == startFlowNum && endFlowNum == sendFlow2PortSize)
				{
					resultFile.close();  // 关闭文件
					break;
				}
			}
#ifdef ITERA_COUT
			cout << fileIndex << " wW:" << setw(6) << setprecision(4) << width_Weight << "  time:" << setw(5) << timeCounter
				<< " discardNum:" << setw(5) << discardFlowCount
				<< " allTimePoint:" << calcSumTimePoint << endl;
#endif
			//getchar();
		}
	}
#ifdef DEBUG_COUT
	cout << "Program Run Over" << endl;
#endif

	//getchar();
	return 0;
}

bool getFileData(int fileIndex, vector<Port>& ports, vector<deque<Flow>>& _flowsVectorSortT)
{
	string dataDir = R"(../data/)";
	string flowtxtName = dataDir + to_string(fileIndex) + "/flow.txt";
	string porttxtName = dataDir + to_string(fileIndex) + "/port.txt";
	ifstream flowFile(flowtxtName), portFile(porttxtName);
	//判断文件是否正常打开
	if (!flowFile || !portFile)
	{
#ifdef DEBUG_COUT
		cout << "open error: " << flowtxtName << endl;
#endif
		return false;
	}
#ifdef DEBUG_COUT
	cout << "open success: " << flowtxtName << endl;
#endif

	portFile.ignore(255, '\n');  // 抛弃第一行
	while (!portFile.eof())
	{
		Port aPort;
		char _c = -1;
		portFile >> aPort.id >> _c >> aPort.width;
		aPort.remainWidth = aPort.width;
		if (_c == -1)  // 文件末尾
			break;

		ports.push_back(aPort);
	}
	sort(ports.begin(), ports.end(), [](Port a, Port b) {return a.remainWidth < b.remainWidth; });
	maxDispatchAreaSize = ports.size() * 20;

	flowFile.ignore(255, '\n');  // 抛弃第一行
	while (!flowFile.eof())
	{
		Flow aFlow;
		char _c = -1;
		flowFile >> aFlow.id >> _c >> aFlow.needWidth >> _c >> aFlow.enterDevTime >> _c >> aFlow.sendNeedTime;
		aFlow.sendRemainTime = aFlow.sendNeedTime;
		if (_c == -1)  // 文件末尾
			break;

		if (aFlow.enterDevTime + 1 > _flowsVectorSortT.size())  // 扩容
			_flowsVectorSortT.resize(aFlow.enterDevTime + 1);
		_flowsVectorSortT[aFlow.enterDevTime].push_back(aFlow);
	}

 	//double _width_Weight = width_Weight;
 	//for (auto& flowVec : _flowsVectorSortT)
 	//{
 	//	sort(flowVec.begin(), flowVec.end(),
 	//		[_width_Weight](Flow a, Flow b)
 	//		{
 	//			return (a.needWidth * _width_Weight + (1.0 - _width_Weight) / a.sendNeedTime < b.needWidth * _width_Weight + (1.0 - _width_Weight) / b.sendNeedTime);
 	//		}
 	//	);
 	//}
	portFile.close();
	flowFile.close();

	return true;
}

