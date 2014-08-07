#include "core.h"
#include "../shared/extrastandard.h"
#include "../shared/regex.h"

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
std::string JSSlash(std::string const &Text)
{
	return std::regex_replace(Text, JSSlashRegex, "\\$0");
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
	std::cout << "Set result: " << Core.Dump() << std::endl;
	Core.AssumeFocus();
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

NucleusT::NucleusT(CoreT &Core) : Core(Core), Parent(Core), Visual(Core.RootVisual.Root), Atom(nullptr) 
{
       Core.NeedRefresh.insert(this);	
}

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
	
void NucleusT::FocusPrevious(void) {}

void NucleusT::FocusNext(void) {}

void NucleusT::FlagRefresh(void) 
{
	Core.NeedRefresh.insert(this);
}

AtomTypeT::~AtomTypeT(void) {}

NucleusT *AtomTypeT::Generate(CoreT &Core) { Assert(false); return nullptr; }

template <typename DataT> struct CommonAtomTypeT;

template <typename DataT> struct CommonNucleusT : NucleusT
{
	friend struct CommonAtomTypeT<DataT>;
	private:
		CommonNucleusT(CoreT &Core, CommonAtomTypeT<DataT> &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), Data(Core) 
		{ 
			Visual.Tag().Add(TypeInfo.Tag);
			FlagRefresh();
		}

		struct SelfFocusedT {};
		typedef size_t PartFocusedT;
		VariantT<SelfFocusedT, PartFocusedT> Focused;

	public:
		CommonAtomTypeT<DataT> const &TypeInfo;
		
		DataT Data;

		void Serialize(Serial::WritePolymorphT &&Polymorph) const override
		{
			for (auto &Part : TypeInfo.Parts)
				Part->Serialize(*this, Polymorph);
		}
		
		AtomTypeT const &GetTypeInfo(void) const override { return TypeInfo; }

		void Focus(FocusDirectionT Direction) override
		{
			NucleusT::Focus(Direction);
			Focused = SelfFocusedT{};
			Visual.SetClass("flag-focused");
		}

		void Defocus(void) override 
		{
			Assert(Focused);
			if (Focused.template Is<PartFocusedT>())
			{
				TypeInfo.Parts[Focused.template Get<PartFocusedT>()]->Defocus(*this);
			}
			else
			{
				Assert(Focused.template Is<SelfFocusedT>());
				Visual.UnsetClass("flag-focused");
			}
			Focused = {};
		}

		void AssumeFocus(void) override
		{
			if (Focused) 
			{
				if (Focused.template Is<PartFocusedT>()) TypeInfo.Parts[Focused.template Get<PartFocusedT>()]->AssumeFocus(*this);
				else if (Focused.template Is<SelfFocusedT>()) Focus(FocusDirectionT::Direct);
				else Assert(false);
			}
			else
			{
				for (size_t Index = 0; Index < TypeInfo.Parts.size(); ++Index)
				{
					auto &Part = TypeInfo.Parts[Index];
					if (!Part->FocusDefault) continue;
					Focused = PartFocusedT(Index);
					Part->AssumeFocus(*this);
					return;
				}
				Focus(FocusDirectionT::Direct);
			}
		}
		
		std::unique_ptr<ActionT> Set(NucleusT *Nucleus) override
		{
			for (size_t Index = 0; Index < TypeInfo.Parts.size(); ++Index)
			{
				auto &Part = TypeInfo.Parts[Index];
				if (!Part->SetDefault) continue;
				auto Result = Part->Set(*this, Nucleus);
				Assert(Result);
				return std::move(Result);
			}
			Assert(false);
			return {};
		}

		std::unique_ptr<ActionT> Set(std::string const &Text) override
		{
			for (size_t Index = 0; Index < TypeInfo.Parts.size(); ++Index)
			{
				auto &Part = TypeInfo.Parts[Index];
				if (!Part->SetDefault) continue;
				auto Result = Part->Set(*this, Text);
				Assert(Result);
				return std::move(Result);
			}
			Assert(false);
			return {};
		}

		void Refresh(void) override
		{
			Visual.Start();
			for (auto &Part : TypeInfo.Parts)
			{
				Part->Refresh(*this);
				Visual.Add(Part->GetVisual(*this));
			}
			std::cout << "Refreshed " << this << " " << typeid(*this).name() << std::endl;
		}

		OptionalT<std::unique_ptr<ActionT>> HandleInput(InputT const &Input) override
		{
			Assert(Focused);
			if (Focused.template Is<SelfFocusedT>())
			{
				if (Input.Main)
				{
					switch (*Input.Main)
					{
						case InputT::MainT::Left: if (Parent) Parent->FocusPrevious(); return {};
						case InputT::MainT::Right: if (Parent) Parent->FocusNext(); return {};
						case InputT::MainT::Enter: 
						{
							Focused = PartFocusedT(0);
							TypeInfo.Parts[0]->Focus(*this, FocusDirectionT::Direct);
							return {};
						}
						case InputT::MainT::Exit: if (Parent) Parent->Focus(FocusDirectionT::Direct); return {};
						default: break;
					}
				}
			}
			else
			{
				if (Input.Main)
				{
					switch (*Input.Main)
					{
						case InputT::MainT::Exit: Focus(FocusDirectionT::Direct); return {};
						default: break;
					}
				}

				auto &Index = Focused.template Get<PartFocusedT>();
				return TypeInfo.Parts[Index]->HandleInput(*this, Input);
			}
			
			if (Parent) return Parent->HandleInput(Input);

			return {};
		}

		void FocusPrevious(void) override
		{
			Assert(Focused);
			Assert(Focused.template Is<PartFocusedT>());
			auto &Index = Focused.template Get<PartFocusedT>();
			if (Index == 0) 
			{
				if (Parent) Parent->FocusPrevious();
			}
			else
			{
				TypeInfo.Parts[Index]->Defocus(*this);
				Index -= 1;
				TypeInfo.Parts[Index]->Focus(*this, FocusDirectionT::FromAhead);
			}
		}

		void FocusNext(void) override
		{
			Assert(Focused);
			Assert(Focused.template Is<PartFocusedT>());
			auto &Index = Focused.template Get<PartFocusedT>();
			AssertGTE(TypeInfo.Parts.size(), 1);
			if (Index < TypeInfo.Parts.size() - 1) 
			{
				TypeInfo.Parts[Index]->Defocus(*this);
				Index += 1;
				TypeInfo.Parts[Index]->Focus(*this, FocusDirectionT::FromBehind);
			}
			else
			{
				if (Parent) Parent->FocusNext();
			}
		}
};

template <typename DataT> struct CommonAtomPartT
{
	virtual ~CommonAtomPartT(void) {}

	typedef CommonNucleusT<DataT> BaseT;

	virtual void Construct(BaseT &Base) = 0;
	virtual VisualT &GetVisual(BaseT &Base) const = 0;
	virtual void Serialize(BaseT const &Base, Serial::WritePolymorphT &Polymorph) const = 0;
	virtual void Focus(BaseT &Base, FocusDirectionT Direction) const = 0;
	virtual void Defocus(BaseT &Base) const {}
	virtual void AssumeFocus(BaseT &Base) const = 0;
	virtual void Refresh(BaseT &Base) const = 0;
	virtual OptionalT<std::unique_ptr<ActionT>> HandleInput(BaseT &Base, InputT const &Input) const = 0;

	virtual std::unique_ptr<ActionT> Set(BaseT &Base, NucleusT *Nucleus) { Assert(false); return {}; }
	virtual std::unique_ptr<ActionT> Set(BaseT &Base, std::string const &Text) { Assert(false); return {}; }
	
	// Unconfigurable
	std::string Identifier;
	bool FocusDefault = false;
	bool SetDefault = false;

	// Configurable
	std::string Prefix;
	std::string Suffix;
};

template <typename DataT> struct CommonAtomTypeT : AtomTypeT
{
	NucleusT *Generate(CoreT &Core) override
	{
		auto Out = new CommonNucleusT<DataT>(Core, *this);
		for (auto &Part : Parts) Part->Construct(*Out);
		return Out;
	}

	std::vector<std::unique_ptr<CommonAtomPartT<DataT>>> Parts;
};

template <typename DataT> struct AtomTypePartT : CommonAtomPartT<DataT>
{
	std::string Identifier;

	using BaseT = typename CommonAtomPartT<DataT>::BaseT;

	AtomT &(*Access)(BaseT &);
	AtomT const &(*AccessConst)(BaseT const &);

	AtomTypePartT(void) {}

	void Construct(BaseT &Base)
	{
		auto &Atom = Access(Base);
		Atom.Callback = [&Base](AtomT &This) { Base.FlagRefresh(); };
		Atom.Parent = &Base;
		Atom.ImmediateSet(new ProtoatomT(Base.Core));
	}

	VisualT &GetVisual(BaseT &Base) const override { return Access(Base)->Visual; }

	void Serialize(BaseT const &Base, Serial::WritePolymorphT &Polymorph) const override
		{ AccessConst(Base)->Serialize(Polymorph.Polymorph(Identifier)); }

	void Focus(BaseT &Base, FocusDirectionT Direction) const override
		{ Access(Base)->Focus(Direction); }

	void AssumeFocus(BaseT &Base) const override
		{ Access(Base)->AssumeFocus(); }

	void Refresh(BaseT &Base) const override
		{ Access(Base)->Refresh(); }

	OptionalT<std::unique_ptr<ActionT>> HandleInput(BaseT &Base, InputT const &Input) const override
		{ return Access(Base)->HandleInput(Input); }
};

struct AtomListElementT 
{
	VisualT Visual;
	AtomT Atom;
	AtomListElementT(CoreT &Core) : Visual(Core.RootVisual.Root), Atom(Core) {}
};
struct AtomListDataPartT
{
	std::vector<std::unique_ptr<AtomListElementT>> List;
	VisualT Visual;
	OptionalT<size_t> Focused;
	AtomListDataPartT(CoreT &Core) : Visual(Core.RootVisual.Root) {}
};

template <typename DataT> struct AtomListTypePartT : CommonAtomPartT<DataT>
{
	using BaseT = typename CommonAtomPartT<DataT>::BaseT; // Of course, normal typedef doesn't work
	using CommonAtomPartT<DataT>::Identifier; // Why? This is so stupid
	using CommonAtomPartT<DataT>::Prefix;
	using CommonAtomPartT<DataT>::Suffix;

	AtomListDataPartT &(*Access)(BaseT &);
	AtomListDataPartT const &(*AccessConst)(BaseT const &);

	AtomListTypePartT(void) {}

	void Construct(BaseT &Base) override
	{
		AddStatement(Base, 0);
		Access(Base).List[0]->Atom.Callback = [&Base](AtomT &This)
		{
			Base.FlagRefresh();
		};
		Access(Base).List[0]->Atom.ImmediateSet(new ProtoatomT(Base.Core));
	}

	VisualT &GetVisual(BaseT &Base) const override { return Access(Base).Visual; }

	void Serialize(BaseT const &Base, Serial::WritePolymorphT &Polymorph) const override
	{ 
		auto Array = Polymorph.Array(Identifier);
		for (auto &Statement : AccessConst(Base).List)
			Statement->Atom->Serialize(Array.Polymorph());
	}

	void Focus(BaseT &Base, FocusDirectionT Direction) const override
	{ 
		auto &List = Access(Base).List;
		auto &Focused = Access(Base).Focused;
		Assert(!List.empty());
		switch (Direction)
		{
			case FocusDirectionT::FromAhead: 
			{
				Focused = List.size() - 1; 
				break;
			}
			case FocusDirectionT::FromBehind:
			{
				Focused = 0; 
				break;
			}
			case FocusDirectionT::Direct:
			{
				Focused = 0; 
				break;
			}
		}
		List[*Focused]->Atom->Focus(Direction);
	}

	void AssumeFocus(BaseT &Base) const override
	{ 
		auto &Focused = Access(Base).Focused;
		if (!Focused) Focused = 0;
		Access(Base).List[*Focused]->Atom->AssumeFocus();
	}

	void Refresh(BaseT &Base) const override
	{ 
		Base.Visual.Start();
		for (auto &Element : Access(Base).List)
		{
			Element->Visual.Start();
			if (!Prefix.empty()) Element->Visual.Add(Prefix);
			Element->Visual.Add(Element->Atom->Visual);
			if (!Suffix.empty()) Element->Visual.Add(Suffix);
			Base.Visual.Add(Element->Visual);
		}
	}

	void AddStatement(BaseT &Base, size_t Index) const
	{
		auto &List = Access(Base).List;
		auto Iterator = List.begin();
		std::advance(Iterator, Index); // Portrait of a well designed api
		auto Element = List.insert(Iterator, make_unique<AtomListElementT>(Base.Core)); // Emplace tries copy constructor, nice job
		(*Element)->Atom.Parent = &Base;
		(*Element)->Atom.Callback = [&Base](AtomT &This)
		{
			Base.FlagRefresh();
		};
	}

	void RemoveStatement(BaseT &Base, size_t Index) const
	{
		auto &List = Access(Base).List;
		auto Iterator = List.begin();
		std::advance(Iterator, Index);
		List.erase(Iterator);
	}

	OptionalT<std::unique_ptr<ActionT>> HandleInput(BaseT &Base, InputT const &Input) const override
	{
		if (Input.Main && (*Input.Main == InputT::MainT::NewStatement))
		{
			Assert(Access(Base).Focused);
			struct AddRemoveStatementT : ActionT
			{
				AtomListTypePartT const &Type;
				BaseT &Base;
				size_t Index;
				OptionalT<HoldT> Add;
				
				AddRemoveStatementT(AtomListTypePartT const &Type, BaseT &Base, size_t Index, OptionalT<HoldT> Add) : Type(Type), Base(Base), Index(Index), Add(Add) {}

				std::unique_ptr<ActionT> Apply(void)
				{
					auto &List = Type.Access(Base).List;
					ActionT *Out;
					if (Add) 
					{
						Assert(Index <= List.size());
						Out = new AddRemoveStatementT(Type, Base, Index, OptionalT<HoldT>{});
						Type.AddStatement(Base, Index);
						List[Index]->Atom.ImmediateSet(Add->Nucleus);
					}
					else 
					{
						Assert(Index < List.size());
						Out = new AddRemoveStatementT(Type, Base, Index, HoldT(Base.Core, Type.Access(Base).List[Index]->Atom.Nucleus));
						Type.RemoveStatement(Base, Index);
					}
					return std::unique_ptr<ActionT>(Out);
				}
			};
			return std::unique_ptr<ActionT>(new AddRemoveStatementT(*this, Base, std::max(*Access(Base).Focused + 1, Access(Base).List.size()), HoldT(Base.Core, new ProtoatomT(Base.Core))));
		}
		if (Base.Parent) return Base.Parent->HandleInput(Input);
		return {};
	}
};

struct StringDataPartT
{
	VisualT Visual;
	size_t Position;
	std::string Text;
	
	StringDataPartT(CoreT &Core) : Visual(Core.RootVisual.Root), Position(0) {}
};
template <typename DataT> struct StringTypePartT : CommonAtomPartT<DataT>
{
	using BaseT = typename CommonAtomPartT<DataT>::BaseT;

	using CommonAtomPartT<DataT>::Identifier;
	using CommonAtomPartT<DataT>::Prefix;
	using CommonAtomPartT<DataT>::Suffix;
	
	StringDataPartT &(*Access)(BaseT &);
	StringDataPartT const &(*AccessConst)(BaseT const &);

	StringTypePartT(void) {}
	
	void Construct(BaseT &Base) {}

	VisualT &GetVisual(BaseT &Base) const override { return Access(Base).Visual; }

	void Serialize(BaseT const &Base, Serial::WritePolymorphT &Polymorph) const override
		{ Polymorph.String(Identifier, AccessConst(Base).Text); }

	void Focus(BaseT &Base, FocusDirectionT Direction) const override
	{ 
		static_cast<NucleusT &>(Base).Focus(Direction);
		switch (Direction)
		{
			case FocusDirectionT::FromAhead: Access(Base).Position = Access(Base).Text.size(); break;
			case FocusDirectionT::FromBehind: Access(Base).Position = 0; break;
			case FocusDirectionT::Direct: Access(Base).Position = 0; break;
		}
		Access(Base).Visual.SetClass("flag-focused");
		static_cast<NucleusT &>(Base).Focus(Direction);
		Base.FlagRefresh();
	}

	void Defocus(BaseT &Base) const override
	{
		Access(Base).Visual.UnsetClass("flag-focused");
	}

	void AssumeFocus(BaseT &Base) const override
		{ static_cast<NucleusT &>(Base).Focus(FocusDirectionT::Direct); }

	void Refresh(BaseT &Base) const override
	{ 
		auto &Visual = Access(Base).Visual;
		Visual.Start();
		if (!Prefix.empty()) Visual.Add(Prefix);
		Visual.Add(Access(Base).Text);
		if (!Suffix.empty()) Visual.Add(Suffix);
	}
		
	struct SetT : ActionT
	{
		StringTypePartT const &Type;
		BaseT &Base;
		unsigned int Position;
		std::string Text;
		
		SetT(StringTypePartT const &Type, BaseT &Base, unsigned int Position, std::string const &Text) : Type(Type), Base(Base), Position(Position), Text(Text) {}
		
		std::unique_ptr<ActionT> Apply(void)
		{
			auto Out = new SetT(Type, Base, Type.Access(Base).Position, Type.Access(Base).Text);
			auto &Data = Type.Access(Base);
			Data.Text = Text;
			Data.Position = Position;
			Base.FlagRefresh();
			return std::unique_ptr<ActionT>(Out);
		}
		
		bool Combine(std::unique_ptr<ActionT> &Other) override
		{
			auto Set = dynamic_cast<SetT *>(Other.get());
			if (!Set) return false;
			if (&Set->Base != &Base) return false;
			Position = Set->Position;
			Text = Set->Text;
			return true;
		}
	};

	OptionalT<std::unique_ptr<ActionT>> HandleInput(BaseT &Base, InputT const &Input) const override
	{ 
		if (Input.Text)
		{
			return std::unique_ptr<ActionT>(new SetT(
				*this, 
				Base, 
				Access(Base).Position + 1, 
				Access(Base).Text.substr(0, Access(Base).Position) + 
					*Input.Text + 
					Access(Base).Text.substr(Access(Base).Position)));
		}
		else if (Input.Main)
		{
			auto &Data = Access(Base);
			switch (*Input.Main)
			{
				case InputT::MainT::Left:
				{
					if (Data.Position == 0) 
					{
						Base.FocusPrevious();
					}
					else
					{
						Data.Position -= 1;
						Base.FlagRefresh();
					}
					return {};
				}
				case InputT::MainT::Right:
				{
					if (Data.Position == Data.Text.size()) 
					{
						Base.FocusNext();
					}
					else
					{
						Data.Position += 1;
						Base.FlagRefresh();
					}
					return {};
				}
				case InputT::MainT::TextBackspace:
				{
					if (Data.Position == 0) return {};
					Data.Text.erase(Data.Position - 1, 1);
					Data.Position -= 1;
					Base.FlagRefresh();
					return {};
				}
				case InputT::MainT::Delete:
				{
					if (Data.Position == Data.Text.size()) return {};
					Data.Text.erase(Data.Position, 1);
					Base.FlagRefresh();
					return {};
				}
				default: break;
			}
		}
		if (Base.Parent) return Base.Parent->HandleInput(Input);
		return {};
	}
	
	std::unique_ptr<ActionT> Set(BaseT &Base, std::string const &Text) override
	{
		return std::unique_ptr<ActionT>(new SetT(*this, Base, Text.size(), Text));
	}
};

struct EnumDataPartT
{
	VisualT Visual;
	size_t Index;
	EnumDataPartT(CoreT &Core) : Visual(Core.RootVisual.Root), Index(0) {}
};
template <typename DataT> struct EnumTypePartT : CommonAtomPartT<DataT>
{
	std::string Identifier;

	std::vector<std::string> Values;

	using BaseT = typename CommonAtomPartT<DataT>::BaseT;

	EnumDataPartT &(*Access)(BaseT &);
	EnumDataPartT const &(*AccessConst)(BaseT const &);

	EnumTypePartT(void) {}
	
	void Construct(BaseT &Base)
	{
		Access(Base).Index = 0;
	}

	VisualT &GetVisual(BaseT &Base) const override { return Access(Base).Visual; }

	void Serialize(BaseT const &Base, Serial::WritePolymorphT &Polymorph) const override
	{ 
		if (AccessConst(Base).Index >= Values.size())
		{
			Assert(false);
			Polymorph.String(Identifier, "Invalid");
		}
		else
		{
			Polymorph.String(Identifier, Values[AccessConst(Base).Index]);
		}
	}

	void Focus(BaseT &Base, FocusDirectionT Direction) const override
	{ 
		static_cast<NucleusT &>(Base).Focus(Direction);
		Access(Base).Visual.SetClass("flag-focused");
	}
	
	void Defocus(BaseT &Base) const override
	{
		Access(Base).Visual.UnsetClass("flag-focused");
	}

	void AssumeFocus(BaseT &Base) const override
		{ static_cast<NucleusT &>(Base).Focus(FocusDirectionT::Direct); }

	void Refresh(BaseT &Base) const override
	{ 
		auto &Data = Access(Base);
		Data.Visual.Start();
		if (Data.Index >= Values.size())
		{
			Assert(false);
			Data.Visual.Add("Invalid");
		}
		else
		{
			Data.Visual.Add(Values[Access(Base).Index]);
		}
	}

	OptionalT<std::unique_ptr<ActionT>> HandleInput(BaseT &Base, InputT const &Input) const override
	{ 
		if (Input.Main)
		{
			auto &Index = Access(Base).Index;
			switch (*Input.Main)
			{
				case InputT::MainT::Left: Base.FocusPrevious(); return {};
				case InputT::MainT::Right: Base.FocusNext(); return {};
				case InputT::MainT::Enter: 
				{
					struct SetT : ActionT
					{
						EnumTypePartT const &Type;
						BaseT &Base;
						size_t Index;

						SetT(EnumTypePartT const &Type, BaseT &Base, size_t Index) : Type(Type), Base(Base), Index(Index)
						{
						}

						std::unique_ptr<ActionT> Apply(void)
						{
							auto Out = new SetT(Type, Base, Type.Access(Base).Index);
							Type.Access(Base).Index = Index;
							Base.FlagRefresh();
							return std::unique_ptr<ActionT>(Out);
						}
					};
					return std::unique_ptr<ActionT>(new SetT(*this, Base, (Index + 1) % Values.size()));
				}
				default: break;
			}
		}
		if (Base.Parent) return Base.Parent->HandleInput(Input);
		return {};
	}
};

struct ModuleDataT
{
	EnumDataPartT Entry;
	AtomT Top;
	ModuleDataT(CoreT &Core) : Entry(Core), Top(Core) {}
};
struct ModuleTypeT : CommonAtomTypeT<ModuleDataT>
{
	ModuleTypeT(void)
	{
		Tag = "Module";
		{
			auto Part = make_unique<EnumTypePartT<ModuleDataT>>();
			Part->Identifier = "Entry";
			Part->Values.push_back("<Has Entry>");
			Part->Values.push_back("<No Entry>");
			Part->Access = [](CommonNucleusT<ModuleDataT> &Nucleus) -> EnumDataPartT & { return Nucleus.Data.Entry; };
			Part->AccessConst = [](CommonNucleusT<ModuleDataT> const &Nucleus) -> EnumDataPartT const & { return Nucleus.Data.Entry; };
			Parts.emplace_back(std::move(Part));
		}
		{
			auto Part = make_unique<AtomTypePartT<ModuleDataT>>();
			Part->Identifier = "Top";
			Part->FocusDefault = true;
			Part->SetDefault = true;
			Part->Access = [](CommonNucleusT<ModuleDataT> &Nucleus) -> AtomT & { return Nucleus.Data.Top; };
			Part->AccessConst = [](CommonNucleusT<ModuleDataT> const &Nucleus) -> AtomT const & { return Nucleus.Data.Top; };
			Parts.emplace_back(std::move(Part));
		}
	}
};
typedef CommonNucleusT<ModuleDataT> ModuleT;

struct GroupDataT
{
	AtomListDataPartT Statements;
	GroupDataT(CoreT &Core) : Statements(Core) {}
};
struct GroupTypeT : CommonAtomTypeT<GroupDataT>
{
	GroupTypeT(void)
	{
		Tag = "Group";
		ReplaceImmediately = true;
		Arity = ArityT::Nullary;
		Prefix = true;
		Precedence = 0;
		LeftAssociative = true;
		{
			auto Part = make_unique<AtomListTypePartT<GroupDataT>>();
			Part->Identifier = "Statements";
			Part->FocusDefault = true;
			Part->SetDefault = true;
			Part->Suffix = ";";
			Part->Access = [](CommonNucleusT<GroupDataT> &Nucleus) -> AtomListDataPartT & { return Nucleus.Data.Statements; };
			Part->AccessConst = [](CommonNucleusT<GroupDataT> const &Nucleus) -> AtomListDataPartT const & { return Nucleus.Data.Statements; };
			Parts.emplace_back(std::move(Part));
		}
	}
};

struct ElementDataT
{
	AtomT Base;
	AtomT Key;
	ElementDataT(CoreT &Core) : Base(Core), Key(Core) {}
};
struct ElementTypeT : CommonAtomTypeT<ElementDataT>
{
	ElementTypeT(void)
	{
		Tag = "Element";
		ReplaceImmediately = true;
		Arity = ArityT::Binary;
		Prefix = false;
		Precedence = 900;
		LeftAssociative = true;
		{
			auto Part = make_unique<AtomTypePartT<ElementDataT>>();
			Part->Identifier = "Base";
			Part->Suffix = ".";
			Part->SetDefault = true;
			Part->Access = [](CommonNucleusT<ElementDataT> &Nucleus) -> AtomT & { return Nucleus.Data.Base; };
			Part->AccessConst = [](CommonNucleusT<ElementDataT> const &Nucleus) -> AtomT const & { return Nucleus.Data.Base; };
			Parts.emplace_back(std::move(Part));
		}
		{
			auto Part = make_unique<AtomTypePartT<ElementDataT>>();
			Part->Identifier = "Key";
			Part->FocusDefault = true;
			Part->Access = [](CommonNucleusT<ElementDataT> &Nucleus) -> AtomT & { return Nucleus.Data.Key; };
			Part->AccessConst = [](CommonNucleusT<ElementDataT> const &Nucleus) -> AtomT const & { return Nucleus.Data.Key; };
			Parts.emplace_back(std::move(Part));
		}
	}
};
typedef CommonNucleusT<ElementDataT> ElementT;

struct AssignmentDataT
{
	AtomT Left;
	AtomT Right;
	AssignmentDataT(CoreT &Core) : Left(Core), Right(Core) {}
};
struct AssignmentTypeT : CommonAtomTypeT<AssignmentDataT>
{
	AssignmentTypeT(void)
	{
		Tag = "Assignment";
		ReplaceImmediately = true;
		Arity = ArityT::Binary;
		Prefix = false;
		Precedence = 900;
		LeftAssociative = true;
		{
			auto Part = make_unique<AtomTypePartT<AssignmentDataT>>();
			Part->Identifier = "Left";
			Part->SetDefault = true;
			Part->Suffix = "=";
			Part->Access = [](CommonNucleusT<AssignmentDataT> &Nucleus) -> AtomT & { return Nucleus.Data.Left; };
			Part->AccessConst = [](CommonNucleusT<AssignmentDataT> const &Nucleus) -> AtomT const & { return Nucleus.Data.Left; };
			Parts.emplace_back(std::move(Part));
		}
		{
			auto Part = make_unique<AtomTypePartT<AssignmentDataT>>();
			Part->Identifier = "Right";
			Part->FocusDefault = true;
			Part->Access = [](CommonNucleusT<AssignmentDataT> &Nucleus) -> AtomT & { return Nucleus.Data.Right; };
			Part->AccessConst = [](CommonNucleusT<AssignmentDataT> const &Nucleus) -> AtomT const & { return Nucleus.Data.Right; };
			Parts.emplace_back(std::move(Part));
		}
	}
};

typedef StringDataPartT StringDataT;
struct StringTypeT : CommonAtomTypeT<StringDataT>
{
	StringTypeT(void) 
	{
		Tag = "String";
		ReplaceImmediately = true;
		Arity = ArityT::Nullary;
		Prefix = true;
		{
			auto Part = make_unique<StringTypePartT<StringDataT>>();
			Part->Identifier = "String";
			Part->SetDefault = true;
			Part->FocusDefault = true;
			Part->Prefix = "'";
			Part->Suffix = "'";
			Part->Access = [](CommonNucleusT<StringDataT> &Nucleus) -> StringDataPartT & { return Nucleus.Data; };
			Part->AccessConst = [](CommonNucleusT<StringDataT> const &Nucleus) -> StringDataPartT const & { return Nucleus.Data; };
			Parts.emplace_back(std::move(Part));
		}
	}
};

CoreT::CoreT(VisualT &RootVisual) : RootVisual(RootVisual), Root(*this), Focused(*this), CursorVisual(RootVisual.Root)
{
	ModuleType.reset(new ModuleTypeT());
	GroupType.reset(new GroupTypeT());
	AssignmentType.reset(new AssignmentTypeT());
	ElementType.reset(new ElementTypeT());
	StringType.reset(new StringTypeT());
	Types["."] = ElementType.get();
	Types["'"] = StringType.get();
	Types["="] = AssignmentType.get();
	Types["{"] = GroupType.get();
	
	Root.Callback = [this](AtomT &This)
	{
		this->RootVisual.Start();
		this->RootVisual.Add(This->Visual);
	};
	
	Root.ImmediateSet(ModuleType->Generate(*this));
	(*Root->As<ModuleT>())->Data.Top.ImmediateSet(GroupType->Generate(*this));
	Refresh();
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
	
ProtoatomT::ProtoatomT(CoreT &Core) : NucleusT(Core), Lifted(Core) 
{
	Visual.Tag().Add(GetTypeInfo().Tag);
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
		TypeT(void)
		{
			Tag = "Protoatom";
		}
	} static Type;
	return Type;
}

AtomTypeT const &ProtoatomT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void ProtoatomT::Focus(FocusDirectionT Direction)
{
	if (Direction == FocusDirectionT::FromBehind)
	{
		Position = 0;
		if (Lifted)
		{
			Lifted->Focus(Direction);
			return;
		}
	}
	else if (Direction == FocusDirectionT::FromAhead)
	{
		Position = Data.size();
	}
	Focused = true;
	Visual.SetClass("flag-focused");
	FlagRefresh();
	NucleusT::Focus(Direction);
}

void ProtoatomT::Defocus(void) 
{
	Focused = false;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
}

void ProtoatomT::AssumeFocus(void)
{
	Focus(FocusDirectionT::Direct);
}

void ProtoatomT::Refresh(void)
{
	Visual.Start();
	if (Lifted) Visual.Add(Lifted->Visual);
	if (Focused)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
}
	
void ProtoatomT::Lift(NucleusT *Nucleus)
{
	Assert(!Lifted);
	Lifted = Nucleus;
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::HandleInput(InputT const &Input)
{
	if (Input.Text)
	{
		auto Text = *Input.Text;

		static auto IdentifierClass = Regex::ParserT<>("[a-zA-Z0-9_]");
		auto NewIsIdentifier = IdentifierClass(Text);
		
		if (Text == " ") return Finish({}, {}, {});
		else if (Text == "\n") return Finish({}, {}, InputT{InputT::MainT::NewStatement, {}});
		else if (Text == ";") return Finish({}, {}, InputT{InputT::MainT::NewStatement, {}});
		else if (!IsIdentifier || (*IsIdentifier == NewIsIdentifier))
		{
			if (!IsIdentifier) IsIdentifier = NewIsIdentifier;
			auto NewData = Data;
			NewData.insert(Position, Text);
			auto NewPosition = Position + 1;
			if (NewPosition == NewData.size()) // Only do auto conversion if appending text
			{
				auto Found = Core.LookUpAtom(NewData);
				if (Found && Found->ReplaceImmediately) 
					return Finish(Found, NewData, {});
			}

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
					Base.FlagRefresh();
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
			return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, Data + Text));
		}
		else if (IsIdentifier && (*IsIdentifier != NewIsIdentifier))
		{
			return Finish({}, {}, {Input});
		}
	}
	else if (Input.Main)
	{
		switch (*Input.Main)
		{
			case InputT::MainT::Left: 
			{
				if (Position == 0) 
				{
					Parent->FocusPrevious();
				}
				else
				{
					Position -= 1;
					FlagRefresh();
				}
			} return {};
			case InputT::MainT::Right:
			{
				if (Position == Data.size()) 
				{
					Parent->FocusNext();
				}
				else
				{
					Position += 1;
					FlagRefresh();
				}
			} return {};
			case InputT::MainT::TextBackspace:
			{
				if (Position == 0) return {};
				Data.erase(Position - 1, 1);
				Position -= 1;
				FlagRefresh();
			} return {};
			case InputT::MainT::Delete:
			{
				if (Position == Data.size()) return {};
				Data.erase(Position, 1);
				FlagRefresh();
			} return {};
			default: break;
		}
	}
	
	auto Result = NucleusT::HandleInput(Input);
	if (Result)
	{
		auto ActionGroup = new ActionGroupT;
		auto FinishResult = Finish({}, {}, {});
		if (FinishResult) ActionGroup->Add(std::move(*FinishResult));
		ActionGroup->Add(std::move(*Result));
		return std::unique_ptr<ActionT>(ActionGroup);
	}

	return {};
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::Finish(
	OptionalT<AtomTypeT *> Type, 
	OptionalT<std::string> NewData, 
	OptionalT<InputT> SeedData)
{
	Assert(Atom);
	std::string Text = NewData ? *NewData : Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	auto Actions = new ActionGroupT;
	if (Type)
	{
		Assert(!SeedData);
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted) 
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			auto Replacement = new ProtoatomT(Core);
			auto Finished = Type->Generate(Core);
			Actions->Add(Replacement->Set(Finished));
			if ((*Type)->Arity != ArityT::Nullary)
			{
				auto Proto = new ProtoatomT(Core);
				Actions->Add(Finished->Set(Proto));
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
			Actions->Add(Finished->Set(Child));
		}
	}
	else
	{
		if (Lifted) 
		{
			if (Data.empty())
			{
				Assert(Atom);
				std::cout << "Replacing " << this << " with " << Lifted.Nucleus << ", atom " << Atom << std::endl;
				return Atom->Set(Lifted.Nucleus);
			}

			std::cout << "NOTE: Can't finish as new element if protoatom has lifted." << std::endl; 
			return {};
		}
			
		auto String = Core.StringType->Generate(Core);
		Actions->Add(String->Set(Text));

		auto Protoatom = new ProtoatomT(Core);

		if (Parent->As<ElementT>())
		{
			Protoatom->Lift(String);
		}
		else
		{
			auto Nucleus = Core.ElementType->Generate(Core);
			auto Element = Nucleus->As<ElementT>();
			(*Element)->Data.Key.ImmediateSet(String);
			Protoatom->Lifted.Set(Nucleus);
		}

		std::cout << Core.Dump() << std::endl; // DEBUG
		Actions->Add(Atom->Set(Protoatom));

		if (SeedData) Actions->Add(Core.ActionHandleInput(*SeedData));
		std::cout << "Standard no-type finish." << std::endl;
	}

	return std::unique_ptr<ActionT>(Actions);
}

}

