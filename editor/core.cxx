#include "core.h"
#include "../shared/extrastandard.h"
#include "../shared/regex.h"

namespace
{
	/*uint64_t VisualIDCounter = 0;

	void EvaluateJS(QWebElement Root, std::string const &Text)
	{
		//std::cout << "Evaluating js: " << Text << std::endl;
		Root.evaluateJavaScript((Text + " null;").c_str());
	}*/
}

namespace Core
{
	
void VisualT::Clear(void) {}
void VisualT::Add(VisualT &Other) {}
void VisualT::Add(std::string const &Text) {}
void VisualT::Set(std::string const &Text) {}
std::string VisualT::Dump(void) { return ""; }

/*VisualT::VisualT(QWebElement const &Root) : Root(Root), ID(::StringT() << "e" << VisualIDCounter++) 
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

std::regex JSSlashRegex("\\\\|'");
std::string JSSlash(std::string const &Text)
{
	return std::regex_replace(Text, JSSlashRegex, "\\$0");
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
}*/

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

void AtomT::ImmediateSet(NucleusT *Nucleus)
{
	Clear();
	this->Nucleus = Nucleus;
	this->Nucleus->Count += 1;
	Assert(!this->Nucleus->Atom);
	this->Nucleus->Atom = this;
	this->Nucleus->Parent = Parent;
	if (Callback) Callback(*this);
}

std::unique_ptr<ActionT> AtomT::Set(NucleusT *Nucleus)
{
	struct SetT : ActionT
	{
		AtomT &Atom;
		HoldT Replacement;

		SetT(AtomT &Atom, HoldT const &Replacement) : Atom(Atom), Replacement(Replacement) { }
		
		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new SetT(Atom, {Atom.Core, Atom.Nucleus});
			Atom.ImmediateSet(Replacement.Nucleus);
			Atom.Core.Refocus();
			std::cout << "Set result: " << Atom.Core.Dump() << std::endl;
			return std::unique_ptr<ActionT>(std::move(Out));
		}
	};
	return std::unique_ptr<ActionT>(new SetT(*this, {Core, Nucleus}));
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

NucleusT::NucleusT(CoreT &Core) : Core(Core), Parent(Core), Atom(nullptr)/*, Visual(Core.RootVisual.Root)*/ { }

NucleusT::~NucleusT(void) {}

void NucleusT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	//Serialize(Serial::WritePolymorphT(GetTypeInfo().Tag, std::move(Prepolymorph)));
	auto Polymorph = Serial::WritePolymorphT(GetTypeInfo().Tag, std::move(Prepolymorph));
	Polymorph.String("this", ::StringT() << this);
	Polymorph.String("parent", ::StringT() << Parent.Nucleus);
	Polymorph.String("atom", ::StringT() << Atom);
	Serialize(std::move(Polymorph));
}
		
void NucleusT::Serialize(Serial::WritePolymorphT &&WritePolymorph) const {}

AtomTypeT const &NucleusT::GetTypeInfo(void) const
{
	static AtomTypeT Type;
	return Type;
}
		
void NucleusT::Refocus(void) { }

void NucleusT::Refresh(void) { Assert(false); }

std::unique_ptr<ActionT> NucleusT::Place(NucleusT *Nucleus) { Assert(false); return {}; }
		
OptionalT<std::unique_ptr<ActionT>> NucleusT::HandleInput(InputT const &Input) { return {}; }

AtomTypeT::~AtomTypeT(void) {}

NucleusT *AtomTypeT::Generate(CoreT &Core) { Assert(false); return nullptr; }

CoreT::CoreT(VisualT &RootVisual) : RootVisual(RootVisual), Root(*this), Focus(*this)
{
	Types["."] = &ElementT::StaticGetTypeInfo();
	Types["'"] = &StringT::StaticGetTypeInfo();
	Types["="] = &AssignmentT::StaticGetTypeInfo();
	Types["{"] = &GroupT::StaticGetTypeInfo();
	
	Root.Callback = [this](AtomT &This)
	{
		this->RootVisual.Clear();
		this->RootVisual.Add(This->Visual);
	};
	
	Apply(Root.Set(new ModuleT(*this)));
}

void CoreT::HandleInput(InputT const &Input)
{
	Assert(Focus);
	std::cout << "Core handle key: [" << Input.Text << "]" << std::endl;
	Apply(Focus->HandleInput(Input));
}

OptionalT<AtomTypeT *> CoreT::LookUpAtom(std::string const &Text)
{
	auto Type = Types.find(Text);
	if (Type == Types.end()) return {};
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

	for (auto &Refreshable : NeedRefresh)
		Refreshable->Refresh();
	NeedRefresh.clear();

	for (auto &Candidate : DeletionCandidates)
		if (Candidate->Count == 0) 
		{
			std::cout << "DEBUG: Deleting " << Candidate << std::endl;
			delete Candidate;
		}
	DeletionCandidates.clear();
	
	//std::cout << RootVisual.Dump() << std::endl;
	std::cout << Dump() << std::endl;
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

void CoreT::Refocus(void)
{
	Focus = nullptr;
	Root->Refocus();
}

std::unique_ptr<ActionT> CoreT::ActionHandleInput(InputT const &Input)
{
	struct NOPT : ActionT
	{
		std::unique_ptr<ActionT> Apply(void) { return std::unique_ptr<ActionT>(new NOPT); }
	};
	struct HandleInputT : ActionT
	{
		CoreT &Core;
		InputT const Input;

		HandleInputT(CoreT &Core, InputT const &Input) : Core(Core), Input(Input) {}

		std::unique_ptr<ActionT> Apply(void)
		{
			Assert(Core.Focus);
			auto Result = Core.Focus->HandleInput(Input);
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
	
ModuleT::ModuleT(CoreT &Core) : NucleusT(Core), Top(Core)
{
	Top.Callback = [this](AtomT &This)
	{
		this->Core.NeedRefresh.insert(this);
	};
	Top.Parent = this;
	Top.ImmediateSet(new GroupT(Core));
}
	
void ModuleT::Serialize(Serial::WritePolymorphT &&WritePolymorph) const
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

AtomTypeT const &ModuleT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void ModuleT::Refocus(void)
{
	if (!Top) return;
	Top->Refocus();
}

void ModuleT::Refresh(void)
{
	this->Visual.Clear();
	this->Visual.Add(Top->Visual);
}

GroupT::GroupT(CoreT &Core) : NucleusT(Core), Focus(0)
{
	AtomCallback = [this](AtomT &This)
	{
		this->Core.NeedRefresh.insert(this);
	};
	AddStatement(0);
	Statements[0].ImmediateSet(new ProtoatomT(Core));
}

void GroupT::Serialize(Serial::WritePolymorphT &&WritePolymorph) const
{
	auto WriteStatements = WritePolymorph.Array("Statements");
	for (auto &Statement : Statements) if (Statement) Statement->Serialize(WriteStatements.Polymorph());
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

AtomTypeT const &GroupT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void GroupT::Refocus(void)
{
	if (Focus >= Statements.size()) return;
	Statements[Focus]->Refocus();
}

void GroupT::Refresh(void)
{
	this->Visual.Clear();
	for (auto &Statement : Statements)
		this->Visual.Add(Statement->Visual);
}

void GroupT::AddStatement(size_t Index)
{
	auto Iterator = Statements.begin();
	std::advance(Iterator, Index); // Portrait of a well designed api
	auto Statement = Statements.insert(Iterator, AtomT(Core)); // Emplace tries copy constructor, nice job
	Statement->Parent = this;
	Statement->Callback = [this](AtomT &This)
	{
		Core.NeedRefresh.insert(this);
	};
}

void GroupT::RemoveStatement(size_t Index)
{
	auto Iterator = Statements.begin();
	std::advance(Iterator, Index);
	Statements.erase(Iterator);
}

std::unique_ptr<ActionT> GroupT::ModifyStatements(bool Add, size_t Index)
{
	struct ModifyStatementsT : ActionT
	{
		GroupT &Base;
		bool Add;
		size_t Index;

		ModifyStatementsT(GroupT &Base, bool Add, size_t Index) : Base(Base), Add(Add), Index(Index) {}

		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new ModifyStatementsT(Base, !Add, Index);
			if (Add) Base.AddStatement(Index);
			else Base.RemoveStatement(Index);
			return std::unique_ptr<ActionT>(Out);
		}
	};
	return std::unique_ptr<ActionT>(new ModifyStatementsT(*this, Add, Index));
}
	
std::unique_ptr<ActionT> GroupT::Set(size_t Index, NucleusT *Value)
{
	struct SetT : ActionT
	{
		GroupT &Base;
		size_t Index;
		HoldT Value;

		SetT(GroupT &Base, size_t Index, HoldT const &Value) : Base(Base), Index(Index), Value(Value) {}

		std::unique_ptr<ActionT> Apply(void)
		{
			return Base.Statements[Index].Set(Value.Nucleus)->Apply();
		}
	};
	return std::unique_ptr<ActionT>(new SetT(*this, Index, {Core, Value}));
}
	
ProtoatomT::ProtoatomT(CoreT &Core) : NucleusT(Core), Lifted(Core)
{
}

void ProtoatomT::Serialize(Serial::WritePolymorphT &&WritePolymorph) const
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

AtomTypeT const &ProtoatomT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void ProtoatomT::Refocus(void)
{
	Core.Focus = this;
}

void ProtoatomT::Refresh(void)
{
	Visual.Clear();
	if (Lifted) Visual.Add(Lifted->Visual);
	Visual.Add(Data);
}
	
void ProtoatomT::Lift(NucleusT *Nucleus)
{
	Assert(!Lifted);
	Lifted = Nucleus;
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::HandleInput(InputT const &Input)
{
	struct SetT : ActionT
	{
		ProtoatomT &Base;
		unsigned int Position;
		std::string Data;
		
		SetT(ProtoatomT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}
		
		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new SetT(Base, Base.Position, Base.Data);
			Base.Data = Data;
			Base.Position = Position;
			Base.Core.NeedRefresh.insert(&Base);
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
	
	if (Input.Text == " ") return Finish({}, {}, {});
	else if (IsIdentifier && *IsIdentifier != Core.IsIdentifierClass(Input.Text)) return Finish({}, {}, {Input});
	else 
	{
		if (!IsIdentifier) IsIdentifier = Core.IsIdentifierClass(Input.Text);
		auto NewData = Data;
	       	NewData.insert(Position, Input.Text);
		auto NewPosition = Position + 1;
		if (NewPosition == NewData.size()) // Only do auto conversion if appending text
		{
			auto Found = Core.LookUpAtom(NewData);
			if (Found && Found->ReplaceImmediately) 
				return Finish(Found, NewData, {});
		}
		return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, Data + Input.Text));
	}
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::Finish(
	OptionalT<AtomTypeT *> Type, 
	OptionalT<std::string> NewData, 
	OptionalT<InputT> SeedData)
{
	std::string Text = NewData ? *NewData : Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	auto Actions = new ActionGroupT;
	if (Type)
	{
		Assert(!SeedData);
		Assert(Atom);
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted) 
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			auto Replacement = new ProtoatomT(Core);
			auto Finished = Type->Generate(Core);
			Actions->Add(Replacement->Place(Finished));
			if ((*Type)->Arity != ArityT::Nullary)
			{
				auto Proto = new ProtoatomT(Core);
				Actions->Add(Finished->Place(Proto));
			}
			Actions->Add(Atom->Set(Replacement));
		}
		else
		{
			auto Child = Lifted ? Lifted.Nucleus : new ProtoatomT(Core);
			Actions->Add(Atom->Set(Child));
			AtomT *WedgeAtom = Atom;
			while (WedgeAtom->Parent && 
				(
					(WedgeAtom->Parent->GetTypeInfo().Precedence > (*Type)->Precedence) || 
					(
						(WedgeAtom->Parent->GetTypeInfo().Precedence == (*Type)->Precedence) &&
						(WedgeAtom->Parent->GetTypeInfo().LeftAssociative)
					)
				))
			{
				WedgeAtom = WedgeAtom->Parent->Atom;
				Child = WedgeAtom->Nucleus;
			}
			auto Finished = (*Type)->Generate(Core);
			Assert(Finished);
			Actions->Add(WedgeAtom->Set(Finished));
			Actions->Add(Finished->Place(Child));
		}
	}
	else
	{
		if (Lifted) 
		{ 
			std::cout << "NOTE: Can't finish as new element if protoatom has lifed." << std::endl; 
			return {};
		}
			
		auto String = new StringT(Core);
		Actions->Add(String->Set(Text));

		auto Protoatom = new ProtoatomT(Core);

		if (Parent->As<ElementT>())
		{
			Protoatom->Lift(String);
		}
		else
		{
			auto Element = new ElementT(Core);
			Element->Key.ImmediateSet(String);
			Protoatom->Lifted.Set(Element);
		}

		std::cout << Core.Dump() << std::endl; // DEBUG
		Actions->Add(Atom->Set(Protoatom));

		if (SeedData) Actions->Add(Core.ActionHandleInput(*SeedData));
	}

	return std::unique_ptr<ActionT>(Actions);
}

ElementT::ElementT(CoreT &Core) : NucleusT(Core), Base(Core), Key(Core), BaseFocused(false)
{
	Base.Callback = Key.Callback = [this](AtomT &This)
	{
		this->Core.NeedRefresh.insert(this);
	};
	Base.Parent = Key.Parent = this;
	Base.ImmediateSet(new ProtoatomT(Core));
	Key.ImmediateSet(new ProtoatomT(Core));
}

void ElementT::Serialize(Serial::WritePolymorphT &&Polymorph) const
{
	Base->Serialize(Polymorph.Polymorph("Base"));
	Key->Serialize(Polymorph.Polymorph("Key"));
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
			Precedence = 900;
			LeftAssociative = true;
		}
	} static Type;
	return Type;
}

AtomTypeT const &ElementT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void ElementT::Refocus(void)
{
	if (BaseFocused) { if (Base) Base->Refocus(); }
	else { if (Key) Key->Refocus(); }
}

void ElementT::Refresh(void)
{
	this->Visual.Clear();
	if (Base)
	{
		this->Visual.Add(Base->Visual);
		this->Visual.Add(".");
	}
	if (Key) this->Visual.Add(Key->Visual);
}
	
std::unique_ptr<ActionT> ElementT::Place(NucleusT *Nucleus)
{
	return Base.Set(Nucleus);
}

std::unique_ptr<ActionT> ElementT::PlaceKey(NucleusT *Nucleus)
{
	return Key.Set(Nucleus);
}

StringT::StringT(CoreT &Core) : NucleusT(Core) 
{
	Refresh();
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
		}
	} static Type;
	return Type;
}
	
void StringT::Serialize(Serial::WritePolymorphT &&Polymorph) const
{
	Polymorph.String("Data", Data);
}

AtomTypeT const &StringT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void StringT::Refresh(void)
{
	Visual.Set("'" + Data + "'");
}
	
std::unique_ptr<ActionT> StringT::Set(std::string const &Text)
{
	struct SetT : ActionT
	{
		StringT &Base;
		std::string Text;

		SetT(StringT &Base, std::string const &Text) : Base(Base), Text(Text) {}

		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new SetT(Base, Base.Data);
			Base.Data = Text;
			Base.Core.NeedRefresh.insert(&Base);
			return std::unique_ptr<ActionT>(Out);
		}
	};
	return std::unique_ptr<ActionT>(new SetT(*this, Text));
}

AssignmentT::AssignmentT(CoreT &Core) : NucleusT(Core), Left(Core), Right(Core), LeftFocused(false)
{
	Left.Callback = Right.Callback = [this](AtomT &This)
	{
		this->Core.NeedRefresh.insert(this);
	};
	Left.Parent = Right.Parent = this;
	Left.ImmediateSet(new ProtoatomT(Core));
	Right.ImmediateSet(new ProtoatomT(Core));
}
	
void AssignmentT::Serialize(Serial::WritePolymorphT &&Polymorph) const
{
	Left->Serialize(Polymorph.Polymorph("Left"));
	Right->Serialize(Polymorph.Polymorph("Right"));
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
			LeftAssociative = false;
		}
	} static Type;
	return Type;
}

AtomTypeT const &AssignmentT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void AssignmentT::Refocus(void)
{
	if (LeftFocused) { if (Left) Left->Refocus(); }
	else { if (Right) Right->Refocus(); }
}

void AssignmentT::Refresh(void)
{
	this->Visual.Clear();
	if (Left) this->Visual.Add(Left->Visual);
	this->Visual.Add("=");
	if (Right) this->Visual.Add(Right->Visual);
}

std::unique_ptr<ActionT> AssignmentT::Place(NucleusT *Nucleus)
{
	return Left.Set(Nucleus);
}

}

