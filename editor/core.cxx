#include "core.h"
#include "protoatom.h"
#include "logging.h"
#include "../shared/extrastandard.h"

#include <regex>
#include <fstream>

namespace Core
{
	
PathElementT::~PathElementT(void) 
{ 
	if (Parent) 
	{
		Parent->Count -= 1;
		if (Parent->Count == 0) delete Parent;
	}
}
	
std::ostream &PathElementT::StandardStream(std::ostream &Stream)
{
	if (Parent) { Parent->StandardStream(Stream); }
	Assert(*this);
	Stream << "[/";
	if (Is<PathTypesT::FieldT>()) Stream << Get<PathTypesT::FieldT>();
	else if (Is<PathTypesT::IndexT>()) Stream << Get<PathTypesT::IndexT>();
	Stream << "]";
	return Stream;
}
	
PathT::PathT(void) : PathT(nullptr) {}

PathT::PathT(PathT const &Other) : PathT(Other.Element) {}

PathT::PathT(PathT &&Other) : PathT(Other.Element) { Other.Element = nullptr; }

PathT::PathT(PathElementT *Element) : Element(Element) { if (Element) Element->Count += 1; }

PathT::~PathT(void)
{
	if (Element)
	{
		Element->Count -= 1;
		if (Element->Count == 0) delete Element;
	}
}

PathT &PathT::operator =(PathT const &Other) { return operator =(Other.Element); }

PathT &PathT::operator =(PathT &&Other)
{
	operator =(Other.Element);
	Other.Element = nullptr;
	return *this;
}

PathT &PathT::operator =(PathElementT *Element)
{
	this->Element = Element;
	if (Element) { Element->Count += 1; }
	return *this;
}

PathT PathT::Field(std::string const &Name) { return new PathElementT(FieldT(Name), Element); }

PathT PathT::Index(size_t Value) { return new PathElementT(IndexT(Value), Element); }
	
PathT::operator bool(void) const { return Element; }

PathElementT *PathT::operator ->(void) const { return Element; }
	
/*void VisualT::Start(void) {}
void VisualT::Add(VisualT &Other) {}
void VisualT::Add(std::string const &Text) {}
void VisualT::Set(std::string const &Text) {}
std::string VisualT::Dump(void) { return ""; }*/

uint64_t VisualIDCounter = 0;

void EvaluateJS(QWebElement Root, std::string const &Text)
{
	std::cout << "Evaluating js: " << Text << std::endl;
	Root.evaluateJavaScript((Text + " null;").c_str());
}

std::regex JSSlashRegex("\\\\|'");
std::regex JSSlashRegex2("\n");
std::string JSSlash(std::string const &Text)
{
	return std::regex_replace(std::regex_replace(Text, JSSlashRegex, "\\$0"), JSSlashRegex2, "\\n");
}

VisualT::VisualT(QWebElement const &Root) : Root(Root), ID(::StringT() << "e" << VisualIDCounter++) 
{
	EvaluateJS(Root, ::StringT()
		<< ID << " = document.createElement('div');");
	EvaluateJS(Root, ::StringT()
		<< ID << ".classList.add('new');");
	EvaluateJS(Root, ::StringT()
		<< "setTimeout(function() { " << ID << ".classList.remove('new'); }, 1);");
}

VisualT::VisualT(QWebElement const &Root, QWebElement const &Element) : VisualT(Root)
{
	auto ElementB = Element; ElementB.setAttribute("id", ID.c_str());
	EvaluateJS(Root, ::StringT()
		<< ID << " = document.getElementById('" << ID << "');");
}

VisualT::~VisualT(void)
{
	EvaluateJS(Root, ::StringT()
		<< "delete window." << ID << ";");
}

void VisualT::SetFocusTarget(NucleusT const *Nucleus)
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".Nucleus = " << reinterpret_cast<intptr_t>(Nucleus) << ";");
}
	
void VisualT::SetClass(std::string const &Class)
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".classList.add('" << JSSlash(Class) << "');");
}

void VisualT::UnsetClass(std::string const &Class)
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".classList.remove('" << JSSlash(Class) << "');");
}
	
VisualT &VisualT::Tag(void)
{
	if (!TagVisual)
	{
		make_unique(TagVisual, Root);
		TagVisual->SetClass("tag");
	}
	return *TagVisual;
}

void VisualT::Start(void) 
{ 
	EvaluateJS(Root, ::StringT()
		<< "while (" << ID << ".lastChild) { " << ID << ".removeChild(" << ID << ".lastChild); }");
	if (TagVisual) Add(*TagVisual);
}

void VisualT::Add(VisualT &Other) 
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".appendChild(" << Other.ID << ");");
}

void VisualT::Add(std::string const &Text) 
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".appendChild(document.createTextNode('" << JSSlash(Text) << "'));");
}
	
void VisualT::Set(std::string const &Text)
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".innerText = '" << JSSlash(Text) << "';");
}

std::string VisualT::Dump(void)
{
	return Root.toOuterXml().toUtf8().data();
}

void VisualT::Scroll(void)
{
	EvaluateJS(Root, ::StringT() << "(function() {"
		"var ElementPosition = " << ID << ".getBoundingClientRect();"
		"ElementTop = ElementPosition.top;"
		"ElementBottom = ElementPosition.bottom;"
		"ElementBottom -= 10;" // Qt/Webkit issues
		//"console.log('etop ' + ElementTop + ', ebottom ' + ElementBottom + ', wheight ' + window.innerHeight);"
		"if (ElementTop < 0)"
		"{" 
			<< ID << ".scrollIntoView(true);"
			"window.scrollBy(0, -10);"
		"}"
		"else if (ElementBottom > window.innerHeight)"
		"{" 
			"if (ElementBottom - ElementTop >= window.innerHeight)"
				"{" << ID << ".scrollIntoView(true); }"
			"else"
			"{" 
				<< ID << ".scrollIntoView(false);"
				"window.scrollBy(0, 30);" // Qt/Webkit issues
				"console.log('hi');"
			"}"
		"}"
		"})();");
}

void VisualT::ScrollToTop(void)
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".scrollIntoView(true);");
}

ReactionT::~ReactionT(void) {}
	
bool ReactionT::Combine(std::unique_ptr<ReactionT> &Other) { return false; }

ActionT::ActionT(std::string const &Name) : Name(Name) {}

ActionT::ArgumentT::~ArgumentT(void) {}

ActionT::~ActionT(void) {}
	
FunctionActionT::FunctionActionT(std::string const &Name, std::function<void(std::unique_ptr<UndoLevelT> &Level)> const &Function) : ActionT(Name), Function(Function) {}

void FunctionActionT::Apply(std::unique_ptr<UndoLevelT> &Level) { Function(Level); }

AtomT::AtomT(CoreT &Core) : Core(Core), Parent(nullptr), Nucleus(nullptr) {}

AtomT::~AtomT(void) 
{
	Clear();
}

NucleusT *AtomT::operator ->(void) 
{ 
	return Nucleus; 
}

NucleusT const *AtomT::operator ->(void) const
{ 
	return Nucleus; 
}

AtomT::operator bool(void) const { return Nucleus; }

void AtomT::Set(std::unique_ptr<UndoLevelT> &Level, NucleusT *Replacement)
{
	TRACE;

	struct SetT : ReactionT
	{
		AtomT &Base;
		HoldT Replacement;

		SetT(AtomT &Base, NucleusT *Replacement) : Base(Base), Replacement(Base.Core, Replacement) { }
		
		void Apply(std::unique_ptr<UndoLevelT> &Level) { Base.Set(Level, Replacement.Nucleus); }
	};
	Level->Add(make_unique<SetT>(*this, Nucleus));
	if (Callback) Callback(Replacement);
	Clear();
	Assert(Core.UndoingOrRedoing || Replacement);
	if (!Replacement) 
	{
		//std::cout << "Set result: " << Core.Dump() << std::endl;
		return;
	}
	Nucleus = Replacement;
	//std::cout << "Set " << this << " to " << Replacement << std::endl;
	Replacement->Count += 1;
	if (Replacement->Atom)
	{
		//std::cout << "Relocating atom " << Replacement << std::endl;
		if (Replacement->Atom->Callback) Replacement->Atom->Callback(nullptr);
		Level->Add(make_unique<SetT>(*Replacement->Atom, Replacement));
		Replacement->Atom->Clear();
	}
	Assert(!Replacement->Atom);
	Replacement->Atom = this;
	Replacement->Parent = Parent;
	//std::cout << "Set result: " << Core.Dump() << std::endl;
}

void AtomT::Clear(void)
{
	if (!Nucleus) return;
	AssertE(Nucleus->Atom, this);
	Nucleus->Parent = nullptr;
	Nucleus->Atom = nullptr;
	Nucleus->Count -= 1;
	if (Nucleus->Count == 0) Core.DeletionCandidates.insert(Nucleus);
	Nucleus = nullptr;
	//std::cout << "Cleared " << this << std::endl;
}
	
HoldT::HoldT(CoreT &Core) : Core(Core), Nucleus(nullptr) {}

HoldT::HoldT(CoreT &Core, NucleusT *Nucleus) : Core(Core), Nucleus(nullptr)
{
	Set(Nucleus);
}

HoldT::HoldT(HoldT const &Other) : Core(Other.Core), Nucleus(nullptr)
{
	Set(Other.Nucleus);
}

HoldT::~HoldT(void) 
{ 
	Clear(); 
}

NucleusT *HoldT::operator ->(void) 
{ 
	return Nucleus; 
}

NucleusT const *HoldT::operator ->(void) const
{ 
	return Nucleus; 
}

HoldT &HoldT::operator =(NucleusT *Nucleus)
{
	Clear();
	Set(Nucleus);
	return *this;
}

HoldT &HoldT::operator =(HoldT &&Hold)
{
	Clear();
	Nucleus = Hold.Nucleus;
	Hold.Nucleus = nullptr;
	return *this;
}

void HoldT::Set(NucleusT *Nucleus)
{
	if (!Nucleus) return;
	this->Nucleus = Nucleus;
	Nucleus->Count += 1;
}

void HoldT::Clear(void)
{
	if (!Nucleus) return;
	Nucleus->Count -= 1;
	if (Nucleus->Count == 0)
		Core.DeletionCandidates.insert(Nucleus);
	Nucleus = nullptr;
}

HoldT::operator bool(void) const { return Nucleus; }

OptionalT<NucleusT *> NucleusT::PartParent(void)
{
	if (!Parent) return {};
	auto Test = Parent.Nucleus;
	while (!Test->template As<CompositeT>()) // Since everything is a Composite now, this is an okay test.  Maybe add a PartParent ref to AtomT?
	{
		if (!Test->Parent) return {};
		Test = Test->Parent.Nucleus;
	}
	return Test;
}

NucleusT::NucleusT(CoreT &Core) : Core(Core), Parent(Core), Visual(Core.RootVisual.Root), Atom(nullptr) 
{
	Visual.SetFocusTarget(this);
	Core.NeedRefresh.insert(this);	
}

NucleusT::~NucleusT(void) {}

void NucleusT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	//Serialize(Serial::WritePolymorphT(GetTypeInfo().Tag, std::move(Prepolymorph)));
	auto Polymorph = Serial::WritePolymorphT(GetTypeInfo().Tag, std::move(Prepolymorph));
	/*Polymorph.String("this", ::StringT() << this);
	Polymorph.String("parent", ::StringT() << Parent.Nucleus);
	Polymorph.String("atom", ::StringT() << Atom);*/
	Serialize(Polymorph);
}
		
void NucleusT::Serialize(Serial::WritePolymorphT &WritePolymorph) const {}

AtomTypeT const &NucleusT::GetTypeInfo(void) const
{
	static AtomTypeT Type;
	return Type;
}

void NucleusT::Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction) 
{
	TRACE;
	if (Parent) Parent->AlignFocus(this);
	Core.Focus(Level, this);
}
	
void NucleusT::AlignFocus(NucleusT *Child)
{
	if (Parent) Parent->AlignFocus(this);
}

void NucleusT::Defocus(std::unique_ptr<UndoLevelT> &Level) {}
		
void NucleusT::AssumeFocus(std::unique_ptr<UndoLevelT> &Level) { }

void NucleusT::Refresh(void) { Assert(false); }

void NucleusT::FocusPrevious(std::unique_ptr<UndoLevelT> &Level) { TRACE; }

void NucleusT::FocusNext(std::unique_ptr<UndoLevelT> &Level) { TRACE; }
	
bool NucleusT::IsFocused(void) const { return Core.Focused.Nucleus == this; }

void NucleusT::FlagRefresh(void) 
{
	Core.NeedRefresh.insert(this);
}
	
void NucleusT::FlagStatusChange(void)
{
	for (auto &Watcher : StatusWatchers)
		Watcher.second(this);
}

void NucleusT::WatchStatus(uintptr_t ID, std::function<void(NucleusT *Changed)> Callback)
{
	if (!Assert(StatusWatchers.find(ID) == StatusWatchers.end())) return;
	StatusWatchers[ID] = Callback;
}

void NucleusT::IgnoreStatus(uintptr_t ID)
{
	auto Found = StatusWatchers.find(ID);
	if (Assert(Found != StatusWatchers.end()))
		StatusWatchers.erase(Found);
}
	
AtomTypeT::~AtomTypeT(void) {}

Serial::ReadErrorT AtomTypeT::Deserialize(Serial::ReadObjectT &Object)
{
	Object.String("Tag", [this](std::string &&Value) -> Serial::ReadErrorT { Tag = std::move(Value); return {}; });
	Object.Bool("ReplaceImmediately", [this](bool Value) -> Serial::ReadErrorT { ReplaceImmediately = Value; return {}; });
	Object.Bool("LeftAssociative", [this](bool Value) -> Serial::ReadErrorT { LeftAssociative = Value; return {}; });
	Object.Int("Precedence", [this](int64_t Value) -> Serial::ReadErrorT { Precedence = Value; return {}; });
	Object.Bool("SpatiallyVertical", [this](bool Value) -> Serial::ReadErrorT { SpatiallyVertical = Value; return {}; });
	return {};
}

void AtomTypeT::Serialize(Serial::WriteObjectT &Object) const
{
	Object.String("Tag", Tag);
	Object.Bool("ReplaceImmediately", ReplaceImmediately);
	Object.Bool("LeftAssociative", LeftAssociative);
	Object.Int("Precedence", Precedence);
	Object.Bool("SpatiallyVertical", SpatiallyVertical);
}

NucleusT *AtomTypeT::Generate(CoreT &Core) { Assert(false); return nullptr; }
	
FocusT::FocusT(CoreT &Core, NucleusT *Nucleus, bool DoNothing) : Core(Core), Target(Core, Nucleus), DoNothing(DoNothing)
{
}

void FocusT::Apply(std::unique_ptr<UndoLevelT> &Level)
{
	Level->Add(make_unique<FocusT>(Core, Target.Nucleus, !DoNothing));
	Target->Focus(Level, FocusDirectionT::Direct);
}

void UndoLevelT::Add(std::unique_ptr<ReactionT> Reaction)
{
	if (!Reactions.empty() && Reactions.back()->Combine(Reaction)) return;
	Reactions.push_back(std::move(Reaction));
}

void UndoLevelT::ApplyUndo(std::unique_ptr<UndoLevelT> &Level)
{
	for (auto Reaction = Reactions.rbegin(); Reaction != Reactions.rend(); ++Reaction)
		(*Reaction)->Apply(Level);
	/*for (auto &Reaction : Reactions)
		Reaction->Apply(Level);*/
}

void UndoLevelT::ApplyRedo(std::unique_ptr<UndoLevelT> &Level)
{
	for (auto Reaction = Reactions.rbegin(); Reaction != Reactions.rend(); ++Reaction)
		(*Reaction)->Apply(Level);
}

bool UndoLevelT::Combine(std::unique_ptr<UndoLevelT> &Other)
{
	auto OtherGroup = dynamic_cast<UndoLevelT *>(Other.get());
	if (!OtherGroup) return false;
	return 
	(
		(Reactions.size() == 1) && 
		(OtherGroup->Reactions.size() == 1) && 
		Reactions.front()->Combine(OtherGroup->Reactions.front())
	);
}

CoreT::CoreT(VisualT &RootVisual) : 
	RootVisual(RootVisual), 
	Root(*this), 
	Focused(*this), 
	TextMode(true), 
	SoloProtoatomType(nullptr), 
	InsertProtoatomType(nullptr), 
	AppendProtoatomType(nullptr), 
	ElementType(nullptr), 
	StringType(nullptr), 
	NeedScroll(false),
	CursorVisual(RootVisual.Root),
	UndoingOrRedoing(false)
{
	CursorVisual.SetClass("type-cursor");
	CursorVisual.Add("|");
	
	Types.emplace("SoloProtoatom", make_unique<SoloProtoatomTypeT>());
	SoloProtoatomType = Types["SoloProtoatom"].get();
	Types.emplace("InsertProtoatom", make_unique<InsertProtoatomTypeT>());
	InsertProtoatomType = Types["InsertProtoatom"].get();
	Types.emplace("AppendProtoatom", make_unique<AppendProtoatomTypeT>());
	AppendProtoatomType = Types["AppendProtoatom"].get();

	{
		Serial::ReadT Read;
		Read.Object([this](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
		{
			Object.Array("Types", [this](Serial::ReadArrayT &Array) -> Serial::ReadErrorT
			{
				Array.Object([this](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
				{
					auto Type = new CompositeTypeT;
					auto Error = Type->Deserialize(Object);
					if (Error) return Error;
					Object.Finally([this, Type](void) -> Serial::ReadErrorT
					{
						if (Types.find(Type->Tag) != Types.end()) 
							return (::StringT() << "Type " << Type->Tag << " defined twice.").str();
						Types.emplace(Type->Tag, std::unique_ptr<AtomTypeT>(Type));
						return {};
					});
					return {};
				});
				return {};
			});
			return {};
		});
		auto Error = Read.Parse(Filesystem::PathT::Here()->Enter("config.json"));
		if (Error) throw ConstructionErrorT() << *Error;
	}

	{
		Serial::WriteT Writer;
		{
			auto WriteRoot = Writer.Object();
			auto Array = WriteRoot.Array("Types");
			for (auto &Type : Types) 
			{
				auto Object = Array.Object();
				Type.second->Serialize(Object);
			}
		}
		std::cout << "config.json:\n" << Writer.Dump() << std::endl;
	}

	{
		auto Found = Types.find("Element");
		if (Found == Types.end()) throw ConstructionErrorT() << "Missing Element type definition.";
		CheckElementType(Found->second.get());
		ElementType = Found->second.get();
	}
	{
		auto Found = Types.find("String");
		if (Found == Types.end()) throw ConstructionErrorT() << "Missing String type definition.";
		CheckStringType(Found->second.get());
		StringType = Found->second.get();
	}

	for (auto &Type : Types)
		if (Type.second->Operator) TypeLookup[*Type.second->Operator] = Type.second.get();
	
	Root.Callback = [this](NucleusT *Nucleus)
	{
		this->RootVisual.Start();
		Assert(Nucleus);
		this->RootVisual.Add(Nucleus->Visual);
	};
	Refresh();
}

CoreT::~CoreT(void)
{
	std::ofstream("dump.kk") << Dump();
}
	
void CoreT::Serialize(Filesystem::PathT const &Path)
{
	if (!Assert(Root)) return; // TODO Error?
	Serial::WriteT Writer;
	{
		auto WriteRoot = Writer.Object();
		Root->Serialize(WriteRoot.Polymorph("Root"));
	}
	Writer.Dump(Path);
}

void CoreT::Deserialize(Filesystem::PathT const &Path)
{
	UndoQueue.clear();
	RedoQueue.clear();
	auto UndoLevel = make_unique<UndoLevelT>();
	AtomT NewRoot(*this);
	{
		Serial::ReadT Read;
		Read.Object([this, &NewRoot, &UndoLevel](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
		{
			//return Deserialize(NewRoot, "Module", Object);
			Object.Polymorph("Root", [this, &NewRoot, &UndoLevel](std::string &&Key, Serial::ReadObjectT &Object)
			{
				return Deserialize(UndoLevel, NewRoot, std::move(Key), Object);
			});
			return {};
		});
		Serial::ReadErrorT ParseError;
		ParseError = Read.Parse(Path);
		if (ParseError) throw ConstructionErrorT() << *ParseError;
	}
	Root.Set(UndoLevel, NewRoot.Nucleus);
	AssumeFocus(UndoLevel);
	Refresh();
	Focused->RegisterActions();
}

bool InHandleInput = false; // DEBUG
void CoreT::HandleInput(std::shared_ptr<ActionT> Action)
{
	TRACE;
	Assert(!InHandleInput);
	InHandleInput = true;

	auto UndoLevel = make_unique<UndoLevelT>();

	std::cout << "Action " << Action->Name << std::endl;
	Action->Apply(UndoLevel);
	
	AssumeFocus(UndoLevel);

	ResetActions();

	if (UndoLevel->Reactions.empty()) UndoLevel.reset(nullptr);
	else
	{
		// Tree modification occurred
		
		if (!UndoQueue.empty())
		{
			auto CombineResult = UndoQueue.front()->Combine(UndoLevel);
			if (CombineResult) UndoLevel.reset(nullptr);
		}
		if (UndoLevel) UndoQueue.push_front(std::move(UndoLevel));
		RedoQueue.clear();
	
		for (auto &Candidate : DeletionCandidates)
			if (Candidate->Count == 0) 
			{
				std::cout << "DEBUG: Deleting " << Candidate << std::endl;
				delete Candidate;
			}
		DeletionCandidates.clear();
	}
	Assert(!UndoLevel);

	Refresh();

	Focused->RegisterActions();

	InHandleInput = false;
}

bool CoreT::HasChanges(void)
{
	return !UndoQueue.empty() || !RedoQueue.empty();
}
	
void CoreT::Focus(NucleusT *Nucleus)
{
	Assert(!InHandleInput);
	auto Action = std::make_shared<FunctionActionT>("Direct select", [this, Nucleus](std::unique_ptr<UndoLevelT> &Level)
	{
		Nucleus->Focus(Level, FocusDirectionT::Direct);
	});
	HandleInput(Action);
}

Serial::ReadErrorT CoreT::Deserialize(std::unique_ptr<UndoLevelT> &Level, AtomT &Out, std::string const &TypeName, Serial::ReadObjectT &Object)
{
	std::unique_ptr<NucleusT> Nucleus;
	auto Type = Types.find(TypeName);
	if (Type == Types.end()) return (StringT() << "Unknown type \'" << TypeName << "\'").str();
	Out.Set(Level, Type->second->Generate(*this));
	return Out->Deserialize(Object);
}

OptionalT<AtomTypeT *> CoreT::LookUpAtomType(std::string const &Text)
{
	auto Type = TypeLookup.find(Text);
	if (Type == TypeLookup.end()) return {};
	return &*Type->second;
}
	
void CoreT::Copy(NucleusT *Tree)
{
	// TODO unify this with Serialize - need stream abstraction
	if (!CopyCallback) return;
	if (!Assert(Root)) return; // TODO Error?
	Serial::WriteT Writer;
	{
		auto WriteRoot = Writer.Object();
		Tree->Serialize(WriteRoot.Polymorph("Root"));
	}
	CopyCallback(Writer.Dump());
}

void CoreT::Paste(std::unique_ptr<UndoLevelT> &UndoLevel, AtomT &Destination)
{
	// TODO unify this with deserialize... somehow. Need stream abstraction
	if (!PasteCallback) return;
	HoldT TempRoot2(*this); // Used to prevent clear reaction when setting destination
	{
		AtomT TempRoot(*this); // Used as buffer to abort if deserialization fails
		Serial::ReadT Read;
		Read.Object([this, &TempRoot, &UndoLevel](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
		{
			Object.Polymorph("Root", [this, &TempRoot, &UndoLevel](std::string &&Key, Serial::ReadObjectT &Object)
			{
				auto DiscardUndo = make_unique<UndoLevelT>();
				return Deserialize(DiscardUndo, TempRoot, std::move(Key), Object);
			});
			return {};
		});
		auto Text = PasteCallback();
		if (Text)
		{
			auto ParseError = Read.Parse(*Text);
			if (ParseError) 
			{
				std::cout << "Error pasting: " << *ParseError << std::endl;
				return;
			}
		}
		Assert(TempRoot.Nucleus);
		TempRoot2.Set(TempRoot.Nucleus);
	}
	Destination.Set(UndoLevel, TempRoot2.Nucleus);
}

void CoreT::Focus(std::unique_ptr<UndoLevelT> &Level, NucleusT *Nucleus)
{
	if (Focused.Nucleus == Nucleus) return;
	if (Focused) Focused->Defocus(Level);
	Focused = Nucleus;
	std::cout << "FOCUSED " << Nucleus << std::endl;
	NeedScroll = true;
}
	
void CoreT::Refresh(void)
{
	for (auto &Refreshable : NeedRefresh)
		Refreshable->Refresh();
	NeedRefresh.clear();
	if (NeedScroll && Focused)
	{
		Focused->Visual.Scroll();
		NeedScroll = false;
	}
}
	
void CoreT::AssumeFocus(std::unique_ptr<UndoLevelT> &Level)
{
	if (Root) 
	{
		std::cout << "Assuming focus..." << std::endl;
		Root->AssumeFocus(Level);
		std::cout << "Assuming focus... DONE" << std::endl;
	}
}
	
void CoreT::ResetActions(void)
{
	TRACE;
	if (ResetActionsCallback) ResetActionsCallback();
	Actions.clear();

	// These are kind of hackish - since NewUndoLevel is shared state
	// and these preempt the HandleInput usage to reverse/advance the stack
	// TODO make the undo level an argument for all actionable function calls?
	RegisterAction(make_unique<FunctionActionT>("Undo", [this](std::unique_ptr<UndoLevelT> &Level) 
	{ 
		UndoingOrRedoing = true;
		if (UndoQueue.empty()) return;
		auto UndoLevel = make_unique<UndoLevelT>();
		UndoQueue.front()->ApplyUndo(UndoLevel);
		RedoQueue.push_front(std::move(UndoLevel));
		UndoQueue.pop_front();
		UndoingOrRedoing = false;
	}));

	RegisterAction(make_unique<FunctionActionT>("Redo", [this](std::unique_ptr<UndoLevelT> &Level) 
	{ 
		UndoingOrRedoing = true;
		if (RedoQueue.empty()) return;
		auto UndoLevel = make_unique<UndoLevelT>();
		RedoQueue.front()->ApplyRedo(UndoLevel);
		UndoQueue.push_front(std::move(UndoLevel));
		RedoQueue.pop_front();
		UndoingOrRedoing = false;
	}));
}

void CoreT::RegisterAction(std::shared_ptr<ActionT> Action)
{
	TRACE;
	Actions.push_back(Action);
	if (RegisterActionCallback) RegisterActionCallback(Action);
}

std::string CoreT::Dump(void) const
{
	if (!Root) return {};
	Serial::WriteT Writer;
	{
		auto WriteRoot = Writer.Object();
		Root->Serialize(WriteRoot.Polymorph("Root"));
	}
	return Writer.Dump();
}
		
}

