#include "core.h"
#include "protoatom.h"
#include "logging.h"
#include "../ren-cxx-basics/extrastandard.h"

#include <regex>
#include <fstream>

namespace Core
{

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
				"window.scrollBy(0, Math.max(0, ElementBottom - window.innerHeight + 15));" // Qt/Webkit issues
				//<< ID << ".scrollIntoView(false);"
				//"window.scrollBy(0, 30);" // Qt/Webkit issues
			"}"
		"}"
		"})();");
}

void VisualT::ScrollToTop(void)
{
	EvaluateJS(Root, ::StringT()
		<< ID << ".scrollIntoView(true);");
}
			
bool TOCLocationT::operator <(TOCLocationT const &Other) const
{
	auto ThisOrders = Orders.begin();
	auto OtherOrders = Other.Orders.begin();
	for (size_t Index = 0; Index < std::min(Orders.size(), Other.Orders.size()); ++Index)
	{
		if (*ThisOrders < *OtherOrders) return true;
		if (*ThisOrders > *OtherOrders) return false;
		++ThisOrders;
		++OtherOrders;
	}
	if (Orders.size() < Other.Orders.size()) return true;
	return false;
}

bool TOCLocationT::Contains(TOCLocationT const &Other) const
{
	if (Orders.size() > Other.Orders.size()) return false;
	auto ThisOrders = Orders.begin();
	auto OtherOrders = Other.Orders.begin();
	for (size_t Index = 0; Index < Orders.size() - 1; ++Index)
	{
		if (*ThisOrders != *OtherOrders) return false;
		++ThisOrders;
		++OtherOrders;
	}
	/*AssertNE(*ThisOrders, *OtherOrders);
	if (*OtherOrders < *ThisOrders) return false;
	*/
	AssertGT(*OtherOrders, *ThisOrders);
	if (Other.Orders.size() > Orders.size()) return true;
	return Other.Level > Level;
}
	
std::string TOCLocationT::Dump(void *Pointer) const
{
	std::stringstream Out;
	for (auto &Ord : Orders) Out << Ord << " ";
	Out << "(" << Level << ") " << Pointer;
	return Out.str();
}
	
TOCVisualT::~TOCVisualT(void) {}

ReactionT::~ReactionT(void) {}
	
bool ReactionT::Combine(std::unique_ptr<ReactionT> &Other) { return false; }

ActionT::ActionT(std::string const &Name) : Name(Name) {}

ActionT::ArgumentT::~ArgumentT(void) {}

ActionT::~ActionT(void) {}
	
FunctionActionT::FunctionActionT(std::string const &Name, std::function<void(std::unique_ptr<UndoLevelT> &Level)> const &Function) : ActionT(Name), Function(Function) {}

void FunctionActionT::Apply(std::unique_ptr<UndoLevelT> &Level) { Function(Level); }

HoldT::HoldT(CoreT &Core) : Core(Core), Nucleus_(nullptr) {}

HoldT::HoldT(CoreT &Core, NucleusT *Nucleus) : Core(Core), Nucleus_(nullptr)
{
	Set(Nucleus);
}

HoldT::HoldT(HoldT const &Other) : Core(Other.Core), Nucleus_(nullptr)
{
	Set(Other.Nucleus_);
}

HoldT::~HoldT(void) 
{ 
	Clear(); 
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
	Nucleus_ = Hold.Nucleus_;
	Hold.Nucleus_ = nullptr;
	return *this;
}

void HoldT::Set(NucleusT *Nucleus)
{
	if (!Nucleus) return;
	Nucleus_ = Nucleus;
	Nucleus_->Count += 1;
}

void HoldT::Clear(void)
{
	if (!Nucleus_) return;
	Nucleus_->Count -= 1;
	if (Nucleus_->Count == 0)
		Core.DeletionCandidates.insert(Nucleus_);
	Nucleus_ = nullptr;
}

HoldT::operator bool(void) const { return Nucleus_; }

NucleusT *HoldT::operator ->(void) 
{ 
	return Nucleus_; 
}

NucleusT const *HoldT::operator ->(void) const
{ 
	return Nucleus_; 
}
	
NucleusT *HoldT::Nucleus(void) { return Nucleus_; }

NucleusT const *HoldT::Nucleus(void) const { return Nucleus_; }

AtomT::AtomT(CoreT &Core) : Core(Core), Parent(nullptr), Order_(0), Nucleus_(nullptr) {}

AtomT::~AtomT(void) 
{
	Clear();
}

void AtomT::Set(std::unique_ptr<UndoLevelT> &Level, NucleusT *Replacement)
{
	TRACE;

	struct SetT : ReactionT
	{
		AtomT &Base;
		HoldT Replacement;

		SetT(AtomT &Base, NucleusT *Replacement) : Base(Base), Replacement(Base.Core, Replacement) { }
		
		void Apply(std::unique_ptr<UndoLevelT> &Level) { Base.Set(Level, Replacement.Nucleus()); }
	};
	Level->Add(std::make_unique<SetT>(*this, Nucleus_));

	if (Callback) Callback(Replacement);

	auto Original = Nucleus_;
	Clear();
	if (Original) Original->LocationChanged();

	Assert(Core.UndoingOrRedoing || Replacement);
	if (!Replacement) 
	{
		//std::cout << "Set result: " << Core.Dump() << std::endl;
		return; // ^ Already cleared above
	}

	// Assign
	Nucleus_ = Replacement;
	//std::cout << "Set " << this << " to " << Replacement << std::endl;

	Nucleus_->Count += 1;

	if (Nucleus_->Atom_)
	{
		//std::cout << "Relocating atom " << Nucleus_ << std::endl;
		if (Nucleus_->Atom_->Callback) Nucleus_->Atom_->Callback(nullptr);
		Level->Add(std::make_unique<SetT>(*Nucleus_->Atom_, Nucleus_));
		Nucleus_->Atom_->Clear();
	}
	Assert(!Nucleus_->Atom_);
	Nucleus_->Atom_ = this;

	Nucleus_->Parent_ = Parent;

	Nucleus_->LocationChanged();
	if (Parent) 
	{
		auto Composite = Parent->As<CompositeT>();
		if (!Composite)
			if (auto RealParent = Parent->PartParent())
				Composite = RealParent->As<CompositeT>();
		if (Composite)
			Nucleus_->ViewChanged(Core.IsFramed(), Composite->TypeInfo.Ellipsize ? Composite->Depth() + 1 : Composite->Depth());
	}

	//std::cout << "Set result: " << Core.Dump() << std::endl;
}
	
AtomT::operator bool(void) const { return Nucleus_; }

NucleusT *AtomT::operator ->(void) 
{ 
	return Nucleus_; 
}

NucleusT const *AtomT::operator ->(void) const
{ 
	return Nucleus_; 
}
	
NucleusT *AtomT::Nucleus(void) { return Nucleus_; }

NucleusT const *AtomT::Nucleus(void) const { return Nucleus_; }

size_t AtomT::Order(void) const { return Order_; }

void AtomT::SetOrder(size_t Order)
{
	Order_ = Order;
	if (Nucleus_) Nucleus_->LocationChanged();
}

void AtomT::Clear(void)
{
	if (!Nucleus_) return;
	AssertE(Nucleus_->Atom_, this);
	Nucleus_->Parent_ = nullptr;
	Nucleus_->Atom_ = nullptr;
	Nucleus_->Count -= 1;
	if (Nucleus_->Count == 0) Core.DeletionCandidates.insert(Nucleus_);
	Nucleus_ = nullptr;
	//std::cout << "Cleared " << this << std::endl;
}
	
NucleusReduction1T::NucleusReduction1T(CoreT &Core) : Parent_(Core), Atom_(nullptr) { }

NucleusT *NucleusReduction1T::Parent(void) { return Parent_.Nucleus(); }

NucleusT const *NucleusReduction1T::Parent(void) const { return Parent_.Nucleus(); }

AtomT *NucleusReduction1T::Atom(void) { return Atom_; }

AtomT const *NucleusReduction1T::Atom(void) const { return Atom_; }
	
NucleusT::NucleusT(CoreT &Core) : NucleusReduction1T(Core), Core(Core), Visual(Core.RootVisual.Root)
{
	Visual.SetFocusTarget(this);
	Core.NeedRefresh.insert(this);	
}

NucleusT::~NucleusT(void) {}

OptionalT<NucleusT *> NucleusT::PartParent(void)
{
	if (!Parent()) return {};
	auto Test = Parent();
	while (!Test->template As<CompositeT>()) // Since everything is a Composite now, this is an okay test.  Maybe add a PartParent ref to AtomT?
	{
		if (!Test->Parent()) return {};
		Test = Test->Parent();
	}
	return Test;
}

OptionalT<std::list<size_t>> NucleusT::GetGlobalOrder(void) const
{
	std::list<size_t> Path;
	NucleusT const *At = this;
	while (At->Atom())
	{
		Path.push_front(At->Atom()->Order());
		if (!At->Parent()) break;
		At = At->Parent();
	}

	if (At->Atom() != &Core.Root) return {};
	return Path;
}
	
AtomTypeT const &NucleusT::GetTypeInfo(void) const
{
	static AtomTypeT Type;
	return Type;
}

void NucleusT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	//Serialize(Serial::WritePolymorphT(GetTypeInfo().Tag, std::move(Prepolymorph)));
	auto Polymorph = Serial::WritePolymorphT(GetTypeInfo().Tag, std::move(Prepolymorph));
	/*Polymorph.String("this", ::StringT() << this);
	Polymorph.String("parent", ::StringT() << Parent.Nucleus());
	Polymorph.String("atom", ::StringT() << Atom);*/
	Serialize(Polymorph);
}
		
void NucleusT::Serialize(Serial::WritePolymorphT &WritePolymorph) const {}

void NucleusT::Focus(std::unique_ptr<UndoLevelT> &Level, FocusDirectionT Direction) 
{
	TRACE;
	if (Parent()) Parent()->AlignFocus(this);
	((CoreReduction2T &)Core).Focus(Level, this);
}
	
void NucleusT::FocusPrevious(std::unique_ptr<UndoLevelT> &Level) { TRACE; }

void NucleusT::FocusNext(std::unique_ptr<UndoLevelT> &Level) { TRACE; }
	
void NucleusT::AlignFocus(NucleusT *Child)
{
	if (Parent()) Parent()->AlignFocus(this);
}
	
void NucleusT::AssumeFocus(std::unique_ptr<UndoLevelT> &Level) { }

bool NucleusT::IsFocused(void) const { return Core.Focused() == this; }
	
bool NucleusT::IsFramed(void) const { return Core.Framed.Nucleus() == this; }

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
	
void NucleusT::Defocus(std::unique_ptr<UndoLevelT> &Level) {}
		
void NucleusT::LocationChanged(void) {}

void NucleusT::ViewChanged(bool Framed, size_t Depth) { }

void NucleusT::Refresh(void) { Assert(false); }

void NucleusT::FlagRefresh(void) 
{
	Core.NeedRefresh.insert(this);
}
	
void NucleusT::FlagStatusChange(void)
{
	for (auto &Watcher : StatusWatchers)
		Watcher.second(this);
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
	Level->Add(std::make_unique<FocusT>(Core, Target.Nucleus(), !DoNothing));
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
	
CoreBaseT::CoreBaseT(CoreT &Core, VisualT &RootVisual) : 
	TextMode(true),
	This(Core),
	Root(Core), 
	Focused_(Core),
	RootVisual(RootVisual), 
	RootRefresh(false),
	NeedScroll(false)
{
}

#define COREVINIT CoreBaseT((CoreT &)(*(CoreT *)nullptr), (VisualT &)(*(VisualT *)nullptr))

CoreBaseT::SettingsT const &CoreBaseT::Settings(void) const { return Settings_; }
	
NucleusT *CoreBaseT::Focused(void) { return Focused_.Nucleus(); }

NucleusT const *CoreBaseT::Focused(void) const { return Focused_.Nucleus(); }

Serial::ReadErrorT CoreBaseT::Deserialize(std::unique_ptr<UndoLevelT> &Level, AtomT &Out, std::string const &TypeName, Serial::ReadObjectT &Object)
{
	std::unique_ptr<NucleusT> Nucleus;
	auto Type = Types.find(TypeName);
	if (Type == Types.end()) return (StringT() << "Unknown type \'" << TypeName << "\'").str();
	Out.Set(Level, Type->second->Generate(This));
	return Out->Deserialize(Object);
}

CoreReduction1T::CoreReduction1T(void) : 
	COREVINIT, 
	CursorVisual(RootVisual.Root),
	SoloProtoatomType(nullptr), 
	InsertProtoatomType(nullptr), 
	AppendProtoatomType(nullptr), 
	ElementType(nullptr), 
	StringType(nullptr)
{
	CursorVisual.SetClass("cursor");
	CursorVisual.Add("|");
}

CoreReduction2_1T::CoreReduction2_1T(CoreT &Core) : COREVINIT, Framed(Core) {}

CoreReduction2T::CoreReduction2T(CoreT &Core) : COREVINIT, CoreReduction2_1T(Core) {}

bool CoreReduction2T::IsFramed(void) const { return Framed; }

void CoreReduction2T::Focus(std::unique_ptr<UndoLevelT> &Level, NucleusT *Nucleus)
{
	Assert(Nucleus);
	if (Focused_.Nucleus() == Nucleus) return;
	if (Focused_) Focused_->Defocus(Level);
	Focused_ = Nucleus;
	std::cout << "FOCUSED " << Nucleus << std::endl;
	NeedScroll = true;

	if (Framed)
	{
		std::vector<NucleusT *> Ancestry;
		OptionalT<NucleusT *> Unframe = Nucleus;
		if (!Unframe->As<CompositeT>()) Unframe = Unframe->PartParent();
		while (Unframe)
		{
			auto Composite = Unframe->As<CompositeT>();
			Assert(Composite);
			bool HitFramed = *Unframe == Framed.Nucleus();
			if (Composite->TypeInfo.Ellipsize || HitFramed)
			{
				Ancestry.push_back(*Unframe);
				std::cout << "Ancestor " << Unframe->GetTypeInfo().Tag << std::endl;
				if (HitFramed) break;
			}
			Unframe = Unframe->PartParent();
		}
		std::cout << "Ancestry size " << Ancestry.size() << std::endl;
		if (!Ancestry.empty() && (Ancestry.back() == Framed.Nucleus()))
		{
			if (Settings_.FrameDepth && (Ancestry.size() > *Settings_.FrameDepth))
			{
				std::cout << "Ancestry > " << *Settings_.FrameDepth << std::endl;
				Frame(Ancestry[*Settings_.FrameDepth - 1]);
			}
		}
		else
		{
			std::cout << "Ancestry no framed (" << Framed.Nucleus() << ")" << std::endl;
			if (Settings_.UnframeAtRoot && (Root.Nucleus() == Nucleus)) Frame(nullptr);
			else Frame(Nucleus);
		}
	}

	if (FocusChangedCallback) FocusChangedCallback();
}
	
void CoreReduction2T::Frame(NucleusT *Nucleus)
{
	if (Framed.Nucleus() == Nucleus) return;
	if (Framed && Framed->Parent()) { Framed->Parent()->FlagRefresh(); }
	Framed = Nucleus;
	if (Framed) Framed->ViewChanged(true, 1);
	else Root->ViewChanged(false, 1);
	RootRefresh = true;
}
	
void CoreReduction2T::Copy(NucleusT *Tree)
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

void CoreReduction2T::Paste(std::unique_ptr<UndoLevelT> &UndoLevel, AtomT &Destination)
{
	// TODO unify this with deserialize... somehow. Need stream abstraction
	if (!PasteCallback) return;
	HoldT TempRoot2(This); // Used to prevent clear reaction when setting destination
	{
		AtomT TempRoot(This); // Used as buffer to abort if deserialization fails
		Serial::ReadT Read;
		Read.Object([this, &TempRoot, &UndoLevel](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
		{
			Object.Polymorph("Root", [this, &TempRoot, &UndoLevel](std::string &&Key, Serial::ReadObjectT &Object)
			{
				auto DiscardUndo = std::make_unique<UndoLevelT>();
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
		Assert(TempRoot.Nucleus());
		TempRoot2.Set(TempRoot.Nucleus());
	}
	Destination.Set(UndoLevel, TempRoot2.Nucleus());
}

CoreReduction3T::CoreReduction3T(void) : COREVINIT {}

CoreReduction4_1T::CoreReduction4_1T(void) : COREVINIT {}

CoreReduction4T::CoreReduction4T(void) : COREVINIT {}

OptionalT<AtomTypeT *> CoreReduction4T::LookUpAtomType(std::string const &Text)
{
	auto Type = TypeLookup.find(Text);
	if (Type == TypeLookup.end()) return {};
	return &*Type->second;
}
	
bool CoreReduction4T::CouldBeAtomType(std::string const &Text)
{
	auto Type = TypeLookup.lower_bound(Text);
	if (Type == TypeLookup.end()) return false;
	if (Type->first.length() < Text.length()) return false;
	return Type->first.substr(0, Text.length()) == Text;
}

CoreReduction5T::CoreReduction5T(void) : 
	COREVINIT, 
	UndoingOrRedoing(false)
{
}
	
CoreT::CoreT(VisualT &RootVisual) : 
	CoreBaseT(*this, RootVisual),
	CoreReduction2T(*this), 
	FrameVisual(RootVisual.Root)
{
	FrameVisual.SetClass("frame");
	
	Types.emplace("SoloProtoatom", std::make_unique<SoloProtoatomTypeT>());
	SoloProtoatomType = Types["SoloProtoatom"].get();
	Types.emplace("InsertProtoatom", std::make_unique<InsertProtoatomTypeT>());
	InsertProtoatomType = Types["InsertProtoatom"].get();
	Types.emplace("AppendProtoatom", std::make_unique<AppendProtoatomTypeT>());
	AppendProtoatomType = Types["AppendProtoatom"].get();

	Root.Callback = [this](NucleusT *Nucleus) { RootRefresh = true; };
	Refresh();
}

CoreT::~CoreT(void)
{
	std::ofstream("dump.kk") << Dump();
}

Serial::ReadErrorT CoreT::Configure(Serial::ReadObjectT &Object)
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
				if (Type->Tag == "Element")
				{
					CheckElementType(Type);
					ElementType = Type;
				}
				else if (Type->Tag == "String")
				{
					CheckStringType(Type);
					StringType = Type;
				}
				return {};
			});
			return {};
		});
		return {};
	});
	Object.UInt("FrameDepth", [this](uint64_t Value) -> Serial::ReadErrorT 
		{ Settings_.FrameDepth = Value; return {}; });
	Object.Bool("UnframeAtRoot", [this](bool Value) -> Serial::ReadErrorT 
		{ Settings_.UnframeAtRoot = Value; return {}; });
	Object.Bool("StartFramed", [this](bool Value) -> Serial::ReadErrorT 
		{ Settings_.StartFramed = Value; return {}; });
	return {};
}

Serial::ReadErrorT CoreT::ConfigureFinished(void)
{
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
	if (!ElementType) return {"Missing type for Element in config."};
	if (!StringType) return {"Missing type for String in config."};
	for (auto &Type : Types)
		if (Type.second->Operator) TypeLookup[*Type.second->Operator] = Type.second.get();
	return {};
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
	auto UndoLevel = std::make_unique<UndoLevelT>();
	AtomT NewRoot(*this);
	{
		Serial::ReadT Read;
		Read.Object([this, &NewRoot, &UndoLevel](Serial::ReadObjectT &Object) -> Serial::ReadErrorT
		{
			//return Deserialize(NewRoot, "Module", Object);
			Object.Polymorph("Root", [this, &NewRoot, &UndoLevel](std::string &&Key, Serial::ReadObjectT &Object)
			{
				return CoreReduction1T::Deserialize(UndoLevel, NewRoot, std::move(Key), Object);
			});
			return {};
		});
		Serial::ReadErrorT ParseError;
		ParseError = Read.Parse(Path);
		if (ParseError) throw ConstructionErrorT() << *ParseError;
	}
	Root.Set(UndoLevel, NewRoot.Nucleus());
	if (Settings_.StartFramed) Frame(Root.Nucleus());
	AssumeFocus(UndoLevel);
	Refresh();
	Focused_->RegisterActions();
	RegisterActions();
}

bool InHandleInput = false; // DEBUG
void CoreT::HandleInput(std::shared_ptr<ActionT> Action)
{
	TRACE;
	Assert(!InHandleInput);
	InHandleInput = true;

	auto UndoLevel = std::make_unique<UndoLevelT>();

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

	Focused_->RegisterActions();
	RegisterActions();
	
	if (HandleInputFinishedCallback)
		HandleInputFinishedCallback();

	InHandleInput = false;
}

bool CoreT::HasChanges(void)
{
	return !UndoQueue.empty() || !RedoQueue.empty();
}
	
void CoreT::Focus(NucleusT *Nucleus)
{
	Assert(!InHandleInput);
	auto Action = std::make_shared<FunctionActionT>("", [this, Nucleus](std::unique_ptr<UndoLevelT> &Level)
	{
		Nucleus->Focus(Level, FocusDirectionT::Direct);
	});
	HandleInput(Action);
}

void CoreT::Refresh(void)
{
	if (RootRefresh)
	{
		RootVisual.Start();
		Assert(Root.Nucleus());
		if (Framed && (Framed.Nucleus() != Root.Nucleus()))
		{
			FrameVisual.Start();
			FrameVisual.Add(Framed->Visual);
			RootVisual.Add(FrameVisual);
		}
		else RootVisual.Add(Root->Visual);
		RootRefresh = false;
	}
	for (auto &Refreshable : NeedRefresh)
		Refreshable->Refresh();
	NeedRefresh.clear();
	if (NeedScroll && Focused_)
	{
		Focused_->Visual.Scroll();
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
}

void CoreT::RegisterAction(std::shared_ptr<ActionT> Action)
{
	Actions.push_back(Action);
	if (RegisterActionCallback) RegisterActionCallback(Action);
}

void CoreT::RegisterActions(void)
{
	RegisterAction(std::make_unique<FunctionActionT>("Undo", [this](std::unique_ptr<UndoLevelT> &Level) 
	{ 
		UndoingOrRedoing = true;
		if (UndoQueue.empty()) return;
		auto UndoLevel = std::make_unique<UndoLevelT>();
		UndoQueue.front()->ApplyUndo(UndoLevel);
		RedoQueue.push_front(std::move(UndoLevel));
		UndoQueue.pop_front();
		UndoingOrRedoing = false;
	}));

	RegisterAction(std::make_unique<FunctionActionT>("Redo", [this](std::unique_ptr<UndoLevelT> &Level) 
	{ 
		UndoingOrRedoing = true;
		if (RedoQueue.empty()) return;
		auto UndoLevel = std::make_unique<UndoLevelT>();
		RedoQueue.front()->ApplyRedo(UndoLevel);
		UndoQueue.push_front(std::move(UndoLevel));
		RedoQueue.pop_front();
		UndoingOrRedoing = false;
	}));
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

