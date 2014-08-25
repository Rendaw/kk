// TODO
// Vertically oriented on group atomlist, not group
// XML
// Load atoms from file, put in map
// Replace parent, insert, insert line before, insert line after
// Cut, copy, paste
// Backspace deletes parent if empty

#include <QApplication>
#include <QWindow>
#include <QBoxLayout>
#include <QWebView>
#include <QKeyEvent>
#include <QWebFrame>
#include <QWebElement>

#include "core.h"
#include "../shared/type.h"
#include "../shared/extrastandard.h"

#include <fstream>

using namespace Core;

struct WebViewT : QWebView
{
	CoreT *Core;
	void keyPressEvent(QKeyEvent *Event)
	{
		OptionalT<InputT::MainT> Main;
		OptionalT<std::string> Text;
		std::string RawText = std::string(Event->text().toUtf8().data());
		if (!RawText.empty() && (RawText.size() == 1) && (RawText[0] >= 33) && (RawText[0] <= 126)) Text = RawText;
		if (RawText == " ") Text = RawText;
		switch (Event->key())
		{
			case Qt::Key_Left: Main = InputT::MainT::Left; break;
			case Qt::Key_Right: Main = InputT::MainT::Right; break;
			case Qt::Key_Up: Main = InputT::MainT::Up; break;
			case Qt::Key_Down: Main = InputT::MainT::Down; break;
			case Qt::Key_Backspace: Main = InputT::MainT::TextBackspace; break;
			case Qt::Key_Delete: Main = InputT::MainT::Delete; break;
			case Qt::Key_Return: Main = InputT::MainT::Enter; Text = std::string("\n"); break;
			case Qt::Key_Escape: Main = InputT::MainT::Exit; break;
			default: break;
		}
		if (Text)
		{
			if (*Text == "x") Main = InputT::MainT::Delete;
			else if (*Text == "o") Main = InputT::MainT::NewStatement;
			else if (*Text == "O") Main = InputT::MainT::NewStatementBefore;
			else if (*Text == "r") Main = InputT::MainT::ReplaceParent;
			else if (*Text == "w") Main = InputT::MainT::Wedge;
			else if (*Text == "h") Main = InputT::MainT::Left;
			else if (*Text == "j") Main = InputT::MainT::Down;
			else if (*Text == "k") Main = InputT::MainT::Up;
			else if (*Text == "l") Main = InputT::MainT::Right;
		}
		Core->HandleInput({Main, Text});
	}
};

struct WebPageT : QWebPage
{
	void javaScriptAlert(QWebFrame * frame, const QString & msg )
		{ std::cout << "[ALERT] " << msg.toUtf8().data() << std::endl; }

	void javaScriptConsoleMessage(const QString &Message, int LineNumber, const QString &SourceID)
		{ std::cout << "[LOG] " << SourceID.toUtf8().data() << ":" << LineNumber << " " << Message.toUtf8().data() << std::endl; }
};

int main(int argc, char **argv)
{
	QApplication QTContext(argc, argv);
	auto Window = new QWidget();
	Window->setWindowTitle("KK Editor QT");
	auto WindowLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	WindowLayout->setMargin(0);
	auto WebView = new WebViewT();
	auto WebPage = new WebPageT();
	WebView->setPage(WebPage);
	WindowLayout->addWidget(WebView);
	Window->setLayout(WindowLayout);

	auto HTMLRoot = WebView->page()->mainFrame()->documentElement(); // QWebElement
	// QT bug since 2010: http://www.qtcentre.org/threads/36609-Set-html-lt-style-gt-with-QWebElement
	HTMLRoot.findFirst("head").setInnerXml((
		StringT() << 
			"<style type=\"text/css\">" << 
			std::ifstream("editor.css").rdbuf() << 
			"</style>").str().c_str());
	VisualT BodyVisual(HTMLRoot, HTMLRoot.findFirst("body"));

	try 
	{ 
		CoreT Core(BodyVisual);
		WebView->Core = &Core;

		Window->show();
		auto Result = QTContext.exec();
		std::ofstream("dump.html") << HTMLRoot.toInnerXml().toUtf8().data();
		return Result;
	}
	catch (ConstructionErrorT &Error)
	{
		std::cerr << "ERROR: " << Error << std::endl;
	}
}


