#include <QApplication>
#include <QWindow>
#include <QBoxLayout>
#include <QWebView>
#include <QKeyEvent>
#include <QWebFrame>
#include <QWebElement>
#include <QToolBar>
#include <QMenu>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QAction>
#include <QFileDialog>
#include <QCheckBox>
#include <QPushButton>

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
	try 
	{ 
		// ----
		// Command line parsing
		std::string Filename;
		Filesystem::PathT WorkingDirectory;
		if (argc >= 2) 
		{ 
			auto FullPath = Filesystem::PathT::Qualify(argv[1]);
			WorkingDirectory = FullPath->Exit();
			Filename = FullPath->Filename();
		}
		else WorkingDirectory = Filesystem::PathT::Here();

		// ----
		// Window
		QApplication QTContext(argc, argv);
		auto Window = new QWidget();
		Window->setWindowTitle("KK Editor QT");
		auto WindowLayout = new QBoxLayout(QBoxLayout::TopToBottom);
		WindowLayout->setMargin(0);
		WindowLayout->setSpacing(0);
		Window->setLayout(WindowLayout);

		// ----
		// Toolbar
		auto Toolbar = new QToolBar(Window);
		WindowLayout->addWidget(Toolbar);

		// ----
		// Core 
		auto WebPage = new WebPageT();
		auto WebView = new WebViewT();
		WebView->setPage(WebPage);
		auto WebLayout = new QBoxLayout(QBoxLayout::TopToBottom);
		WebLayout->setMargin(0);
		WebLayout->addWidget(WebView);
		auto WebFrame = new QFrame(Window);
		WebFrame->setFrameStyle(QFrame::Box | QFrame::Sunken);
		WebFrame->setLayout(WebLayout);
		WindowLayout->addWidget(WebFrame);

		auto HTMLRoot = WebView->page()->mainFrame()->documentElement(); // QWebElement
		// QT bug since 2010: http://www.qtcentre.org/threads/36609-Set-html-lt-style-gt-with-QWebElement
		HTMLRoot.findFirst("head").setInnerXml((
			StringT() << 
				"<style type=\"text/css\">" << 
				std::ifstream("editor.css").rdbuf() << 
				"</style>").str().c_str());
		VisualT BodyVisual(HTMLRoot, HTMLRoot.findFirst("body"));

		CoreT Core(BodyVisual);
		WebView->Core = &Core;

		// ----
		// Toolbar buttons
		auto NewButton = new QAction("New", Window);
		Toolbar->addAction(NewButton);
		auto OpenButton = new QAction("Open", Window);
		Toolbar->addAction(OpenButton);
		Toolbar->addSeparator();
		auto FilenameEntry = new QLineEdit(Window);
		FilenameEntry->setPlaceholderText("No Filename");
		Toolbar->addWidget(FilenameEntry);
		auto SaveButton = new QAction("Save", Window);
		Toolbar->addAction(SaveButton);
		auto NewNameMenu = new QMenu(Window);
		auto SaveAsButton = new QAction("Save As", Window);
		NewNameMenu->addAction(SaveAsButton);
		auto RenameButton = new QAction("Rename", Window);
		NewNameMenu->addAction(RenameButton);
		auto BackupButton = new QAction("Backup", Window);
		NewNameMenu->addAction(BackupButton);
		
		auto NameFixed = [&](void)
		{
			SaveButton->setMenu(nullptr);
		};
		auto NameChanged = [&](void)
		{
			std::string NewFilename(FilenameEntry->text().toUtf8().data());

			if (NewFilename.empty()) { SaveButton->setDisabled(true); return; }
			else SaveButton->setDisabled(false);

			if (NewFilename == Filename) NameFixed();
			else SaveButton->setMenu(NewNameMenu);
		};
		//NameChanged();
		QObject::connect(FilenameEntry, &QLineEdit::textChanged, [&](const QString &NewText) { NameChanged(); });
		FilenameEntry->setText(Filename.c_str());

		auto Save = [&](bool ChangeName, bool DeleteOld)
		{
			std::string NewFilename(FilenameEntry->text().toUtf8().data());
			auto WritePath = WorkingDirectory->Enter(NewFilename);
			if ((NewFilename != Filename) && WritePath->Exists()) 
				if (QMessageBox::question(
					Window,
					"Confirm Overwrite", 
					(StringT() << WritePath << " already exists.  Are you sure you wish to overwrite this file?").str().c_str()) != QMessageBox::Yes)
					return;
			Core.Serialize(WorkingDirectory->Enter(NewFilename));
			if (DeleteOld) WorkingDirectory->Enter(Filename)->Delete();
			if (ChangeName) Filename = FilenameEntry->text().toUtf8().data();
		};
		QObject::connect(SaveButton, &QAction::triggered, [&](bool) { Save(false, false); });
		QObject::connect(SaveAsButton, &QAction::triggered, [&](bool) { Save(true, false); });
		QObject::connect(RenameButton, &QAction::triggered, [&](bool) { Save(true, true); });
		QObject::connect(BackupButton, &QAction::triggered, [&](bool) { Save(false, true); });

		auto ConfirmDiscard = [&](void)
		{
			if (!Core.HasChanges()) return true;
			return QMessageBox::question
			(
				Window,
				"Discard Changes", 
				"Are you sure you wish to discard all changes to the current document?"
			) == QMessageBox::Yes;
		};
		auto New = [&](void)
		{
			if (!ConfirmDiscard()) return;
			Core.Deserialize(Filesystem::PathT::Here()->Enter("default.kk"));
			Filename.clear();
			FilenameEntry->setText(Filename.c_str());
		};
		QObject::connect(NewButton, &QAction::triggered, [&](bool) 
		{
			New();
		});
		QObject::connect(OpenButton, &QAction::triggered, [&](bool) 
		{
			auto QtFilename = QFileDialog::getOpenFileName(
				Window, 
				"Open file...",
				QDir::homePath(),
				"Source files (*.kk);;All files (*)");
			if (QtFilename.isEmpty()) return;
			if (!ConfirmDiscard()) return;
			Core.Deserialize(Filesystem::PathT::Qualify(QtFilename.toUtf8().data()));
		});

		auto Space1 = new QWidget();
		Space1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		Space1->setVisible(true);
		Toolbar->addWidget(Space1);
		//Toolbar->addSeparator();
		auto ConfigureButton = Toolbar->addAction("Configure");
		bool VisualizeStructure = false;
		QObject::connect(ConfigureButton, &QAction::triggered, [&](bool) 
		{
			auto Dialog = new QDialog(Window);
			Dialog->setWindowTitle("Settings");
			auto ConfigLayout = new QBoxLayout(QBoxLayout::TopToBottom);

			auto VisualizeStructureCheck = new QCheckBox("Visualize Structure", Dialog);
			VisualizeStructureCheck->setChecked(VisualizeStructure);
			ConfigLayout->addWidget(VisualizeStructureCheck);
			
			auto ActionLayout = new QBoxLayout(QBoxLayout::RightToLeft);
			auto OkayButton = new QPushButton("Okay");
			ActionLayout->addWidget(OkayButton);
			OkayButton->setDefault(true);
			auto CloseButton = new QPushButton("Close");
			ActionLayout->addWidget(CloseButton);
			ActionLayout->addStretch();
			ConfigLayout->addLayout(ActionLayout);

			QObject::connect(OkayButton, &QPushButton::clicked, [=, &BodyVisual, &VisualizeStructure](bool)
			{
				VisualizeStructure = VisualizeStructureCheck->isChecked();
				if (VisualizeStructure) BodyVisual.SetClass("flag-visualizestructure");
				else BodyVisual.UnsetClass("flag-visualizestructure");
				Dialog->close();
			});
			QObject::connect(CloseButton, &QPushButton::clicked, [=](bool)
			{
				Dialog->close();
			});

			Dialog->setLayout(ConfigLayout);
			Dialog->show();
		});

		// ----
		// Go time!
		New();
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


