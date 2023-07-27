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

	int atVecSortT_i = -1;

	bool operator ==(const Flow &a) { return this->id == a.id; };
};

class Port
{
public:
	int id = -1;
	int width = 0;

	int remainWidth = 0;       // 剩余带宽
	vector<Flow> sendingFlow;  // 正在发送的流
};

bool getFileData(int fileIndex, vector<Port> & ports, vector<deque<Flow>> & _flowsVectorSortT);

unsigned long timeCounter;  // 计时用

vector<Port> portVector;    // 端口 序列

vector<deque<Flow>> flowVectorSortT;  // 按照进入设备时间进行整理的flow组

int main(void)
{
	{
		int fileIndex = -1;
		while (true)
		{
			timeCounter = 0;
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
			int startFlowNum = 0, endFlowNum = 0;  // 记录 开始发送 和 发送完成 的流的数量
			unsigned int firstNotEmpty_i = 0;      // 记录第一个在 flowVectorSortT 中size不为 0 的索引
			for (; ; timeCounter++)   // 递增时间
			{
				// 按进入设备时间找到第一个 size 不为 0 的 flow 组
				for (unsigned int i = firstNotEmpty_i; (i <= timeCounter) && (i < flowVectorSortT.size()); i++)
				{
					if (flowVectorSortT[i].empty())
						continue;

					firstNotEmpty_i = i;
					break;
				}
				
				bool insertFlow2Port = true;
				/* ① 先选择最合适的流插入端口的发送队列 */
				while (insertFlow2Port)    // 是否要把流插入端口
				{
					unsigned int fullPortNum = 0;  // 已经塞满带宽的端口 个数
					Flow bestFlow;
					bestFlow.sendNeedTime = 1;
					bestFlow.needWidth = 1;
					for (unsigned int i = firstNotEmpty_i; (i <= timeCounter) && (i < flowVectorSortT.size()); i++)
					{
						deque<Flow>& flowVec = flowVectorSortT[i];
						if (!flowVec.empty() && flowVec[0].sendNeedTime >= bestFlow.sendNeedTime)
						{
							// 找一个最合适的flow
							bestFlow = flowVec.front();
							bestFlow.atVecSortT_i = i;
						}
					}

					if (bestFlow.id == -1)
						break;					

					// 查找可用端口(插入 最合适的流)
					for (auto& port : portVector)
					{
						if (port.remainWidth >= bestFlow.needWidth)
						{
							port.sendingFlow.push_back(bestFlow);
							port.remainWidth -= bestFlow.needWidth;
							flowVectorSortT[bestFlow.atVecSortT_i].pop_front();
							break;
						}
						else
						{
							fullPortNum++;
							if (fullPortNum == portVector.size())
								insertFlow2Port = false;  // 所有端口都没有空间了
						}
					}
				}


				/* ② 如果仍有带宽空隙，可安排小的流进入 */
				// if(找到ports最大剩余的带宽>=时间段内最小的流带宽)，在这个 带宽内<= 找当前时间内流最接近这个大小的流
				bool enoughFewWidth = true;
				while (enoughFewWidth)
				{
					int minWidthFlow_i_atVectSortT = -1;
					Flow minWidthFlow;
					for (unsigned int i = firstNotEmpty_i; (i <= timeCounter) && (i < flowVectorSortT.size()); i++)
					{
						deque<Flow>& _flowVec = flowVectorSortT[i];
						if (!_flowVec.empty())
						{
							auto minWidthFlowIt = min_element(_flowVec.begin(), _flowVec.end(),
								[](const Flow& a, const Flow& b) {return a.needWidth < b.needWidth; });
							if (minWidthFlowIt->needWidth < minWidthFlow.needWidth)
							{
								minWidthFlow = *minWidthFlowIt;
								minWidthFlow_i_atVectSortT = i;
							}
						}
					}

					if (minWidthFlow_i_atVectSortT >= 0)  // 当前时间段内是否还有需要发送的流
					{
						// 找到ports最大剩余的带宽及其位置
						int maxRemainPortWidth = -1, maxRemainPortWidth_i_atPortVector = -1;
						auto maxRemainPortWidthIt = max_element(portVector.begin(), portVector.end(), [](Port a, Port b) {return a.remainWidth < b.remainWidth; });
						maxRemainPortWidth = (*maxRemainPortWidthIt).remainWidth;
						maxRemainPortWidth_i_atPortVector = maxRemainPortWidthIt - portVector.begin();

						if (maxRemainPortWidth >= minWidthFlow.needWidth)  // 最大剩余的带宽是否大于时间段内最小的流带宽
						{
							map<int, Flow> bestInsertFlowMap;

							for (unsigned int i = firstNotEmpty_i; (i <= timeCounter) && (i < flowVectorSortT.size()); i++)
							{
								deque<Flow>& flowVec = flowVectorSortT[i];
								// 这里考虑找出所有小于maxRemainPortWidth的流，并将他们存进 Map<i, Flow>中
								// 还是找出flowVectorSortT[i]内小于maxRemainPortWidth的但占用带宽最大的流
								if (!flowVec.empty())
								{
									Flow tempFlow;  // flowVec中小于maxRemainPortWidth的但占用带宽最大的流
									tempFlow.needWidth = -1;
									int tempFlowIt_i = -1;

									for (auto flowIt = flowVec.begin(); flowIt != flowVec.end(); ++flowIt)
									{
										if (flowIt->needWidth <= maxRemainPortWidth && flowIt->needWidth > tempFlow.needWidth)
										{
											tempFlow = *flowIt;
											tempFlow.atVecSortT_i = i;
											tempFlowIt_i = flowIt - flowVec.begin();
										}
									}

									if (tempFlow.needWidth > 0)
										bestInsertFlowMap.insert(pair<int, Flow>(tempFlowIt_i, tempFlow));
								}
							}

							auto maxWidthMapIt = max_element(bestInsertFlowMap.begin(), bestInsertFlowMap.end(), 
								[](const pair<int, Flow>& a, const pair<int, Flow>& b) {return a.second.needWidth < b.second.needWidth; });

							auto& needDelFlowVec = flowVectorSortT[maxWidthMapIt->second.atVecSortT_i];
							needDelFlowVec.erase(maxWidthMapIt->first + needDelFlowVec.begin());
							portVector[maxRemainPortWidth_i_atPortVector].sendingFlow.push_back(maxWidthMapIt->second);
							portVector[maxRemainPortWidth_i_atPortVector].remainWidth -= maxWidthMapIt->second.needWidth;
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

				/* 打印端口带宽使用效率 */
#ifdef DEBUG_COUT				
				for (const auto& port_ : portVector)
				{
					float temp = (port_.width - port_.remainWidth) / float(port_.width);
					if (temp > 0.996)
					{

						cout << port_.id << ": " << setw(4) << "  " << "%  ";
					}
					else
					{
						cout << port_.id << ": " << setw(4) << setprecision(3) << (temp * 100.0) << "%  ";

					}
				}
				cout << "  ----- " << timeCounter << endl;
#endif
				/* ③ 每个端口的发送队列状态更新 */
				for (auto& port : portVector)
				{
					vector<Flow> needDelete;    // 发送结束需要删除的流
					for (auto& flow : port.sendingFlow)
					{
						if (flow.sendNeedTime == flow.sendRemainTime && flow.isSending == false)
						{
							// 这是新加入发送队列的流，将其标记为发送状态并输出 result 文件
							flow.isSending = true;
							startFlowNum++;
#ifdef DEBUG_COUT
							cout.setf(ios_base::left, ios_base::adjustfield);
#endif
							resultFile << flow.id << "," << port.id << "," << timeCounter << endl;
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
				}

				if (endFlowNum == startFlowNum && endFlowNum == sendFlow2PortSize)
				{
					resultFile.close();  // 关闭文件
					break;
				}
			}
#ifdef ITERA_COUT
			cout << fileIndex << " w_W:" << setw(7) << width_Weight << " time:" << timeCounter << endl;
#endif
			//getchar();
		}
#ifdef ITERA_COUT
		cout << endl;
#endif
	}
#ifdef DEBUG_COUT
	cout << "Program Run Over" << endl;
#endif

	//getchar();
	return 0;
}

bool getFileData(int fileIndex, vector<Port> & ports, vector<deque<Flow>> & _flowsVectorSortT)
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
	// 端口按照带宽升序排列
	sort(ports.begin(), ports.end(), [](Port a, Port b) {return a.remainWidth < b.remainWidth; });

	flowFile.ignore(255, '\n');  // 抛弃第一行
	while (!flowFile.eof())
	{
		Flow aFlow;
		char _c = -1;
		flowFile >> aFlow.id >> _c >> aFlow.needWidth >> _c >> aFlow.enterDevTime >> _c >> aFlow.sendNeedTime;
		aFlow.sendRemainTime = aFlow.sendNeedTime;
		if(_c == -1)  // 文件末尾
			break;

		if (aFlow.enterDevTime + 1 > _flowsVectorSortT.size())  // 扩容
			_flowsVectorSortT.resize(aFlow.enterDevTime + 1);
		_flowsVectorSortT[aFlow.enterDevTime].push_back(aFlow);
	}
	
	for (auto & flowVec : flowVectorSortT)
	{
		sort(flowVec.begin(), flowVec.end(),
			[](const Flow& a, const Flow& b)
			{
				// 按照发送需要的时间降序排列
				return (a.sendNeedTime > b.sendNeedTime);
			} 
		);
	}

	portFile.close();
	flowFile.close();

	return true;
}

