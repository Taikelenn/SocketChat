#include "ClientApplication.h"

#include "../Logger.h"
#include "../Packets/NetPacket.h"
#include "../Packets/Protocol.h"

#include <stdexcept>
#include <cstdio>

void ClientSocketApp::HandlePacket_LoginAck(PKT_S2C_LoginAck* packet)
{
	if (packet->result != LoginResult::Success)
	{
		LogWarning("Server refused to log us in!");
		SendMessageW(ui.g_Window, WM_LOGINERR_TERMINATE, (WPARAM)packet->result, 0);
	}
	else
	{
		this->myUserId = packet->userId;
		LogInfo("Login successful!");
	}
}

void ClientSocketApp::HandlePacket_NewChat(PKT_S2C_NewChat* packet)
{
	int itemIdx = (int)SendMessageW(ui.g_ChatListBox, LB_INSERTSTRING, 0, (LPARAM)this->UI_UTF8ToWideString("*** " + packet->chatName + " ***").c_str());
	SendMessageW(ui.g_ChatListBox, LB_SETITEMDATA, itemIdx, (LPARAM)new uint64_t(packet->chatID));

	if (packet->flashWindow)
	{
		FLASHWINFO pfwi;
		pfwi.cbSize = sizeof(pfwi);
		pfwi.hwnd = ui.g_Window;
		pfwi.dwFlags = FLASHW_TIMERNOFG | FLASHW_ALL;
		pfwi.uCount = 5;
		pfwi.dwTimeout = 0;

		FlashWindowEx(&pfwi);
	}
}

void ClientSocketApp::HandlePacket_ResolveUsernameAns(PKT_S2C_ResolveUsernameAns* packet)
{
	this->usernameToIdMapping[packet->username] = packet->userId;
	if (packet->userId != INVALID_USER_ID)
	{
		this->userIdToNameMapping[packet->userId] = packet->username;
	}

	LogInfo("Resolved %s to %I64u", packet->username.c_str(), packet->userId);
	SetEvent(this->hResolveEvent);
}

void ClientSocketApp::HandlePacket_OpenChatAns(PKT_S2C_OpenChatAns* packet)
{
	PKT_S2C_OpenChatAns* packetCopy = PKT_S2C_OpenChatAns::Deserialize(packet->Serialize().get()).release();
	PostMessageW(ui.g_Window, WM_CHATOPEN, 0, (LPARAM)packetCopy);
}

void ClientSocketApp::HandlePacket_NewMessage(PKT_S2C_NewMessage* packet)
{
	if (packet->chatId != this->GetCurrentChatId())
	{
		this->SetChatUnread(packet->chatId);
		return;
	}

	PKT_S2C_NewMessage* packetCopy = PKT_S2C_NewMessage::Deserialize(packet->Serialize().get()).release();
	PostMessageW(ui.g_Window, WM_CHATADDMSG, 0, (LPARAM)packetCopy);
}

void ClientSocketApp::HandlePacket_ReplaceChatList(PKT_S2C_ReplaceChatList* packet)
{
	// Clear the current chat list
	int chatCount = SendMessageW(ui.g_ChatListBox, LB_GETCOUNT, 0, 0);
	for (int i = 0; i < chatCount; ++i)
	{
		uint64_t* currChatIdPtr = (uint64_t*)SendMessageW(ui.g_ChatListBox, LB_GETITEMDATA, i, 0);
		delete currChatIdPtr;
	}

	SendMessageW(ui.g_ChatListBox, LB_RESETCONTENT, 0, 0);
	this->UI_ClearChatText();
	this->OpenChatRoom(INVALID_CHAT_ID);

	// Fill the chat list anew (order matters!)
	for (size_t i = 0; i < packet->rooms.size(); ++i)
	{
		DatabaseChatRoomInfoLite* chatRoom = &packet->rooms[i];
		std::wstring chatNameWide = this->UI_UTF8ToWideString(chatRoom->chatName);

		if (chatRoom->isUnread)
		{
			chatNameWide = L"*** " + chatNameWide + L" ***";
		}

		int chatIdx = SendMessageW(ui.g_ChatListBox, LB_ADDSTRING, 0, (LPARAM)chatNameWide.c_str());
		SendMessageW(ui.g_ChatListBox, LB_SETITEMDATA, chatIdx, (LPARAM)new uint64_t(chatRoom->chatId));
	}
}

void ClientSocketApp::HandlePacket_ReplaceParticipantList(PKT_S2C_ReplaceParticipantList* packet)
{
	PKT_S2C_ReplaceParticipantList* packetCopy = PKT_S2C_ReplaceParticipantList::Deserialize(packet->Serialize().get()).release();
	PostMessageW(ui.g_Window, WM_REPLACEPARTICIPANTS, 0, (LPARAM)packetCopy);
}

void ClientSocketApp::HandlePacket_MessageBox(PKT_S2C_MessageBox* packet)
{
	MessageBoxW(this->UI_GetTopmostWindow(), this->UI_UTF8ToWideString(packet->message).c_str(), L"Server message", MB_OK | MB_ICONWARNING);

	if (packet->isDisconnection)
	{
		SendMessageW(ui.g_Window, WM_CLOSE, 0, 0);
	}
}

void ClientSocketApp::HandlePacket_StartTransmission(PKT_S2C_StartTransmission* packet)
{
	if (myFilePromises.find(packet->promiseId) == myFilePromises.end())
	{
		return;
	}

	LogInfo("Starting upload of %S", myFilePromises[packet->promiseId].c_str());

	HANDLE hUploadedFile = CreateFileW(myFilePromises[packet->promiseId].c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hUploadedFile == INVALID_HANDLE_VALUE)
	{
		return;
	}

	LARGE_INTEGER fileSize = { 0 };
	GetFileSizeEx(hUploadedFile, &fileSize);

	PKT_C2S_SendFileChunk pkt;
	pkt.fileData = std::vector<uint8_t>();
	pkt.fileData.resize(fileSize.QuadPart);

	DWORD readBytes;
	ReadFile(hUploadedFile, pkt.fileData.data(), fileSize.QuadPart, &readBytes, nullptr);
	CloseHandle(hUploadedFile);

	cApp->SendNetEvent(pkt.Serialize());
}

void ClientSocketApp::HandlePacket_ReceiveFileChunk(PKT_S2C_ReceiveFileChunk* packet)
{
	LogInfo("Received whole file of size %u", packet->fileData.size());

	std::wstring filePath = L"E:\\chat_downloads\\" + this->allFilePromises[this->currentFilePromiseId];
	HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD bytesWritten;
	WriteFile(hFile, packet->fileData.data(), packet->fileData.size(), &bytesWritten, nullptr);
	CloseHandle(hFile);

	MessageBoxW(this->UI_GetTopmostWindow(), (L"File has been saved to:\n" + filePath).c_str(), L"Download completed", MB_OK | MB_ICONINFORMATION);
}

void ClientSocketApp::NotifyNetworkEvent(std::unique_ptr<NetPacket> packet)
{
	if (packet->GetLength() == 0)
	{
		return;
	}

	switch ((PacketHeader)packet->GetData()[0])
	{
	case PacketHeader::S2C_LoginAck:
	{
		std::unique_ptr<PKT_S2C_LoginAck> pkt = PKT_S2C_LoginAck::Deserialize(packet.get());
		this->HandlePacket_LoginAck(pkt.get());

		break;
	}
	case PacketHeader::S2C_NewChat:
	{
		std::unique_ptr<PKT_S2C_NewChat> pkt = PKT_S2C_NewChat::Deserialize(packet.get());
		this->HandlePacket_NewChat(pkt.get());

		break;
	}
	case PacketHeader::S2C_ResolveUsernameAns:
	{
		std::unique_ptr<PKT_S2C_ResolveUsernameAns> pkt = PKT_S2C_ResolveUsernameAns::Deserialize(packet.get());
		this->HandlePacket_ResolveUsernameAns(pkt.get());

		break;
	}
	case PacketHeader::S2C_OpenChatAns:
	{
		std::unique_ptr<PKT_S2C_OpenChatAns> pkt = PKT_S2C_OpenChatAns::Deserialize(packet.get());
		this->HandlePacket_OpenChatAns(pkt.get());

		break;
	}
	case PacketHeader::S2C_NewMessage:
	{
		std::unique_ptr<PKT_S2C_NewMessage> pkt = PKT_S2C_NewMessage::Deserialize(packet.get());
		this->HandlePacket_NewMessage(pkt.get());

		break;
	}
	case PacketHeader::S2C_ReplaceChatList:
	{
		std::unique_ptr<PKT_S2C_ReplaceChatList> pkt = PKT_S2C_ReplaceChatList::Deserialize(packet.get());
		this->HandlePacket_ReplaceChatList(pkt.get());

		break;
	}
	case PacketHeader::S2C_ReplaceParticipantList:
	{
		std::unique_ptr<PKT_S2C_ReplaceParticipantList> pkt = PKT_S2C_ReplaceParticipantList::Deserialize(packet.get());
		this->HandlePacket_ReplaceParticipantList(pkt.get());

		break;
	}
	case PacketHeader::S2C_MessageBox:
	{
		std::unique_ptr<PKT_S2C_MessageBox> pkt = PKT_S2C_MessageBox::Deserialize(packet.get());
		this->HandlePacket_MessageBox(pkt.get());

		break;
	}
	case PacketHeader::S2C_StartTransmission:
	{
		std::unique_ptr<PKT_S2C_StartTransmission> pkt = PKT_S2C_StartTransmission::Deserialize(packet.get());
		this->HandlePacket_StartTransmission(pkt.get());

		break;
	}
	case PacketHeader::S2C_ReceiveFileChunk:
	{
		std::unique_ptr<PKT_S2C_ReceiveFileChunk> pkt = PKT_S2C_ReceiveFileChunk::Deserialize(packet.get());
		this->HandlePacket_ReceiveFileChunk(pkt.get());

		break;
	}
	}
}
