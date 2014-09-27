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
#include <QClipboard>
#include <QMimeData>
#include <QByteArray>
#include <QSplitter>
#include <QTreeWidget>
#include <QHeaderView>

#include "core.h"
#include "../ren-cxx-basics/type.h"
#include "../ren-cxx-basics/extrastandard.h"

#include <fstream>

using namespace Core;

struct WebViewT : QWebView
{
	CoreT *Core = nullptr;

	std::map<std::string, std::list<std::string>> ActionKeys{
		{"Undo", {"u", "Ctrl+z"}},
		{"Redo", {"Shift+U", "Ctrl+Shift+z", "Ctrl+y", "Ctrl+r"}},
		{"Copy", {"y", "Ctrl+c"}},
		{"Cut", {"Ctrl+x"}},
		//{"Cut", {"x", "Ctrl+x"}},
		{"Paste", {"p", "Ctrl+v"}},
		{"Enter", {"Return"}},
		{"Frame", {"Shift+Return"}},
		{"Unframe", {"Shift+Escape"}},
		//{"Unframe and exit", {"Escape"}},
		{"Exit", {"Escape"}},
		{"Up", {"k", "Up"}},
		{"Down", {"j", "Down"}},
		{"Left", {"h", "Left"}},
		{"Right", {"l", "Right"}},
		{"Delete", {"x", "Delete"}},
		//{"Delete", {"Delete"}},
		{"Backspace", {"Backspace"}},
		{"Finish", {"Space"}},
		{"Insert before", {"i"}},
		{"Insert after", {"a"}},
		{"Replace parent", {"r"}},
		{"Insert statement before", {"Shift+O"}},
		{"Insert statement after", {"o"}},
		{"Finish and insert statement after", {";", "Return"}},
		{"Advance value", {"Enter"}},
	};

	std::list<std::function<bool(QKeyEvent *Event)>> Actions;
	
	void SetCore(CoreT *Core)
	{
		this->Core = Core;

		Core->ResetActionsCallback = [this](void)
		{
			Actions.clear();
		};
		Core->RegisterActionCallback = [this](std::shared_ptr<ActionT> Action)
		{
			//std::cout << "Registering " << Action->Name << std::endl;
			if (Action->Arguments.empty())
			{
				auto FoundSequences = ActionKeys.find(Action->Name);
				if (FoundSequences == ActionKeys.end()) 
				{
					std::cout << "No key found for " << Action->Name << std::endl;
					return;
				}
				for (auto const &SequenceString : FoundSequences->second)
				{
					QKeySequence Sequence(SequenceString.c_str());
					Actions.push_back([this, Action, Sequence](QKeyEvent *Event)
					{
						if (Sequence == QKeySequence(Event->key() | Event->modifiers()))
						{
							this->Core->HandleInput(Action);
							return true;
						}
						return false;
					});
				}
				return;
			}

			if (Action->Arguments.size() == 1)
			{
				auto TextArgument = dynamic_cast<ActionT::TextArgumentT *>(Action->Arguments[0]);
				if (TextArgument)
				{
					Actions.push_back([this, Action, TextArgument](QKeyEvent *Event)
					{
						std::string Text(Event->text().toUtf8().data());
						static std::regex Printable("[[:print:]]+"); // Probably needs testing with utf-8
						// It would be nice if Qt had an "isPrintable" modifier on QKeyEvent
						// TODO Just blacklist all modifier and unprintable characters, maybe
						if (!std::regex_match(Text, Printable)) return false;
						if (TextArgument->Regex && !std::regex_match(Text, *TextArgument->Regex)) 
							return false;
						TextArgument->Data = Text;
						this->Core->HandleInput(Action);
						return true;
					});
					return;
				}
			}

			Assert(false); // TODO, popup dialog or something
		};
		
	}

	void keyPressEvent(QKeyEvent *Event)
	{
		std::cout << "Got key event " << QKeySequence(Event->key() | Event->modifiers()).toString().toUtf8().data() << std::endl;
		for (auto &Handler : Actions)
			if (Handler(Event)) return;
	}

	void mousePressEvent(QMouseEvent *Event)
	{
		auto HTMLRoot = page()->mainFrame()->documentElement(); // QWebElement
		/*auto Result = HTMLRoot.evaluateJavaScript((
				StringT() << "document.elementFromPoint(" << Event->x() << ", " << Event->y() << ").Nucleus;"
			).str().c_str());*/
		auto Result = HTMLRoot.evaluateJavaScript((
				StringT() << "(function() {"
					"var Found = document.elementFromPoint(" << Event->x() << ", " << Event->y() << ");"
					"while (!Found.Nucleus && Found.parentNode) { Found = Found.parentNode; };"
					"if (Found) { return Found.Nucleus; };"
					"})();"
			).str().c_str());
		bool Converted = false;
		NucleusT *Found = reinterpret_cast<NucleusT *>(Result.toLongLong(&Converted));
		if (!Converted) return;
		Core->TextMode = false;
		Core->Focus(Found);
		Core->Refresh();
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
		// Main area + panels
		auto PanelSplitter = new QSplitter(Window);
		WindowLayout->addWidget(PanelSplitter);

		// ----
		// TOC
		struct TOCVisualT : Core::TOCVisualT
		{
			QTreeWidget &TOC;
			bool &NeedsRestructure;

			OptionalT<TOCLocationT> Location;
			NucleusT *Owner;

			static bool Less(TOCVisualT *Left, TOCVisualT *Right) 
			{ 
				Assert(Left->Location);
				Assert(Right->Location);
				if (!Left->Location) return true;
				if (!Right->Location) return false;
				return *Left->Location < *Right->Location; 
			}

			//std::set<TOCVisualT *, decltype(&Less)> &TOCEntries;
			std::map<TOCLocationT, TOCVisualT *> &TOCEntries;
			std::unique_ptr<QTreeWidgetItem> QTItem;

			//TOCVisualT(QTreeWidget &TOC, bool &NeedsRestructure, NucleusT *Owner, std::set<TOCVisualT *, decltype(&Less)> &TOCEntries, std::unique_ptr<QTreeWidgetItem> &&QTItem) : TOC(TOC), NeedsRestructure(NeedsRestructure), Owner(Owner), TOCEntries(TOCEntries), QTItem(std::move(QTItem)) 
			TOCVisualT(QTreeWidget &TOC, bool &NeedsRestructure, NucleusT *Owner, std::map<TOCLocationT, TOCVisualT *> &TOCEntries, std::unique_ptr<QTreeWidgetItem> &&QTItem) : TOC(TOC), NeedsRestructure(NeedsRestructure), Owner(Owner), TOCEntries(TOCEntries), QTItem(std::move(QTItem)) 
			{ 
				this->QTItem->setData(0, Qt::UserRole, QVariant::fromValue((void *)this));
			}

			void SetText(std::string const &NewText) override
			{
				QTItem->setText(0, NewText.c_str());
			}

			void SetLocation(OptionalT<TOCLocationT> &&Location) override
			{
				if (this->Location) TOCEntries.erase(*this->Location);
				this->Location = std::move(Location);
				if (this->Location)
				{
					auto Result = TOCEntries.insert({*this->Location, this});
					Assert(Result.second); // New element inserted, no matching element found
					NeedsRestructure = true;
				}
			}
		};

		//std::set<TOCVisualT *, decltype(&TOCVisualT::Less)> TOCEntries(TOCVisualT::Less); // For destruction after core
		std::map<TOCLocationT, TOCVisualT *> TOCEntries; // For destruction after core
		bool TOCNeedsRestructure = false;
		bool TOCNeedsReselect = false;
		auto TOCPanel = new QTreeWidget(Window);
		TOCPanel->setColumnCount(1);
		TOCPanel->header()->close();
		TOCPanel->setFocusPolicy(Qt::NoFocus);
		TOCPanel->setSelectionMode(QAbstractItemView::SingleSelection);
		//TOCPanel->setSelectionMode(QAbstractItemView::MultiSelection);
		PanelSplitter->addWidget(TOCPanel);

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
		PanelSplitter->addWidget(WebFrame);

		auto HTMLRoot = WebView->page()->mainFrame()->documentElement(); // QWebElement
		// QT bug since 2010: http://www.qtcentre.org/threads/36609-Set-html-lt-style-gt-with-QWebElement
		HTMLRoot.findFirst("head").setInnerXml((
			StringT() << 
				"<style type=\"text/css\">" << 
				std::ifstream("editor.css").rdbuf() << 
				"</style>").str().c_str());
		VisualT BodyVisual(HTMLRoot, HTMLRoot.findFirst("body"));

		CoreT Core(BodyVisual);
		WebView->SetCore(&Core);

		// ----
		// Load config
		bool VisualizeStructure = false;
		{
			Serial::ReadT Read;
			Read.Object([&](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
			{
				Core.Configure(Object);
				Object.Bool("VisualizeStructure", [&](bool Value) -> Serial::ReadErrorT
					{ VisualizeStructure = Value; return {}; });
				return {};
			});
			auto Error = Read.Parse(Filesystem::PathT::Here()->Enter("config.json"));
			if (Error)
			{
				std::cerr << *Error << std::endl;
				return 1;
			}
		}
		{
			auto Error = Core.ConfigureFinished();
			if (Error)
			{
				std::cerr << *Error << std::endl;
				return 1;
			}
		}
		if (VisualizeStructure) BodyVisual.SetClass("flag-visualizestructure");
		else BodyVisual.UnsetClass("flag-visualizestructure");

		// ----
		// Toolbar buttons
		auto TOCButton = new QAction("Table of Contents", Window);
		TOCButton->setCheckable(true);
		Toolbar->addAction(TOCButton);
		Toolbar->addSeparator();
		auto Space1 = new QWidget();
		Space1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		Space1->setVisible(true);
		Toolbar->addWidget(Space1);
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
		
		QObject::connect(TOCButton, &QAction::triggered, [&](bool Checked) 
		{ 
			if (Checked) TOCPanel->show();
			else TOCPanel->hide();
		});
		
		QObject::connect(TOCPanel, &QTreeWidget::currentItemChanged, [&](QTreeWidgetItem *Current, QTreeWidgetItem *Previous)
		{
			if (Current == Previous) return;
			if (!Current) return;
			Core.Focus(static_cast<TOCVisualT *>(Current->data(0, Qt::UserRole).value<void *>())->Owner);
			Core.Refresh();
		});

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
		(void)ConfirmDiscard; // TODO Check on close if changes

		// ----
		// Other core setup
		Core.CopyCallback = [&](std::string &&Text)
		{
			auto Data = std::make_unique<QMimeData>();
			Data->setText(Text.c_str());
			Data->setData("application/json", QByteArray(Text.c_str(), Text.size()));
			QApplication::clipboard()->setMimeData(Data.release());
			//QApplication::clipboard()->setText(Text.c_str());
		};

		Core.PasteCallback = [&](void) -> std::unique_ptr<std::stringstream>
		{
			auto Data = QApplication::clipboard()->mimeData()->data("application/json");
			if (Data.isNull() || Data.isEmpty()) return {};
			// TODO fix gcc + clang to support stream move constructor
			auto Out = std::make_unique<std::stringstream>();
			Out->write(Data.data(), Data.length());
			return std::move(Out);
		};

		Core.CreateTOCVisual = [&](NucleusT *Owner)
		{
			return std::make_unique<TOCVisualT>(*TOCPanel, TOCNeedsRestructure, Owner, TOCEntries, std::make_unique<QTreeWidgetItem>());
		};

		Core.FocusChangedCallback = [&](void)
		{
			TOCNeedsReselect = true;
		};

		Core.HandleInputFinishedCallback = [&](void)
		{
			if (TOCNeedsRestructure)
			{
				std::list<TOCVisualT *> Stack;
				for (auto &Entry : TOCEntries)
				{
					auto Visual = Entry.second;
					TOCVisualT *LastPopped = nullptr;
					while (!Stack.empty() && !Stack.back()->Location->Contains(*Visual->Location))
					{
						LastPopped = Stack.back();
						Stack.pop_back();
					}
					QTreeWidgetItem *Parent = Stack.empty() ? 
						TOCPanel->invisibleRootItem() : Stack.back()->QTItem.get();
					if (Visual->QTItem->parent() != Parent)
					{
						if (Visual->QTItem->parent())
							Visual->QTItem->parent()->takeChild(
								Visual->QTItem->parent()->indexOfChild(Visual->QTItem.get()));
						Parent->insertChild(
							LastPopped ? Parent->indexOfChild(LastPopped->QTItem.get()) + 1 : 0,
							Visual->QTItem.get());
						Parent->setExpanded(true);
						if (Stack.empty()) AssertE(Visual->QTItem->parent(), (QTreeWidgetItem *)0); // gaah, + wtf qt
						else AssertE(Visual->QTItem->parent(), Parent);
					}
					Stack.push_back(Visual);
				}

				TOCNeedsReselect = true;
				TOCNeedsRestructure = false;
			}
			if (TOCNeedsReselect)
			{
				auto Compare = TOCLocationT{*Core.Focused->GetGlobalOrder(), 0};
				TOCPanel->blockSignals(true);
				auto Found = TOCEntries.lower_bound(Compare);
				[&](void)
				{
					if (Found == TOCEntries.end())
					{
						if (Found == TOCEntries.begin()) 
						{
							TOCPanel->selectionModel()->clearSelection();
							return;
						}
						--Found;
					}
					else
					{
						if (Compare < Found->first) 
						{
							if (Found == TOCEntries.begin()) 
							{
								TOCPanel->selectionModel()->clearSelection();
								return;
							}
							--Found;
						}
					}
					TOCPanel->setCurrentItem(Found->second->QTItem.get());
				}();
				TOCNeedsReselect = false;
				TOCPanel->blockSignals(false);
			}
		};

		// ----
		// Go time!
		if (Filename.empty()) Core.Deserialize(WorkingDirectory->Enter("default.kk"));
		else Core.Deserialize(WorkingDirectory->Enter(Filename));
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


