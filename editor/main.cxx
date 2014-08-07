#include <QApplication>
#include <QWindow>
#include <QBoxLayout>
#include <QWebView>
#include <QKeyEvent>
#include <QWebFrame>
#include <QWebElement>

#include "core.h"

using namespace Core;

struct WebViewT : QWebView
{
	CoreT *Core;
	void keyPressEvent(QKeyEvent *Event)
	{
		InputT::MainT Main;
		OptionalT<std::string> Text;
		std::string RawText = std::string(Event->text().toUtf8().data());
		if (!RawText.empty() && (RawText.size() == 1) && (RawText[0] >= 33) && (RawText[0] <= 126)) Text = RawText;
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
	auto WebView = new WebViewT();
	auto WebPage = new WebPageT();
	WebView->setPage(WebPage);
	WindowLayout->addWidget(WebView);
	Window->setLayout(WindowLayout);

	auto HTMLRoot = WebView->page()->mainFrame()->documentElement(); // QWebElement
	// QT bug since 2010: http://www.qtcentre.org/threads/36609-Set-html-lt-style-gt-with-QWebElement
	HTMLRoot.findFirst("head").setInnerXml(
		"<style type=\"text/css\">"
			"* { margin: 0; padding: 0; }"

			"body { font-size: 16px; }"

			"div"
			"{ "
			"	text-align: left;"
			"	display: inline; "
			"	font-family: monospace;"
			"	"
			"	border: 1px solid LightSteelBlue;"
			"	margin: 2px;"
			"	padding: 2px;"
			"	display: inline-block;"
			"}"

			"div.flag-focused"
			"{ "
			"	border-color: black;"
			"}"
			
			".tag"
			"{"
			"	display: block;"
			"	font-size: 0.6em;"
			"	border: none;"
			"}"

			".group > div"
			"{"
			"	display: block;"
			"}"
		"</style>"
	);
	VisualT BodyVisual(HTMLRoot, HTMLRoot.findFirst("body"));

	CoreT Core(BodyVisual);
	WebView->Core = &Core;

	Window->show();
	return QTContext.exec();

	/*VisualT BodyVisual;
	CoreT Core(BodyVisual);

	std::string Line;
	while (std::getline(std::cin, Line))
	{
		auto Text = Line.substr(0, 1);
		if (Text == "") Core.HandleInput({InputT::MainT::NewStatement});
		else Core.HandleInput({Line.substr(0, 1)});
	}

	return 0;*/
}


