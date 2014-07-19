#include "core.h"
#include "../shared/extrastandard.h"
#include "../shared/regex.h"

namespace
{
	uint64_t VisualIDCounter = 0;

	void EvaluateJS(QWebElement Root, std::string const &Text)
	{
		std::cout << "Evaluating js: " << Text << std::endl;
		Root.evaluateJavaScript(Text.c_str());
	}
}

namespace Core
{

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

void VisualT::Clear(void) 
{ 
	EvaluateJS(Root, ::StringT()
		<< "while (" << ID << ".lastChild) { " << ID << ".removeChild(" << ID << ".lastChild); }");
}

void VisualT::Add(VisualT &Other) 
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".appendChild(" << Other.ID << ");");
}
	
std::regex JSSlashRegex("\\|'");
void VisualT::Set(std::string const &Text)
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".innerText = '" << std::regex_replace(Text, JSSlashRegex, "\\$1") << "';");
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

std::unique_ptr<ActionT> ActionGroupT::Apply(void)
{
	auto Out = new ActionGroupT;
	for (auto &Action : Actions)
		Out->AddReverse(Action->Apply());
	return std::unique_ptr<ActionT>(std::move(Out));
}

AtomT::AtomT(void) : Nucleus(nullptr) {}

AtomT::AtomT(AtomT const &Other) : Callback(Other.Callback), Nucleus(nullptr)
{
	Set(Other.Nucleus);
}

AtomT::~AtomT(void) 
{
	Clear();
}

NucleusT *AtomT::operator ->(void) 
{ 
	return Nucleus; 
}

AtomT &AtomT::operator =(NucleusT *Nucleus)
{
	Clear();
	Set(Nucleus);
	return *this;
}

void AtomT::Set(NucleusT *Nucleus)
{
	if (!Nucleus) return;
	this->Nucleus = Nucleus;
	Nucleus->Count += 1;
	Nucleus->Atoms.insert(this);
	if (Callback) Callback(*this);
}

void AtomT::Clear(void)
{
	if (!this->Nucleus) return;
	this->Nucleus->Atoms.erase(this);
	this->Nucleus->Count -= 1;
	if (this->Nucleus->Count == 0)
		delete this->Nucleus;
}
	
AtomT::operator bool(void) { return Nucleus; }

HoldT::HoldT(void) : Nucleus(nullptr) {}

HoldT::HoldT(NucleusT *Nucleus) : Nucleus(nullptr)
{
	Set(Nucleus);
}

HoldT::HoldT(HoldT const &Other) : Nucleus(nullptr)
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

HoldT &HoldT::operator =(NucleusT *Nucleus)
{
	Clear();
	Set(Nucleus);
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
	if (!this->Nucleus) return;
	this->Nucleus->Count -= 1;
	if (this->Nucleus->Count == 0)
		delete this->Nucleus;
}

HoldT::operator bool(void) { return Nucleus; }

NucleusT::NucleusT(CoreT &Core) : Core(Core), Visual(Core.RootVisual.Root) { }

NucleusT::~NucleusT(void) {}

void NucleusT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph)
{
	Serialize(Serial::WritePolymorphT(GetTypeInfo().Tag, std::move(Prepolymorph)));
}
		
void NucleusT::Serialize(Serial::WritePolymorphT &&WritePolymorph) {}

std::unique_ptr<ActionT> NucleusT::Replace(HoldT Other)
{
	struct ReplaceT : ActionT
	{
		HoldT Original, Replacement;
		
		ReplaceT(HoldT const &Original, HoldT const &Replacement) : Original(Original), Replacement(Replacement) { }
		
		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new ReplaceT(Replacement, Original);
			Original->ImmediateReplace(Replacement.Nucleus);
			return std::unique_ptr<ActionT>(std::move(Out));
		}
	};
	return std::unique_ptr<ActionT>(new ReplaceT(HoldT(this), Other));
}

std::unique_ptr<ActionT> NucleusT::Wedge(OptionalT<HoldT> NewBase)
{
	struct WedgeT : ActionT
	{
		bool Wedge; // C++ is so lame
		HoldT Original, Replacement;

		WedgeT(bool Wedge, HoldT const &Original, HoldT const &Replacement) : Wedge(Wedge), Original(Original), Replacement(Replacement) { }
		
		std::unique_ptr<ActionT> Apply(void)
		{
			if (Wedge)
			{
				auto Out = new WedgeT(false, Original, nullptr);
				Original->ImmediateReplace(Replacement.Nucleus);
				Replacement->Place(Original.Nucleus);
				return std::unique_ptr<ActionT>(std::move(Out));
			}
			else
			{
				auto Out = new WedgeT(true, Original, HoldT(Original->Parent.Nucleus));
				Original->Parent->ImmediateReplace(Original.Nucleus);
				return std::unique_ptr<ActionT>(std::move(Out));
			}
		}
	};
	if (NewBase) return std::unique_ptr<ActionT>(new WedgeT(true, HoldT(this), *NewBase));
	else return std::unique_ptr<ActionT>(new WedgeT(false, HoldT(this), nullptr));
}
		
AtomTypeT &NucleusT::GetTypeInfo(void)
{
	static AtomTypeT Type;
	return Type;
}

void NucleusT::Place(NucleusT *Nucleus) {}
		
OptionalT<std::unique_ptr<ActionT>> NucleusT::HandleKey(std::string const &Text) { return {}; }

void NucleusT::ImmediateReplace(NucleusT *Replacement)
{
	auto Parent = this->Parent;
	auto OldAtoms = Atoms;
	for (auto Atom : OldAtoms) *Atom = Replacement;
	assert(Atoms.empty());
	Replacement->Parent = Parent;
}
	
AtomTypeT::~AtomTypeT(void) {}

NucleusT *AtomTypeT::Generate(CoreT &Core) { return nullptr; }

CoreT::CoreT(VisualT &RootVisual) : RootVisual(RootVisual)
{
	Types["."] = &ElementT::StaticGetTypeInfo();
	Types["'"] = &StringT::StaticGetTypeInfo();
	Types["="] = &AssignmentT::StaticGetTypeInfo();
	Types["{"] = &GroupT::StaticGetTypeInfo();
	
	Root.Callback = [this](AtomT &This)
	{
		this->RootVisual.Clear();
		this->RootVisual.Add(This->Visual);
		std::cout << "Added to root visual" << std::endl;
	};
	
	auto Module = new ModuleT(*this);
	auto Group = new GroupT(*this);
	Module->Top = Group;
	Root = Module;
}

void CoreT::HandleKey(std::string const &Text)
{
	assert(Focus);
	std::cout << "Core handle key: [" << Text << "]" << std::endl;
	Apply(Focus->HandleKey(Text));
	Serial::WriteT Writer;
	{
		auto WriteRoot = Writer.Object();
		Root->Serialize(WriteRoot.Polymorph("Root"));
	}
	std::cout << RootVisual.Dump() << std::endl;
	//std::cout << Writer.Dump() << std::endl;
}

OptionalT<AtomTypeT> CoreT::LookUpAtom(std::string const &Text)
{
	auto Type = Types.find(Text);
	if (Type == Types.end()) return {};
	return *Type->second;
}

void CoreT::Apply(OptionalT<std::unique_ptr<ActionT>> Action)
{
	if (!Action) return;
	auto Result = (*Action)->Apply();
	if (!UndoQueue.empty())
	{
		if (!UndoQueue.front()->Combine(Result))
			UndoQueue.push_front(std::move(Result));
	}
	RedoQueue.clear();
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
	
auto IdentifierClass = Regex::ParserT<>("[a-zA-Z0-9_]");

bool CoreT::IsIdentifierClass(std::string const &Reference)
{
	return IdentifierClass(Reference);
}
	
ModuleT::ModuleT(CoreT &Core) : NucleusT(Core)
{
	Top.Callback = [this](AtomT &This)
	{
		this->Visual.Clear();
		this->Visual.Add(This->Visual);
		std::cout << "Added to module visual" << std::endl;
	};
}
	
void ModuleT::Serialize(Serial::WritePolymorphT &&WritePolymorph)
{
	if (Top) Top->Serialize(WritePolymorph.Polymorph("Top"));
}

AtomTypeT &ModuleT::StaticGetTypeInfo(void)
{
	struct TypeT : AtomTypeT
	{
		NucleusT *Generate(CoreT &Core) override { return new ElementT(Core); }

		TypeT(void)
		{
			Tag = "Module";
		}
	} static Type;
	return Type;
}

AtomTypeT &ModuleT::GetTypeInfo(void)
{
	return StaticGetTypeInfo();
}

GroupT::GroupT(CoreT &Core) : NucleusT(Core)
{
	AtomCallback = [this](AtomT &This)
	{
		this->Visual.Clear();
		for (auto &Statement : Statements)
			this->Visual.Add(Statement->Visual);
		std::cout << "Reset group visual" << std::endl;
	};
	Statements.push_back(AtomT());
	Statements.back().Callback = AtomCallback;
	Statements.back() = new ProtoatomT(Core);
}

void GroupT::Serialize(Serial::WritePolymorphT &&WritePolymorph)
{
	auto WriteStatements = WritePolymorph.Array("Statements");
	for (auto &Statement : Statements) Statement->Serialize(WriteStatements.Polymorph());
}

AtomTypeT &GroupT::StaticGetTypeInfo(void)
{
	struct TypeT : AtomTypeT
	{
		NucleusT *Generate(CoreT &Core) override { return new GroupT(Core); }

		TypeT(void)
		{
			Tag = "Group";
			ReplaceImmediately = true;
			Arity = ArityT::Nullary;
			Prefix = true;
			Precedence = 0;
			LeftAssociative = true;
		}
	} static Type;
	return Type;
}

AtomTypeT &GroupT::GetTypeInfo(void)
{
	return StaticGetTypeInfo();
}
	
ProtoatomT::ProtoatomT(CoreT &Core) : NucleusT(Core)
{
	Core.Focus = this;
}

void ProtoatomT::Serialize(Serial::WritePolymorphT &&WritePolymorph)
{
	if (IsIdentifier) WritePolymorph.Bool("IsIdentifier", *IsIdentifier);
	WritePolymorph.String("Data", Data);
	WritePolymorph.UInt("Position", Position);
	if (Lifted) Lifted->Serialize(WritePolymorph.Polymorph("Lifted"));
}
	
AtomTypeT &ProtoatomT::StaticGetTypeInfo(void)
{
	struct TypeT : AtomTypeT
	{
		NucleusT *Generate(CoreT &Core) override { return new ElementT(Core); }

		TypeT(void)
		{
			Tag = "Protoatom";
		}
	} static Type;
	return Type;
}

AtomTypeT &ProtoatomT::GetTypeInfo(void)
{
	return StaticGetTypeInfo();
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::HandleKey(std::string const &Text)
{
	struct SetT : ActionT
	{
		ProtoatomT &Base;
		unsigned int Position;
		std::string Data;
		
		SetT(ProtoatomT &Base, std::string const &Data) : Base(Base), Position(Base.Position), Data(Data) {}
		
		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new SetT(Base, Base.Data);
			Base.Data = Data;
			Base.Position = Position;
			Base.Refresh();
			return std::unique_ptr<ActionT>(Out);
		}
		
		bool Combine(std::unique_ptr<ActionT> &Other) override
		{
			auto Set = dynamic_cast<SetT *>(Other.get());
			if (!Set) return false;
			if (&Set->Base != &Base) return false;
			Position = Set->Position;
			Data = Set->Data;
			return true;
		}
	};
	
	if (Text == " ") return Finish({}, {}, {});
	else if (IsIdentifier && *IsIdentifier != Core.IsIdentifierClass(Text)) return Finish({}, {}, {Text});
	else 
	{
		if (!IsIdentifier) IsIdentifier = Core.IsIdentifierClass(Text);
		auto NewData = Data;
	       	NewData.insert(Position, Text);
		Position += 1;
		if (Position == NewData.size()) // Only do auto conversion if appending text
		{
			auto Found = Core.LookUpAtom(NewData);
			if (Found && Found->ReplaceImmediately) 
				return Finish(Found, NewData, {});
		}
		return std::unique_ptr<ActionT>(new SetT(*this, Data + Text));
	}
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::Finish(
	OptionalT<AtomTypeT> Type, 
	OptionalT<std::string> NewData, 
	OptionalT<std::string> SeedData)
{
	std::string Text = NewData ? *NewData : Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	if (Type)
	{
		if ((Type->Arity == ArityT::Nullary) || Type->Prefix)
		{
			if (Lifted) return {};
			auto Replacement = new ProtoatomT(Core);
			Replacement->Parent = Parent;
			if (SeedData) Replacement->Data = *SeedData;
			auto Finished = Type->Generate(Core);
			Replacement->Place(Finished);
			return Replace(Replacement);
		}
		else
		{
			auto ActionGroup = new ActionGroupT();
			if (Lifted) ActionGroup->Add(Replace(Lifted));
			AtomT Child; Child = Lifted ? Lifted.Nucleus : this;
			auto Parent = this->Parent;
			while (Parent && 
				(
					(Parent->GetTypeInfo().Precedence > Type->Precedence) || 
					(
						(Parent->GetTypeInfo().Precedence == Type->Precedence) &&
						(Parent->GetTypeInfo().LeftAssociative)
					)
				))
			{
				Child = Parent;
				Parent = Parent->Parent;
			}
			auto Finished = Type->Generate(Core);
			if (Child) ActionGroup->Add(Child->Wedge(HoldT(Finished)));
			else ActionGroup->Add(Replace(Finished));
			return std::unique_ptr<ActionT>(ActionGroup);
		}
	}
	else
	{
		if (Lifted) return {};
		if (Parent->As<ElementT>())
		{
			auto String = new StringT(Core);
			String->Data = Text;
			return Replace(String);
		}
		else
		{
			auto String = new StringT(Core);
			String->Data = Text;
			auto Element = new ElementT(Core);
			Element->PlaceKey(String);
			return Replace(Element);
		}
	}
}

void ProtoatomT::Refresh(void)
{
	Visual.Set(Data);
}

AtomTypeT &ElementT::StaticGetTypeInfo(void)
{
	struct TypeT : AtomTypeT
	{
		NucleusT *Generate(CoreT &Core) override { return new ElementT(Core); }

		TypeT(void)
		{
			Tag = "Element";
			ReplaceImmediately = true;
			Arity = ArityT::Binary;
			Prefix = false;
			Precedence = 0;
			LeftAssociative = true;
		}
	} static Type;
	return Type;
}

AtomTypeT &ElementT::GetTypeInfo(void)
{
	return StaticGetTypeInfo();
}
	
void ElementT::PlaceKey(NucleusT *Nucleus) 
{
	// TODO
}

AtomTypeT &StringT::StaticGetTypeInfo(void)
{
	struct TypeT : AtomTypeT
	{
		NucleusT *Generate(CoreT &Core) override { return new StringT(Core); }

		TypeT(void)
		{
			Tag = "String";
			ReplaceImmediately = true;
			Arity = ArityT::Nullary;
			Prefix = true;
			Precedence = 0;
			LeftAssociative = true;
		}
	} static Type;
	return Type;
}
	
void StringT::Serialize(Serial::WritePolymorphT &&Polymorph)
{
	Polymorph.String("Data", Data);
}

AtomTypeT &StringT::GetTypeInfo(void)
{
	return StaticGetTypeInfo();
}

AtomTypeT &AssignmentT::StaticGetTypeInfo(void)
{
	struct TypeT : AtomTypeT
	{
		NucleusT *Generate(CoreT &Core) override { return new AssignmentT(Core); }

		TypeT(void)
		{
			Tag = "Assignment";
			ReplaceImmediately = true;
			Arity = ArityT::Binary;
			Prefix = false;
			Precedence = 100;
			LeftAssociative = true;
		}
	} static Type;
	return Type;
}

AtomTypeT &AssignmentT::GetTypeInfo(void)
{
	return StaticGetTypeInfo();
}

}

