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
		switch (Event->key())
		{
			case Qt::Key_Left: Core->HandleInput({InputT::MainT::Left}); return;
			case Qt::Key_Right: Core->HandleInput({InputT::MainT::Right}); return;
			case Qt::Key_Up: Core->HandleInput({InputT::MainT::Up}); return;
			case Qt::Key_Down: Core->HandleInput({InputT::MainT::Down}); return;
			case Qt::Key_Backspace: Core->HandleInput({InputT::MainT::Backspace}); return;
			case Qt::Key_Delete: Core->HandleInput({InputT::MainT::Delete}); return;
			case Qt::Key_Return: Core->HandleInput({InputT::MainT::NewStatement}); return;
		}
		std::string const Text = Event->text().toUtf8().data();
		if (!Text.empty() && (Text.size() == 1) && (Text[0] >= 33) && (Text[0] <= 126))
		{
			Core->HandleInput({ExplicitT<InputT::TextT>(), Text});
			return;
		}
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
			"	border: 1px solid black;"
			"	margin: 2px;"
			"	padding: 2px;"
			"	display: inline-block;"
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


