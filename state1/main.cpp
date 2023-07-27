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
	int sendRemainTime = -1;  // ���͹��̻�ʣ����ʱ��

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

	int remainWidth = 0;       // ʣ�����
	vector<Flow> sendingFlow;  // ���ڷ��͵���
};

bool getFileData(int fileIndex, vector<Port> & ports, vector<deque<Flow>> & _flowsVectorSortT);

unsigned long timeCounter;  // ��ʱ��

vector<Port> portVector;    // �˿� ����

vector<deque<Flow>> flowVectorSortT;  // ���ս����豸ʱ����������flow��

int main(void)
{
	{
		int fileIndex = -1;
		while (true)
		{
			timeCounter = 0;
			portVector.clear();
			flowVectorSortT.clear();
			unsigned int sendFlow2PortSize = 0;  // ȫ����������

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
			int startFlowNum = 0, endFlowNum = 0;  // ��¼ ��ʼ���� �� ������� ����������
			unsigned int firstNotEmpty_i = 0;      // ��¼��һ���� flowVectorSortT ��size��Ϊ 0 ������
			for (; ; timeCounter++)   // ����ʱ��
			{
				// �������豸ʱ���ҵ���һ�� size ��Ϊ 0 �� flow ��
				for (unsigned int i = firstNotEmpty_i; (i <= timeCounter) && (i < flowVectorSortT.size()); i++)
				{
					if (flowVectorSortT[i].empty())
						continue;

					firstNotEmpty_i = i;
					break;
				}
				
				bool insertFlow2Port = true;
				/* �� ��ѡ������ʵ�������˿ڵķ��Ͷ��� */
				while (insertFlow2Port)    // �Ƿ�Ҫ��������˿�
				{
					unsigned int fullPortNum = 0;  // �Ѿ���������Ķ˿� ����
					Flow bestFlow;
					bestFlow.sendNeedTime = 1;
					bestFlow.needWidth = 1;
					for (unsigned int i = firstNotEmpty_i; (i <= timeCounter) && (i < flowVectorSortT.size()); i++)
					{
						deque<Flow>& flowVec = flowVectorSortT[i];
						if (!flowVec.empty() && flowVec[0].sendNeedTime >= bestFlow.sendNeedTime)
						{
							// ��һ������ʵ�flow
							bestFlow = flowVec.front();
							bestFlow.atVecSortT_i = i;
						}
					}

					if (bestFlow.id == -1)
						break;					

					// ���ҿ��ö˿�(���� ����ʵ���)
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
								insertFlow2Port = false;  // ���ж˿ڶ�û�пռ���
						}
					}
				}


				/* �� ������д����϶���ɰ���С�������� */
				// if(�ҵ�ports���ʣ��Ĵ���>=ʱ�������С��������)������� ������<= �ҵ�ǰʱ��������ӽ������С����
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

					if (minWidthFlow_i_atVectSortT >= 0)  // ��ǰʱ������Ƿ�����Ҫ���͵���
					{
						// �ҵ�ports���ʣ��Ĵ�����λ��
						int maxRemainPortWidth = -1, maxRemainPortWidth_i_atPortVector = -1;
						auto maxRemainPortWidthIt = max_element(portVector.begin(), portVector.end(), [](Port a, Port b) {return a.remainWidth < b.remainWidth; });
						maxRemainPortWidth = (*maxRemainPortWidthIt).remainWidth;
						maxRemainPortWidth_i_atPortVector = maxRemainPortWidthIt - portVector.begin();

						if (maxRemainPortWidth >= minWidthFlow.needWidth)  // ���ʣ��Ĵ����Ƿ����ʱ�������С��������
						{
							map<int, Flow> bestInsertFlowMap;

							for (unsigned int i = firstNotEmpty_i; (i <= timeCounter) && (i < flowVectorSortT.size()); i++)
							{
								deque<Flow>& flowVec = flowVectorSortT[i];
								// ���￼���ҳ�����С��maxRemainPortWidth�������������Ǵ�� Map<i, Flow>��
								// �����ҳ�flowVectorSortT[i]��С��maxRemainPortWidth�ĵ�ռ�ô���������
								if (!flowVec.empty())
								{
									Flow tempFlow;  // flowVec��С��maxRemainPortWidth�ĵ�ռ�ô���������
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
							enoughFewWidth = false;  // ports �����Ѿ���ȫ��������
						}
					}
					else
					{
						enoughFewWidth = false;  // ��ǰʱ�����Ѿ�û����Ҫ���͵�����
					}
				}

				/* ��ӡ�˿ڴ���ʹ��Ч�� */
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
				/* �� ÿ���˿ڵķ��Ͷ���״̬���� */
				for (auto& port : portVector)
				{
					vector<Flow> needDelete;    // ���ͽ�����Ҫɾ������
					for (auto& flow : port.sendingFlow)
					{
						if (flow.sendNeedTime == flow.sendRemainTime && flow.isSending == false)
						{
							// �����¼��뷢�Ͷ��е�����������Ϊ����״̬����� result �ļ�
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
							needDelete.push_back(flow);  // ���ͽ�����Ҫɾ������
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
					resultFile.close();  // �ر��ļ�
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
	//�ж��ļ��Ƿ�������
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

	portFile.ignore(255, '\n');  // ������һ��
	while (!portFile.eof())
	{	
		Port aPort;
		char _c = -1;
		portFile >> aPort.id >> _c >> aPort.width;
		aPort.remainWidth = aPort.width;
		if (_c == -1)  // �ļ�ĩβ
			break;
		
		ports.push_back(aPort);
	}
	// �˿ڰ��մ�����������
	sort(ports.begin(), ports.end(), [](Port a, Port b) {return a.remainWidth < b.remainWidth; });

	flowFile.ignore(255, '\n');  // ������һ��
	while (!flowFile.eof())
	{
		Flow aFlow;
		char _c = -1;
		flowFile >> aFlow.id >> _c >> aFlow.needWidth >> _c >> aFlow.enterDevTime >> _c >> aFlow.sendNeedTime;
		aFlow.sendRemainTime = aFlow.sendNeedTime;
		if(_c == -1)  // �ļ�ĩβ
			break;

		if (aFlow.enterDevTime + 1 > _flowsVectorSortT.size())  // ����
			_flowsVectorSortT.resize(aFlow.enterDevTime + 1);
		_flowsVectorSortT[aFlow.enterDevTime].push_back(aFlow);
	}
	
	for (auto & flowVec : flowVectorSortT)
	{
		sort(flowVec.begin(), flowVec.end(),
			[](const Flow& a, const Flow& b)
			{
				// ���շ�����Ҫ��ʱ�併������
				return (a.sendNeedTime > b.sendNeedTime);
			} 
		);
	}

	portFile.close();
	flowFile.close();

	return true;
}

