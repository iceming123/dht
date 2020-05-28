#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <string.h>

using namespace std;

#ifdef __windows_include__
#include <winsock2.h>
#include <windows.h>
#define  _PTHREAD_FUNC_  unsigned int  __stdcall
#else

#define  _PTHREAD_FUNC_  void *
typedef void *PVOID;

#define SOCKET int

#endif
#include "../core/dht_demoapi.h"

//to 改进为protobuf解析数据

/* 2.回调函数，DHT网络中,实现接收UDP报文后的处理过程
	解析出msgtype，功能处理
	由node启动时初始化DHT网络时设置
*/
//extern void SaveLatestBlockNode(char *newblockip,int blocktimestamp);

std::string print_buf(const unsigned char *buf, int size)
{
	std::string str;
	char tmp[16] = { 0 };
	for (int i = 0; i < size; i++)
	{
		sprintf(tmp, "%02x", buf[i]);
		str += tmp; memset(tmp, 0, sizeof(tmp));
	}
	return str;
}


unsigned short ReadUint16(unsigned char* buf)
{
	unsigned short data = (buf[0] << 8) | buf[1];
	return data;
}


int ReadInt32(unsigned char *buf)
{
	int data=0;
	((unsigned char*)(&data))[0] = buf[0];
	((unsigned char*)(&data))[1] = buf[1];
	((unsigned char*)(&data))[2] = buf[2];
	((unsigned char*)(&data))[3] = buf[3];
	return data;
}

void HandleNornTransactionReq(char* pbuffer, int len) {
//	transaction_req transation_req;
//    //CHECK_PB_PARSE_MSG(transfer_req.ParseFromArray(pdu->GetBodyData(), pdu->GetBodyLength()));
//    //char m_buffer[1024] = {0};
////    char *pbufbody = pbuffer + sizeof(MsgHeader);
//    transation_req.ParseFromArray(pbuffer,len);
//
//    //transation_req.set_founder(transation_req.founder());
//	Transaction curTransaction;
//	curTransaction.founder = transation_req.founder();
//	curTransaction.counter = transation_req.counter();
//	curTransaction.amount = transation_req.value();
//	std::cout<<"founder=="<<curTransaction.founder<<std::endl;
//	std::cout<<"counter=="<<curTransaction.counter<<std::endl;
//	std::cout<<"amount=="<<curTransaction.amount<<std::endl;
//	///////////////////////////////////////////////////////
//	int err = 123;
//	transaction_res res;
//	res.set_error_code(err);
//	char sarrow[1024]={0};
//	res.SerializeToArray(sarrow,1024);
//	//std::string sbuf = res.SerializeAsString();
//	//std::string ss = "MSGc";
//	//ss += sbuf;
//	int send_len = res.ByteSizeLong();
//	int ret = norn_send_message((unsigned char*)sarrow,send_len,"127.0.0.1",10000);
//	std::cout<<"send transaction msg back,len="<<send_len<<std::endl;
//	std::string sbuf = res.SerializeAsString();
//	std::cout<<"code="
//	<<print_buf((const unsigned char*)sbuf.c_str(),sbuf.length())
//	<<std::endl;
//  	//保存本地临时存储；建民do :
//	//void SaveTransaction(const Transaction &transaction);
//	//SaveTransaction(curTransaction);
//	Transact( curTransaction.founder, curTransaction.counter, curTransaction.amount );
//	//for testin ---need :g_blk.SaveTransaction(curtransaction);
	
}
void handle_norn_account_info(char *buf,int len)
{
	//account_req req;
	//req.ParseFromArray(buf,len);
	//std::cout<<"recv account msg:";
	//std::cout<<"account="<<req.account()<<std::endl;
	//int err = 1;
	//double balance1 = 1000;
	//account_res res;
	//res.set_error_code(err);
	//res.set_balance(balance1);
	//char sarrow[1024]={0};
	//res.SerializeToArray(sarrow,1024);
	////std::string sbuf = res.SerializeAsString();
	////std::string ss = "MSGc";
	////ss += sbuf;
	//int send_len = res.ByteSizeLong();
	//int ret = norn_send_message((unsigned char*)sarrow,send_len,"127.0.0.1",10000);
	//std::cout<<"send account msg back,len="<<send_len<<std::endl;
	//std::string sbuf = res.SerializeAsString();
	//std::cout<<"code="
	//<<print_buf((const unsigned char*)sbuf.c_str(),sbuf.length())
	//<<std::endl;
}

void HandleBroadCastNewBlock(char* pbuffer, int len)
{
 //   char szbuffer[1024] = {0};
	//char nodeip[16] = {0};
	//long blocktimestamp = 0;
	//		//GetLatestBlockNode
	//NornNewBlockReq curMsgReqbody;
 //   memset(&curMsgReqbody,0,sizeof(curMsgReqbody));
 //   memcpy(&curMsgReqbody,(NornNewBlockReq*)pbuffer,sizeof(curMsgReqbody));
 //       //g_newblocknode
 //       //本机ip作为xinblock地址，进行发送：
 //   strcpy(nodeip,curMsgReqbody.block_ip);
 //   blocktimestamp = curMsgReqbody.block_generate_time;
 //   sprintf(szbuffer,"HandleBroadCastNewBlock():,recv msg's ip is:%s,timestamp is :%d",curMsgReqbody.block_ip,curMsgReqbody.block_generate_time);
 //   //log info :
	//std::cout<< szbuffer <<std::endl;
	//std::cout<< "save info iva SaveLatestBlockNode()!" <<std::endl;
	////sgj need add for dpos:
	////SaveLatestBlockNode(nodeip,blocktimestamp);
	////解析包；节点判断是否当前为写块节点：
	////若是，打包数据block，广播整块数据(建民)
	////node.handleTransactionData(blockid);
	////void ClearAllTransactions(void);
}

void recv_msg_handle(const void *data, size_t data_len, char* from)
{
	//MsgHeader *curMsgHeader = (MsgHeader*)data;
	//unsigned char *pszRcvMsg = (unsigned char*)data;
	////Norn_Hander m_norn_header;
	//int length = ReadInt32(pszRcvMsg);
	////m_norn_header.length
	//int version = ReadInt32(pszRcvMsg + 4);
	////m_norn_header.version
	//int command_id = ReadInt32(pszRcvMsg + 8);
	//std::cout<<"len="<<length<<"version="<<version<<"comid="
	//	     <<command_id<<"str=["
	//		 <<print_buf((const unsigned char*)data,data_len)
	//		 <<"]"<<std::endl;
	////MsgHeader *curMsgHeader = (MsgHeader*)data;
	//switch (command_id) {

	//        case CID_BLOCK_NEW_GENERATED:
	//        //产生新块ID的消息，由broadcast而来
	//        //发送方：norn_send_message
 //          		
	//			HandleBroadCastNewBlock((char*)pszRcvMsg+12, length);
	//            break;
	//        //case 1028:
	//		case CID_BLOCK_TRANSATION_REQ:
	//            //保存当前一笔交易记录：
	//           // Transaction curtransaction;
	//           // memcpy(curtransaction,data+sizeof(MsgHeader));
	//           HandleNornTransactionReq((char*)pszRcvMsg+12, length);
	//            break;
	//			
	//		case CID_BLOCK_ACCOUNT_QUERY:
	//			handle_norn_account_info((char*)pszRcvMsg+12, length);
	//			break;
	//        case CID_BLOCK_SYNC_BLOCKDATA:
	//            //1）解析收到的交易块data，调用建民接口打包同步；
	//            //g_blk.writeblockdata()
	//            
	//            //2）或由建民方案负责分布节点的数据同步
	//            break;
	//	    default:
	//	    	//cout << "CBlockChain::HandleBlock, wrong cmd id: %d ", curMsgHeader->command_id);
	//	        std::cout << "CBlockChain::HandleBlock, wrong cmd id " << std::endl;
	//   	        std::cout << curMsgHeader->command_id  << std::endl;
	//		    break;
	//	}
}
