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
	//std::cout << "Evaluating js: " << Text << std::endl;
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

ActionT::~ActionT(void) {}
	
bool ActionT::Combine(std::unique_ptr<ActionT> &Other) { return false; }

void ActionGroupT::Add(std::unique_ptr<ActionT> Action)
{
	Actions.push_back(std::move(Action));
}

void ActionGroupT::AddReverse(std::unique_ptr<ActionT> Action)
{
	Actions.push_front(std::move(Action));
}
	
struct NOPT : ActionT
{
	std::unique_ptr<ActionT> Apply(void) { return std::unique_ptr<ActionT>(new NOPT); }
};

std::unique_ptr<ActionT> ActionGroupT::Apply(void)
{
	auto Out = new ActionGroupT;
	for (auto &Action : Actions)
		Out->AddReverse(Action->Apply());
	return std::unique_ptr<ActionT>(std::move(Out));
}

AtomT::AtomT(AtomT &&Other) : Core(Other.Core), Callback(Other.Callback), Parent(Other.Parent), Nucleus(Other.Nucleus)
{
	if (Nucleus) Nucleus->Atom = this;
	Other.Nucleus = nullptr;
}
	
AtomT &AtomT::operator =(AtomT &&Other)
{
	AssertE(&Core, &Other.Core);
	Callback = Other.Callback;
	Parent = Other.Parent;
	Nucleus = Other.Nucleus;
	Other.Nucleus = nullptr;
	return *this;
}

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

AtomT::SetT::SetT(AtomT &Atom, NucleusT *Replacement) : Atom(Atom), Replacement(Atom.Core, Replacement) { }
		
std::unique_ptr<ActionT> AtomT::SetT::Apply(void)
{
	auto Out = new SetT(Atom, Atom.Nucleus);
	Atom.Set(Replacement.Nucleus);
	return std::unique_ptr<ActionT>(std::move(Out));
}

void AtomT::Set(NucleusT *Nucleus)
{
	TRACE;
	if (Callback) Callback(Nucleus);
	Clear();
	Assert(Nucleus);
	if (!Nucleus) return;
	this->Nucleus = Nucleus;
	Nucleus->Count += 1;
	if (Nucleus->Atom)
	{
		//std::cout << "Relocating atom " << Nucleus << std::endl;
		Nucleus->Atom->Clear();
	}
	Assert(!Nucleus->Atom);
	Nucleus->Atom = this;
	Nucleus->Parent = Parent;
	//std::cout << "Set result: " << Core.Dump() << std::endl;
	Core.AssumeFocus();
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

void NucleusT::Focus(FocusDirectionT Direction) 
{
	if (Core.Focused.Nucleus == this) return;
	if (Core.Focused)
	{
		Core.Focused->Defocus();
	}
	Core.Focused = this;
	std::cout << "FOCUSED " << this << std::endl;
}

void NucleusT::Defocus(void) {}
		
void NucleusT::AssumeFocus(void) { }

void NucleusT::Refresh(void) { Assert(false); }

std::unique_ptr<ActionT> NucleusT::Set(NucleusT *Nucleus) { Assert(false); return {}; }
	
std::unique_ptr<ActionT> NucleusT::Set(std::string const &Text) { Assert(false); return {}; }
		
OptionalT<std::unique_ptr<ActionT>> NucleusT::HandleInput(InputT const &Input) 
{ 
	if (Parent) return Parent->HandleInput(Input); 
	else return {}; 
}
	
void NucleusT::FocusPrevious(void) { TRACE; }

void NucleusT::FocusNext(void) { TRACE; }
	
bool NucleusT::IsEmpty(void) const { TRACE; return false; }
	
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

std::map<ArityT, std::string> ReverseArityLookup = 
{
	{Nullary, "Nullary"},
	{Unary, "Unary"},
	{Binary, "Binary"}
};
std::map<std::string, ArityT> ArityLookup = 
{
	{"Nullary", Nullary},
	{"Unary", Unary},
	{"Binary", Binary}
};

Serial::ReadErrorT AtomTypeT::Deserialize(Serial::ReadObjectT &Object)
{
	Object.String("Tag", [this](std::string &&Value) -> Serial::ReadErrorT { Tag = std::move(Value); return {}; });
	Object.String("LookupSequence", [this](std::string &&Value) -> Serial::ReadErrorT { LookupSequence = std::move(Value); return {}; });
	Object.Bool("ReplaceImmediately", [this](bool Value) -> Serial::ReadErrorT { ReplaceImmediately = Value; return {}; });
	Object.String("Arity", [this](std::string &&Value) -> Serial::ReadErrorT
	{
		auto Found = ArityLookup.find(Value);
		if (Found == ArityLookup.end()) return (::StringT() << "Unknown arity " << Value).str();
		Arity = Found->second;
		return {};
	});
	Object.Bool("Prefix", [this](bool Value) -> Serial::ReadErrorT { Prefix = Value; return {}; });
	Object.Bool("LeftAssociative", [this](bool Value) -> Serial::ReadErrorT { LeftAssociative = Value; return {}; });
	Object.Int("Precedence", [this](int64_t Value) -> Serial::ReadErrorT { Precedence = Value; return {}; });
	Object.Bool("SpatiallyVertical", [this](bool Value) -> Serial::ReadErrorT { SpatiallyVertical = Value; return {}; });
	return {};
}

void AtomTypeT::Serialize(Serial::WriteObjectT &Object) const
{
	Object.String("Tag", Tag);
	if (LookupSequence) Object.String("LookupSequence", *LookupSequence);
	Object.Bool("ReplaceImmediately", ReplaceImmediately);
	{
		auto Found = ReverseArityLookup.find(Arity);
		Assert(Found != ReverseArityLookup.end());
		if (Found != ReverseArityLookup.end()) 
			Object.String("Arity", Found->second);
	}
	Object.Bool("Prefix", Prefix);
	Object.Bool("LeftAssociative", LeftAssociative);
	Object.Int("Precedence", Precedence);
	Object.Bool("SpatiallyVertical", SpatiallyVertical);
}

NucleusT *AtomTypeT::Generate(CoreT &Core) { Assert(false); return nullptr; }
	
FocusT::FocusT(CoreT &Core, NucleusT *Nucleus, bool DoNothing) : Core(Core), Target(Core, Nucleus), DoNothing(DoNothing)
{
}

std::unique_ptr<ActionT> FocusT::Apply(void)
{
	auto Out = new FocusT(Core, Target.Nucleus, !DoNothing);
	Target->Focus(FocusDirectionT::Direct);
	return std::unique_ptr<ActionT>(Out);
}

CoreT::CoreT(VisualT &RootVisual) : RootVisual(RootVisual), Root(*this), Focused(*this), TextMode(true), ProtoatomType(nullptr), ElementType(nullptr), StringType(nullptr), CursorVisual(RootVisual.Root)
{
	CursorVisual.SetClass("type-cursor");
	CursorVisual.Add("|");
	
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
		auto Found = Types.find("Protoatom");
		if (Found == Types.end()) throw ConstructionErrorT() << "Missing Protoatom type definition.";
		ProtoatomType = Found->second.get();
		auto ProtoatomType = Found->second.get();
		auto Composite = dynamic_cast<CompositeTypeT *>(ProtoatomType);
		Assert(Composite);
		if (!AssertE(Composite->Parts.size(), 2)) throw ConstructionErrorT() << "Protoatom must have 2 parts.";
		if (!Assert(dynamic_cast<AtomPartTypeT *>(Composite->Parts[0].get()))) throw ConstructionErrorT() << "Protoatom part 1 must be an atom.";
		if (!Assert(dynamic_cast<AtomPartTypeT *>(Composite->Parts[0].get())->StartEmpty)) throw ConstructionErrorT() << "The Protoatom atom part must be StartEmpty.";
		if (!(dynamic_cast<ProtoatomPartTypeT *>(Composite->Parts[1].get()))) throw ConstructionErrorT() << "Protoatom part 2 must be a \"protoatom\" part.";
	}
	{
		auto Found = Types.find("Element");
		if (Found == Types.end()) throw ConstructionErrorT() << "Missing Element type definition.";
		ElementType = Found->second.get();
		auto ElementType = Found->second.get();
		auto Composite = dynamic_cast<CompositeTypeT *>(ElementType);
		Assert(Composite);
		if (!(
			(Composite->Parts.size() == 2) &&
			(dynamic_cast<AtomPartTypeT *>(Composite->Parts[0].get())) &&
			(dynamic_cast<AtomPartTypeT *>(Composite->Parts[1].get()))
		)) throw ConstructionErrorT() << "Element has unusable definition.";
	}
	{
		auto Found = Types.find("String");
		if (Found == Types.end()) throw ConstructionErrorT() << "Missing String type definition.";
		StringType = Found->second.get();
		auto StringType = Found->second.get();
		auto Composite = dynamic_cast<CompositeTypeT *>(StringType);
		Assert(Composite);
		if (!(
			(Composite->Parts.size() == 1) &&
			(dynamic_cast<StringPartTypeT *>(Composite->Parts[0].get()))
		)) throw ConstructionErrorT() << "String has unusable definition.";
	}

	for (auto &Type : Types)
		if (Type.second->LookupSequence) TypeLookup[*Type.second->LookupSequence] = Type.second.get();
	
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
	return Writer.Dump(Path);
}

void CoreT::Deserialize(Filesystem::PathT const &Path)
{
	UndoQueue.clear();
	RedoQueue.clear();
	AtomT NewRoot(*this);
	{
		Serial::ReadT Read;
		Read.Object([this, &NewRoot](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
		{
			//return Deserialize(NewRoot, "Module", Object);
			Object.Polymorph("Root", [this, &NewRoot](std::string &&Key, Serial::ReadObjectT &Object)
			{
				return Deserialize(NewRoot, std::move(Key), Object);
			});
			return {};
		});
		Serial::ReadErrorT ParseError;
		ParseError = Read.Parse(Path);
		if (ParseError) throw ConstructionErrorT() << *ParseError;
	}
	Root.Set(NewRoot.Nucleus);
	Refresh();
}

Serial::ReadErrorT CoreT::Deserialize(AtomT &Out, std::string const &TypeName, Serial::ReadObjectT &Object)
{
	std::unique_ptr<NucleusT> Nucleus;
	auto Type = Types.find(TypeName);
	if (Type == Types.end()) return (StringT() << "Unknown type \'" << TypeName << "\'").str();
	Out.Set(Type->second->Generate(*this));
	return Out->Deserialize(Object);
}

void CoreT::HandleInput(InputT const &Input)
{
	Assert(Focused);
	std::cout << "Core handle key:\n";
	if (Input.Text)
		std::cout << "\t[" << *Input.Text << "]\n";
	if (Input.Main)
		std::cout << "\tmain [" << (int)*Input.Main << "]\n";
	std::cout << std::flush;
	Apply(Focused->HandleInput(Input));
	Refresh();
}

OptionalT<AtomTypeT *> CoreT::LookUpAtom(std::string const &Text)
{
	auto Type = TypeLookup.find(Text);
	if (Type == TypeLookup.end()) return {};
	return &*Type->second;
}

void CoreT::Apply(OptionalT<std::unique_ptr<ActionT>> Action)
{
	if (!Action) return;

	auto Reaction = (*Action)->Apply();

	if (!UndoQueue.empty())
	{
		if (!UndoQueue.front()->Combine(Reaction))
			UndoQueue.push_front(std::move(Reaction));
	}
	RedoQueue.clear();

	Refresh();

	for (auto &Candidate : DeletionCandidates)
		if (Candidate->Count == 0) 
		{
			std::cout << "DEBUG: Deleting " << Candidate << std::endl;
			delete Candidate;
		}
	DeletionCandidates.clear();
	
	//std::cout << RootVisual.Dump() << std::endl;
	//std::cout << Dump() << std::endl;
}
	
bool CoreT::HasChanges(void)
{
	return !UndoQueue.empty() || !RedoQueue.empty();
}

void CoreT::Undo(void)
{
	if (UndoQueue.empty()) return;
	RedoQueue.push_front(UndoQueue.front()->Apply());
	UndoQueue.pop_front();
}

void CoreT::Redo(void)
{
	if (RedoQueue.empty()) return;
	UndoQueue.push_front(RedoQueue.front()->Apply());
	RedoQueue.pop_front();
}
	
void CoreT::AssumeFocus(void)
{
	if (Root) 
	{
		std::cout << "Assuming focus..." << std::endl;
		Root->AssumeFocus();
		std::cout << "Assuming focus... DONE" << std::endl;
	}
}

std::unique_ptr<ActionT> CoreT::ActionHandleInput(InputT const &Input)
{
	struct HandleInputT : ActionT
	{
		CoreT &Core;
		InputT const Input;

		HandleInputT(CoreT &Core, InputT const &Input) : Core(Core), Input(Input) {}

		std::unique_ptr<ActionT> Apply(void)
		{
			Assert(Core.Focused);
			auto Result = Core.Focused->HandleInput(Input);
			if (Result) return (*Result)->Apply();
			return std::unique_ptr<ActionT>(new NOPT);
		};
	};
	return std::unique_ptr<ActionT>(new HandleInputT(*this, Input));
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
		
void CoreT::Refresh(void)
{
	for (auto &Refreshable : NeedRefresh)
		Refreshable->Refresh();
	NeedRefresh.clear();
}
	
}

