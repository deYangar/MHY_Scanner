﻿#pragma once

#include <initializer_list>

#include <QThread>
#include <QMutex>

#include "Common.h"
#include "ThreadSacn.h"

class QRCodeForScreen
	: public QThread
{
	Q_OBJECT
public:
	QRCodeForScreen(QObject* parent);
	~QRCodeForScreen();
	void setLoginInfo(const std::string& uid, const std::string& token);
	void setLoginInfo(const std::string& uid, const std::string& token, const std::string& name);
	void setServerType(const ServerType::Type& servertype);
	void run();
	void stop();
signals:
	void loginResults(const ScanRet::Type ret);
private:
	void LoginOfficial();
	void LoginBH3BiliBili();
	bool m_stop = false;
	QMutex m_mux;
	std::string uid;
	std::string gameToken;
	std::string m_name;
	ServerType::Type servertype = ServerType::Type::UNKNOW;
	ScanRet::Type ret = ScanRet::Type::UNKNOW;
};