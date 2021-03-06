// -------------------------------------------------------------------------
//    @FileName			:    NFCWorldNet_ClientModule.cpp
//    @Author           :    LvSheng.Huang
//    @Date             :    2013-01-02
//    @Module           :    NFCWorldNet_ClientModule
//    @Desc             :
// -------------------------------------------------------------------------

#include "NFCWorldToMasterModule.h"
#include "NFWorldNet_ClientPlugin.h"
#include "NFComm/NFCore/NFCDataList.h"
#include "NFComm/NFMessageDefine/NFMsgDefine.h"
#include "NFComm/NFPluginModule/NFINetClientModule.hpp"
#include "NFComm/NFMessageDefine/NFProtocolDefine.hpp"

bool NFCWorldToMasterModule::Init()
{
	m_pNetClientModule = NF_NEW NFINetClientModule(pPluginManager);

	m_pNetClientModule->Init();

	return true;
}

bool NFCWorldToMasterModule::Shut()
{
	return true;
}

bool NFCWorldToMasterModule::AfterInit()
{
	m_pClassModule = pPluginManager->FindModule<NFIClassModule>();
	m_pElementModule = pPluginManager->FindModule<NFIElementModule>();
	m_pLogModule = pPluginManager->FindModule<NFILogModule>();
	m_pWorldNet_ServerModule = pPluginManager->FindModule<NFIWorldNet_ServerModule>();

	m_pNetClientModule->AddReceiveCallBack(NFMsg::EGMI_REQ_CONNECT_WORLD, this, &NFCWorldToMasterModule::OnSelectServerProcess);
	m_pNetClientModule->AddReceiveCallBack(NFMsg::EGMI_REQ_KICK_CLIENT_INWORLD, this, &NFCWorldToMasterModule::OnKickClientProcess);
	m_pNetClientModule->AddReceiveCallBack(this, &NFCWorldToMasterModule::InvalidMessage);

	m_pNetClientModule->AddEventCallBack(this, &NFCWorldToMasterModule::OnSocketMSEvent);

	NF_SHARE_PTR<NFIClass> xLogicClass = m_pClassModule->GetElement(NFrame::Server::ThisName());
	if (xLogicClass)
	{
		NFList<std::string>& strIdList = xLogicClass->GetIdList();
		std::string strId;
		for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
		{
			const int nServerType = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Type());
			const int nServerID = m_pElementModule->GetPropertyInt(strId, NFrame::Server::ServerID());
			if (nServerType == NF_SERVER_TYPES::NF_ST_MASTER)
			{
				const int nPort = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Port());
				const int nMaxConnect = m_pElementModule->GetPropertyInt(strId, NFrame::Server::MaxOnline());
				const int nCpus = m_pElementModule->GetPropertyInt(strId, NFrame::Server::CpuCount());
				const std::string& strName = m_pElementModule->GetPropertyString(strId, NFrame::Server::Name());
				const std::string& strIP = m_pElementModule->GetPropertyString(strId, NFrame::Server::IP());

				ConnectData xServerData;

				xServerData.nGameID = nServerID;
				xServerData.eServerType = (NF_SERVER_TYPES)nServerType;
				xServerData.strIP = strIP;
				xServerData.nPort = nPort;
				xServerData.strName = strName;

				m_pNetClientModule->AddServer(xServerData);
			}
		}
	}

	return true;
}


bool NFCWorldToMasterModule::Execute()
{
	m_pNetClientModule->Execute();
	ServerReport();
	return true;
}

void NFCWorldToMasterModule::Register(NFINet* pNet)
{
	NF_SHARE_PTR<NFIClass> xLogicClass = m_pClassModule->GetElement(NFrame::Server::ThisName());
	if (xLogicClass)
	{
		NFList<std::string>& strIdList = xLogicClass->GetIdList();
		std::string strId;
		for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
		{
			const int nServerType = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Type());
			const int nServerID = m_pElementModule->GetPropertyInt(strId, NFrame::Server::ServerID());
			if (nServerType == NF_SERVER_TYPES::NF_ST_WORLD && pPluginManager->GetAppID() == nServerID)
			{
				const int nPort = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Port());
				const int nMaxConnect = m_pElementModule->GetPropertyInt(strId, NFrame::Server::MaxOnline());
				const int nCpus = m_pElementModule->GetPropertyInt(strId, NFrame::Server::CpuCount());
				const std::string& strName = m_pElementModule->GetPropertyString(strId, NFrame::Server::Name());
				const std::string& strIP = m_pElementModule->GetPropertyString(strId, NFrame::Server::IP());

				NFMsg::ServerInfoReportList xMsg;
				NFMsg::ServerInfoReport* pData = xMsg.add_server_list();

				pData->set_server_id(nServerID);
				pData->set_server_name(strName);
				pData->set_server_cur_count(0);
				pData->set_server_ip(strIP);
				pData->set_server_port(nPort);
				pData->set_server_max_online(nMaxConnect);
				pData->set_server_state(NFMsg::EST_NARMAL);
				pData->set_server_type(nServerType);

				NFMsg::ServerInfoExt pb_ServerInfoExt;
				pData->mutable_server_info_list_ext()->CopyFrom(pb_ServerInfoExt);

				NF_SHARE_PTR<ConnectData> pServerData = m_pNetClientModule->GetServerNetInfo(pNet);
				if (pServerData)
				{
					int nTargetID = pServerData->nGameID;
					m_pNetClientModule->SendToServerByPB(nTargetID, NFMsg::EGameMsgID::EGMI_MTL_WORLD_REGISTERED, xMsg);

					m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(0, pData->server_id()), pData->server_name(), "Register");
				}
			}
		}
	}
}

void NFCWorldToMasterModule::ServerReport()
{
	if (mLastReportTime + 10 > pPluginManager->GetNowTime())
	{
		return;
	}
	mLastReportTime = pPluginManager->GetNowTime();
	std::shared_ptr<NFIClass> xLogicClass = m_pClassModule->GetElement(NFrame::Server::ThisName());
	if (xLogicClass)
	{
		NFList<std::string>& strIdList = xLogicClass->GetIdList();
		std::string strId;
		for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
		{
			const int nServerType = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Type());
			const int nServerID = m_pElementModule->GetPropertyInt(strId, NFrame::Server::ServerID());
			if (pPluginManager->GetAppID() == nServerID)
			{
				const int nPort = m_pElementModule->GetPropertyInt(strId, NFrame::Server::Port());
				const int nMaxConnect = m_pElementModule->GetPropertyInt(strId, NFrame::Server::MaxOnline());
				const std::string& strName = m_pElementModule->GetPropertyString(strId, NFrame::Server::Name());
				const std::string& strIP = m_pElementModule->GetPropertyString(strId, NFrame::Server::IP());

				NFMsg::ServerInfoReport reqMsg;

				reqMsg.set_server_id(nServerID);
				reqMsg.set_server_name(strName);
				reqMsg.set_server_cur_count(0);
				reqMsg.set_server_ip(strIP);
				reqMsg.set_server_port(nPort);
				reqMsg.set_server_max_online(nMaxConnect);
				reqMsg.set_server_state(NFMsg::EST_NARMAL);
				reqMsg.set_server_type(nServerType);

				for (int n = 0;n < 10;n++)
				{
					AddServerInfoExt("key" + lexical_cast<std::string>(n), "value" + lexical_cast<std::string>(n));
				}

				NFMsg::ServerInfoExt pb_ServerInfoExt;
				for (auto it = m_mServerInfoExt.begin(); it != m_mServerInfoExt.end(); it++)
				{
					*pb_ServerInfoExt.add_key() = it->first;
					*pb_ServerInfoExt.add_value() = it->second;
				}
				reqMsg.mutable_server_info_list_ext()->CopyFrom(pb_ServerInfoExt);

				std::shared_ptr<ConnectData> pServerData = m_pNetClientModule->GetServerList().First();
				if (pServerData)
				{
					m_pNetClientModule->SendToServerByPB(pServerData->nGameID, NFMsg::EGMI_STS_SERVER_REPORT, reqMsg);
				}
			}
		}
	}
}

void NFCWorldToMasterModule::RefreshWorldInfo()
{

}

void NFCWorldToMasterModule::OnSelectServerProcess(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	NFGUID nPlayerID;
	NFMsg::ReqConnectWorld xMsg;
	if (!NFINetModule::ReceivePB(nSockIndex, nMsgID, msg, nLen, xMsg, nPlayerID))
	{
		return;
	}

	NF_SHARE_PTR<ServerData> xServerData = m_pWorldNet_ServerModule->GetSuitProxyForEnter();
	if (xServerData)
	{
		NFMsg::AckConnectWorldResult xData;

		xData.set_world_id(xMsg.world_id());
		xData.mutable_sender()->CopyFrom(xMsg.sender());
		xData.set_login_id(xMsg.login_id());
		xData.set_account(xMsg.account());

		xData.set_world_ip(xServerData->pData->server_ip());
		xData.set_world_port(xServerData->pData->server_port());
		xData.set_world_key(xMsg.account());

		m_pWorldNet_ServerModule->GetNetModule()->SendMsgPB(NFMsg::EGMI_ACK_CONNECT_WORLD, xData, xServerData->nFD);

		m_pNetClientModule->SendSuitByPB(xMsg.account(), NFMsg::EGMI_ACK_CONNECT_WORLD, xData);
	}

}

void NFCWorldToMasterModule::OnKickClientProcess(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	NFGUID nPlayerID;
	NFMsg::ReqKickFromWorld xMsg;
	if (!NFINetModule::ReceivePB(nSockIndex, nMsgID, msg, nLen, xMsg, nPlayerID))
	{
		return;
	}

	
	//     NFCDataList var;
	//     var << xMsg.world_id() << xMsg.account();
	//     m_pEventProcessModule->DoEvent(NFGUID(), NFED_ON_KICK_FROM_SERVER, var);
}

void NFCWorldToMasterModule::InvalidMessage(const int nSockIndex, const int nMsgID, const char * msg, const uint32_t nLen)
{
	printf("NFNet || unMsgID=%d\n", nMsgID);
}

void NFCWorldToMasterModule::OnSocketMSEvent(const int nSockIndex, const NF_NET_EVENT eEvent, NFINet* pNet)
{
	if (eEvent & NF_NET_EVENT_EOF)
	{
		m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(0, nSockIndex), "NF_NET_EVENT_EOF", "Connection closed", __FUNCTION__, __LINE__);
	}
	else if (eEvent & NF_NET_EVENT_ERROR)
	{
		m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(0, nSockIndex), "NF_NET_EVENT_ERROR", "Got an error on the connection", __FUNCTION__, __LINE__);
	}
	else if (eEvent & NF_NET_EVENT_TIMEOUT)
	{
		m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(0, nSockIndex), "NF_NET_EVENT_TIMEOUT", "read timeout", __FUNCTION__, __LINE__);
	}
	else  if (eEvent == NF_NET_EVENT_CONNECTED)
	{
		m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(0, nSockIndex), "NF_NET_EVENT_CONNECTED", "connectioned success", __FUNCTION__, __LINE__);
		Register(pNet);
	}
}

void NFCWorldToMasterModule::OnClientDisconnect(const int nAddress)
{

}

void NFCWorldToMasterModule::OnClientConnected(const int nAddress)
{

}

bool NFCWorldToMasterModule::BeforeShut()
{
	return true;
}

void NFCWorldToMasterModule::LogServerInfo(const std::string& strServerInfo)
{
	m_pLogModule->LogNormal(NFILogModule::NLL_INFO_NORMAL, NFGUID(), strServerInfo, "");
}

NFINetClientModule* NFCWorldToMasterModule::GetNetClientModule()
{
	return m_pNetClientModule;
}

void NFCWorldToMasterModule::AddServerInfoExt(const std::string & key, const std::string & value)
{
	m_mServerInfoExt[key] = value;
}
