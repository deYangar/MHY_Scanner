﻿#include "ScannerGui.h"

#include <fstream>
#include <filesystem>

#include <QMessageBox>
#include <QWindow>
#include <QRegularExpressionValidator>
#include <QStringList> 
#include <Json.h>

#include "OfficialApi.h"
#include "Mihoyosdk.h"
#include "LoginBili.h"
#include "LoginWindow.h"

ScannerGui::ScannerGui(QWidget* parent)
	: QMainWindow(parent)
	, t1(this)
	, t2(this)
{
	ui.setupUi(this);
	connect(ui.pBtLoginAccount, &QPushButton::clicked, this, &ScannerGui::pBtLoginAccount);
	connect(ui.pBtstartScreen, &QPushButton::clicked, this, &ScannerGui::pBtstartScreen);
	connect(ui.pBtSwitch, &QPushButton::clicked, this, &ScannerGui::pBtSwitch);
	connect(ui.pBtDeleteAccount, &QPushButton::clicked, this, &ScannerGui::pBtDeleteAccount);
	connect(ui.checkBoxAutoScreen, &QCheckBox::stateChanged, this, &ScannerGui::checkBoxAutoScreen);
	connect(ui.checkBoxAutoExit, &QCheckBox::stateChanged, this, &ScannerGui::checkBoxAutoExit);
	connect(ui.pBtStream, &QPushButton::clicked, this, &ScannerGui::pBtStream);
	connect(ui.tableWidget, &QTableWidget::cellClicked, this, &ScannerGui::getInfo);
	connect(&t1, &QRCodeForScreen::loginResults, this, &ScannerGui::islogin);
	connect(&t2, &ThreadStreamProcess::loginResults, this, &ScannerGui::islogin);

	std::string config = loadConfig();
	configJson.parse(config);
	Mihoyosdk m;
	m.setBHVer(configJson["bh_ver"]);
	o.start();

	//FIXME:可能出现不合法的配置文件
	loadUserinfo();
	std::string readName;
	ui.tableWidget->setColumnCount(5);
	QStringList header;
	header << "序号" << "UID" << "用户名" << "类型" << "备注";
	ui.tableWidget->setHorizontalHeaderLabels(header);
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	ui.tableWidget->setColumnWidth(0, 35);
	ui.tableWidget->setColumnWidth(1, 100);
	ui.tableWidget->setColumnWidth(2, 100);
	ui.tableWidget->setColumnWidth(3, 100);
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
	ui.tableWidget->verticalHeader()->setVisible(false);
	ui.tableWidget->horizontalHeader()->setFont(QFont("楷体", 11));
	ui.tableWidget->setAlternatingRowColors(true);

	ui.tableWidget->horizontalHeader()->setStyleSheet(\
		"QHeaderView::section {"\
		"padding: 1px;"\
		"border: none;"\
		"border-bottom: 1px solid rgb(75, 120, 154);"\
		"border-right: 1px solid rgb(75, 120, 154);"\
		"background-color:#e2e6e7;"\
		"color:#333333;"\
		"}"\
	);
	ui.label_3->setText(SCAN_VER);

	ui.tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui.tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	ui.tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);


	for (int i = 0; i < (int)userinfo["num"]; i++)
	{
		insertTableItems(
			QString::fromStdString(userinfo["account"][i]["uid"]),
			QString::fromStdString(userinfo["account"][i]["name"]),
			QString::fromStdString(userinfo["account"][i]["type"]),
			QString::fromStdString("无"));
	}

	ui.lineEditLiveId->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]+$"), this));
	ui.lineEditLiveId->setClearButtonEnabled(true);

	if (configJson["auto_start"] && static_cast<int>(userinfo["last_account"]) != 0)
	{
		countA = static_cast<int>(userinfo["last_account"]) - 1;
		//FIXME 代理会导致响应过慢
		ui.pBtstartScreen->clicked();
		ui.checkBoxAutoScreen->setChecked(true);
		ui.lineEditUname->setText(QString::fromStdString(userinfo["account"][countA]["name"]));
		ui.tableWidget->setCurrentCell(countA, QItemSelectionModel::Select);
	}
	if (configJson["auto_exit"])
	{
		ui.checkBoxAutoExit->setChecked(true);
	}
}

ScannerGui::~ScannerGui()
{
	t1.stop();
	t2.stop();
}

void ScannerGui::insertTableItems(QString uid, QString userName, QString type, QString notes)
{
	QTableWidgetItem* item[5]{};
	int nCount = ui.tableWidget->rowCount();
	ui.tableWidget->insertRow(nCount);
	item[0] = new QTableWidgetItem(QString("%1").arg(nCount + 1));
	ui.tableWidget->setItem(nCount, 0, item[0]);
	item[1] = new QTableWidgetItem(uid);
	ui.tableWidget->setItem(nCount, 1, item[1]);
	item[2] = new QTableWidgetItem(userName);
	ui.tableWidget->setItem(nCount, 2, item[2]);
	item[3] = new QTableWidgetItem(type);
	ui.tableWidget->setItem(nCount, 3, item[3]);
	item[4] = new QTableWidgetItem(notes);
	ui.tableWidget->setItem(nCount, 4, item[4]);
}

void ScannerGui::pBtLoginAccount()
{
	LoginWindow loginwindow;
	ui.pBtLoginAccount->setEnabled(false);
	ui.pBtstartScreen->setEnabled(false);
	ui.pBtStream->setEnabled(false);
	if (t1.isRunning() || t2.isRunning())
	{
		t1.stop();
		t2.stop();
		ui.pBtstartScreen->setText("开始监视屏幕");
		ui.pBtStream->setText("开始监视直播间");
	}
	loginwindow.exec();
	if (loginwindow.getIsReject())
	{
		ui.pBtLoginAccount->setEnabled(true);
		ui.pBtstartScreen->setEnabled(true);
		ui.pBtStream->setEnabled(true);
		return;
	}
	if (loginwindow.type == 1)
	{
		OfficialApi o;
		if (o.cookieParser(loginwindow.cookie) != 0)
		{
			QMessageBox::information(this, "错误", "cookie格式错误", QMessageBox::Yes);
			ui.pBtLoginAccount->setEnabled(true);
			ui.pBtstartScreen->setEnabled(true);
			ui.pBtStream->setEnabled(true);
			return;
		}
		std::string uid = o.getUid();
		if (checkDuplicates(uid))
		{
			QMessageBox::information(this, "提示", "该账号已添加，无需重复添加", QMessageBox::Yes);
			ui.pBtLoginAccount->setEnabled(true);
			ui.pBtstartScreen->setEnabled(true);
			ui.pBtStream->setEnabled(true);
			return;
		}
		std::string token;
		int code = o.getMultiTokenByLoginTicket(token);
		if (code == 0)
		{
			std::string name = o.getUserName(uid);

			int num = userinfo["num"];
			insertTableItems(QString::fromStdString(uid), QString::fromStdString(name), "官服", "无");
			userinfo["account"][num]["access_key"] = token;
			userinfo["account"][num]["uid"] = uid;
			userinfo["account"][num]["name"] = name;
			userinfo["account"][num]["type"] = "官服";
			userinfo["num"] = num + 1;
		}
		else
		{
			QString info = QString::fromStdString(token);
			QMessageBox::information(this, "错误", info, QMessageBox::Yes);
		}
	}
	if (loginwindow.type == 2)
	{
		LoginBili loginbili;
		std::string account;
		std::string pwd;
		std::string message;
		std::string name;
		std::string uid;
		std::string token;
		loginwindow.getAccountPwd(account, pwd);
		int code = loginbili.loginBiliPwd(account, pwd, message, uid, token, name);
		if (checkDuplicates(uid))
		{
			QMessageBox::information(this, "提示", "该账号已添加，无需重复添加", QMessageBox::Yes);
			ui.pBtLoginAccount->setEnabled(true);
			ui.pBtstartScreen->setEnabled(true);
			ui.pBtStream->setEnabled(true);
			return;
		}
		if (code != 0)
		{
			QString Qmessage = QString::fromStdString(message);
			QMessageBox::information(this, "提示", Qmessage, QMessageBox::Yes);
		}
		else
		{
			int num = userinfo["num"];
			insertTableItems(QString::fromStdString(uid), QString::fromStdString(name), "崩坏3B服", "无");
			userinfo["account"][num]["access_key"] = token;
			userinfo["account"][num]["uid"] = uid;
			userinfo["account"][num]["name"] = name;
			userinfo["account"][num]["type"] = "崩坏3B服";
			userinfo["num"] = num + 1;
		}
	}
	updateUserinfo(userinfo.str());
	ui.pBtLoginAccount->setEnabled(true);
	ui.pBtstartScreen->setEnabled(true);
	ui.pBtStream->setEnabled(true);
}

void ScannerGui::pBtstartScreen()
{
	if (countA == -1)
	{
		QMessageBox::information(this, "提示", "没有选择任何账号", QMessageBox::Yes);
		return;
	}
	if (t1.isRunning())
	{
		t1.stop();
		ui.pBtstartScreen->setText("开始监视屏幕");
		return;
	}
	if (t2.isRunning())
	{
		t2.stop();
		ui.pBtStream->setText("开始监视直播间");
	}
	std::string type = userinfo["account"][countA]["type"];
	if (type == "官服")
	{
		OfficialApi o;
		std::string stoken = userinfo["account"][countA]["access_key"];
		std::string uid = userinfo["account"][countA]["uid"];
		std::string gameToken;
		//可用性检查
		int code = o.getGameToken(stoken, uid, gameToken);
		if (code != 0)
		{
			failure();
			return;
		}
		t1.setServerType(ServerType::Type::Official);
		t1.setLoginInfo(uid, gameToken);
	}
	else if (type == "崩坏3B服")
	{
		LoginBili b;
		std::string stoken = userinfo["account"][countA]["access_key"];
		std::string uid = userinfo["account"][countA]["uid"];
		std::string name;
		//可用性检查
		int code = b.loginBiliKey(name, uid, stoken);
		if (code != 0)
		{
			failure();
			return;
		}
		t1.setServerType(ServerType::Type::BH3_BiliBili);
		t1.setLoginInfo(uid, stoken, name);
	}
	else
	{
		return;
	}
	t1.start();
	ui.pBtstartScreen->setText("监视屏幕二维码中");
}

void ScannerGui::pBtStream()
{
	std::string stream_link;
	std::map<std::string, std::string> heards;
	if (countA == -1)
	{
		QMessageBox::information(this, "提示", "没有选择任何账号", QMessageBox::Yes);
		return;
	}
	if (t1.isRunning())
	{
		t1.stop();
		ui.pBtstartScreen->setText("开始监视屏幕");
	}
	if (t2.isRunning())
	{
		t2.stop();
		ui.pBtStream->setText("开始监视直播间");
		return;
	}
	//检查直播间状态
	if (!getStreamLink(ui.lineEditLiveId->text().toStdString(), stream_link, heards))
	{
		return;
	}
	else
	{
		t2.setUrl(stream_link, heards);
	}
	const std::string& type = userinfo["account"][countA]["type"];
	if (type == "官服")
	{
		OfficialApi o;
		std::string stoken = userinfo["account"][countA]["access_key"];
		std::string uid = userinfo["account"][countA]["uid"];
		std::string gameToken;
		//可用性检查
		int code = o.getGameToken(stoken, uid, gameToken);
		if (code != 0)
		{
			failure();
			return;
		}
		t2.setServerType(ServerType::Type::Official);
		t2.setLoginInfo(uid, gameToken);
	}
	else if (type == "崩坏3B服")
	{
		LoginBili b;
		std::string stoken = userinfo["account"][countA]["access_key"];
		std::string uid = userinfo["account"][countA]["uid"];
		std::string name;
		//可用性检查
		int code = b.loginBiliKey(name, uid, stoken);
		if (code != 0)
		{
			failure();
			return;
		}
		t2.setServerType(ServerType::Type::BH3_BiliBili);
		t2.setLoginInfo(uid, stoken, name);
	}
	else
	{
		return;
	}
	t2.start(QThread::Priority::TimeCriticalPriority);
	ui.pBtStream->setText("监视直播二维码中");
	return;
}

void ScannerGui::closeEvent(QCloseEvent* event)
{
	t1.stop();
	t2.stop();
}

void ScannerGui::showEvent(QShowEvent* event)
{

}

void ScannerGui::islogin(const ScanRet::Type ret)
{
	QMessageBox* messageBox = new QMessageBox(this);
	auto Show_QMessageBox = [&](const QString& title, const QString& text)
		{
			ui.pBtStream->setText("开始监视直播间");
			ui.pBtstartScreen->setText("开始监视屏幕");
			messageBox->setIcon(QMessageBox::Information);
			messageBox->setWindowTitle(title);
			messageBox->setText(text);
			messageBox->addButton(QMessageBox::Yes);
			messageBox->show();
		};
	switch (ret)
	{
	case ScanRet::UNKNOW:
		break;
	case ScanRet::FAILURE_1:
		Show_QMessageBox("提示", "扫码失败!");
		break;
	case ScanRet::FAILURE_2:
		Show_QMessageBox("提示", "扫码二次确认失败!");
		break;
	case ScanRet::LIVESTOP:
		Show_QMessageBox("提示", "直播中断!");
		break;
	case ScanRet::SUCCESS:
	{
		Show_QMessageBox("提示", "扫码成功!");
		if (configJson["auto_exit"])
		{
			exit(0);
		}
		break;
	}
	case ScanRet::STREAMERROR:
		Show_QMessageBox("提示", "直播流初始化失败!");
		break;
	default:
		break;
	}
}

void ScannerGui::checkBoxAutoScreen(int state)
{
	if (state == Qt::Checked)
	{
		configJson["auto_start"] = true;
		updateConfig();
	}
	else if (state == Qt::Unchecked)
	{
		configJson["auto_start"] = false;
		updateConfig();
	}
}

void ScannerGui::checkBoxAutoExit(int state)
{
	if (state == Qt::Checked)
	{
		configJson["auto_exit"] = true;
		updateConfig();
	}
	else if (state == Qt::Unchecked)
	{
		configJson["auto_exit"] = false;
		updateConfig();
	}
}

void ScannerGui::updateConfig()//先放这里
{
	const std::string output = configJson.str();
	std::ofstream outFile("./Config/config.json");
	std::stringstream outStr;
	bool isInPair = false;
	for (int i = 0; i < output.size(); i++)
	{
		if (output[i] == '{')
		{
			outStr << "{\n";
			continue;
		}
		if (output[i] == '}')
		{
			outStr << "\n}";
			isInPair = false;
			continue;
		}
		if (output[i] == ',')
		{
			outStr << ",\n";
			isInPair = false;
			continue;
		}
		if (!isInPair)
		{
			outStr << "  ";
			isInPair = true;
		}
		outStr << output[i];
	}
	outFile << outStr.str();
	outFile.close();
}

void ScannerGui::updateUserinfo(const std::string& str)
{
	std::ofstream outputFile("./Config/userinfo.json");
	if (outputFile.is_open())
	{
		outputFile << str;
		outputFile.close();
		std::cout << "字符串已成功写入文件" << std::endl;
	}
	else
	{
		std::cout << "无法打开文件" << std::endl;
	}
}

int ScannerGui::liveIdError(const LiveStreamStatus::Status& data)
{
	switch (data)
	{
	case LiveStreamStatus::Absent:
	{
		QMessageBox::information(this, "提示", "直播间不存在!", QMessageBox::Yes);
		return 1;
	}
	case LiveStreamStatus::NotLive:
	{
		QMessageBox::information(this, "提示", "直播间未开播！", QMessageBox::Yes);
		return 1;
	}
	case LiveStreamStatus::Error:
	{
		QMessageBox::information(this, "提示", "直播间未知错误!", QMessageBox::Yes);
		return 1;
	}
	default:
		return 0;
	}
	return 0;
}

void ScannerGui::loadUserinfo()
{
	const std::string filePath = "./Config/userinfo.json";
	if (std::filesystem::exists(filePath))
	{
		std::string configContent = readConfigFile(filePath);
		//if config file error？
		userinfo.parse(configContent);
	}
	else
	{
		// default
		std::string defaultConfig = R"({"account":[],"last_account":0,"num":0})";
		createDefaultConfigFile(filePath, defaultConfig);
		userinfo.parse(defaultConfig);
	}
}

int ScannerGui::getSelectedRowIndex()
{
	QList<QTableWidgetItem*> item = ui.tableWidget->selectedItems();
	if (item.count() == 0)
	{
		return -1;
	}
	return ui.tableWidget->row(item.at(0));
}

bool ScannerGui::checkDuplicates(const std::string uid)
{
	for (int i = 0; i < (int)userinfo["num"]; i++)
	{
		std::string m_uid = userinfo["account"][i]["uid"];
		if (uid == m_uid)
		{
			return true;
		}
	}
	return false;
}

bool ScannerGui::getStreamLink(const std::string& roomid, std::string& url, std::map<std::string, std::string>& heards)
{
	std::uint32_t live_type = ui.comboBox->currentIndex();
	switch (live_type)
	{
	case 0:
	{
		LiveBili live(roomid);
		if (liveIdError(live.GetLiveStreamStatus()) != 0)
		{
			return false;
		}
		heards =
		{
			{"user_agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) \
			Chrome/110.0.0.0 Safari/537.36 Edg/110.0.1587.41"},
			{"referer", "https://live.bilibili.com"}
		};
		url = live.GetLiveStreamLink();
		break;

	}
	case 1:
	{
		LiveDouyin live(roomid);
		if (liveIdError(live.GetLiveStreamStatus()) != 0)
		{
			return false;
		}
		url = live.GetLiveStreamLink();
		break;
	}
	case 2:
	{
		LiveHuya live(roomid);
		if (liveIdError(live.GetLiveStreamStatus()) != 0)
		{
			return false;
		}
		url = live.GetLiveStreamLink();
		break;
	}
	default:
		break;
	}
	return true;
}

std::string ScannerGui::loadConfig()
{
	const std::string filePath = "./Config/config.json";
	if (std::filesystem::exists(filePath))
	{
		std::string configContent = readConfigFile(filePath);
		return configContent;
	}
	else
	{
		std::string defaultConfig =
			R"({
	"auto_exit": false,
	"auto_start": false,
	"bh_ver":"6.8.0"
})";
		createDefaultConfigFile(filePath, defaultConfig);
		return defaultConfig;
	}

}

std::string ScannerGui::readConfigFile(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (file.is_open())
	{
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();
		std::cout << "读取配置文件成功。\n";
		return content;
	}
	else
	{
		std::cout << "无法打开配置文件。\n";
		return "";
	}
}

void ScannerGui::createDefaultConfigFile(const std::string& filePath, std::string defaultConfig)
{
	std::filesystem::path directory = std::filesystem::path(filePath).parent_path();
	if (!std::filesystem::exists(directory))
	{
		if (!std::filesystem::create_directories(directory))
		{
			std::cout << "无法创建配置文件夹。\n";
			QMessageBox::information(this, "致命错误！", "无法创建配置文件夹！", QMessageBox::Yes);
			exit(0);
		}
	}
	std::ofstream file(filePath);
	if (file.is_open())
	{
		file << defaultConfig;
		file.close();
		std::cout << "创建并写入默认配置文件成功。\n";
	}
	else
	{
		std::cout << "无法创建配置文件。\n";
	}
}

void ScannerGui::failure()
{
	QMessageBox* messageBox = new QMessageBox(this);
	messageBox->setAttribute(Qt::WA_DeleteOnClose);
	messageBox->setText("登录状态失效，\n请重新添加账号！");
	messageBox->setWindowTitle("提示");
	messageBox->setIcon(QMessageBox::Information);
	messageBox->show();
}

void ScannerGui::getInfo(int x, int y)
{
	QTableWidgetItem* item = ui.tableWidget->item(x, 2);
	QString cellText = item->text();
	ui.lineEditUname->setText(cellText);
	countA = x;
#ifdef _DEBUG
	std::cout << std::format(R"(row = {} , user_name = {})", x, cellText.toStdString()) << std::endl;
#endif // _DEBUG
}

void ScannerGui::pBtSwitch()
{
	int nCurrentRow = getSelectedRowIndex();
	if (nCurrentRow != -1)
	{
		//ui.tableWidget->setCurrentCell(nCurrentRow, QItemSelectionModel::Current);
		userinfo["last_account"] = nCurrentRow + 1;
		updateUserinfo(userinfo.str());
		QMessageBox::information(this, "设置成功！", "勾选下方\"启动时自动监视屏幕\"将在下次启动时自动扫描并使用该账号登录", QMessageBox::Yes);
		return;
	}
	else
	{
		QMessageBox::information(this, "提示", "没有选择任何账号", QMessageBox::Yes);
		return;
	}
}

void ScannerGui::pBtDeleteAccount()
{
	int nCurrentRow = getSelectedRowIndex();
	if (nCurrentRow == -1)
	{
		QMessageBox::information(this, "提示", "没有选择任何账号", QMessageBox::Yes);
		return;
	}
	userinfo["num"] = (int)userinfo["num"] - 1;
	//判断删除的账号是否是启动默认账号
	if (static_cast<int>(userinfo["last_account"]) == countA + 1)
	{
		userinfo["last_account"] = 0;
	}
	userinfo["account"].remove(countA);
	const std::string& str = userinfo.str();
#ifdef _DEBUG
	std::cout << str << std::endl;
#endif // _DEBUG
	updateUserinfo(str);
	ui.tableWidget->setCurrentCell(nCurrentRow, QItemSelectionModel::Current);
	ui.tableWidget->removeRow(nCurrentRow);
	for (int i = 0; i < (int)userinfo["num"]; i++)
	{
		QTableWidgetItem* item = new QTableWidgetItem(QString("%1").arg(i + 1));
		ui.tableWidget->setItem(i, 0, item);
	}
}

void OnlineUpdate::run()
{
	Mihoyosdk m;
	m.setOAServer();
}

OnlineUpdate::~OnlineUpdate()
{
	wait();
}
