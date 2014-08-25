#include "composite.h"

#include "logging.h"
#include "protoatom.h"

namespace Core
{

CompositeT::CompositeT(CoreT &Core, CompositeTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), PrefixVisual(Core.RootVisual.Root), InfixVisual(Core.RootVisual.Root), SuffixVisual(Core.RootVisual.Root), EffectivelyVertical(TypeInfo.SpatiallyVertical)
{
	TRACE;
	Visual.Tag().Add(TypeInfo.Tag);
	Visual.SetClass("type");
	Visual.SetClass(StringT() << "type-" << TypeInfo.Tag);
	PrefixVisual.SetClass("affix-outer");
	InfixVisual.SetClass("affix-outer");
	SuffixVisual.SetClass("affix-outer");
}

Serial::ReadErrorT CompositeT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	for (auto &Part : Parts) 
	{
		auto Error = Part->Deserialize(Object);
		if (Error) return Error;
	}
	return {};
}

void CompositeT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	TRACE;
	for (auto &Part : Parts)
		Part->Serialize(Polymorph);
}

AtomTypeT const &CompositeT::GetTypeInfo(void) const
{
	TRACE;
	return TypeInfo;
}

void CompositeT::Focus(FocusDirectionT Direction)
{
	TRACE;
	/*switch (Direction)
	{
		case FocusDirectionT::FromAhead:
			Focused = PartFocusedT{Parts.size() - 1};
			Parts.back()->Focus(Direction);
			break;
		case FocusDirectionT::FromBehind:
			Focused = PartFocusedT{0};
			Parts.front()->Focus(Direction);
			break;
		case FocusDirectionT::Direct:
			if (Core.TextMode && FocusDefault()) break;
			Focused = SelfFocusedT{};
			NucleusT::Focus(Direction);
			Visual.SetClass("flag-focused");
			break;
	}*/
	if (Core.TextMode && FocusDefault()) return;
	Focused = SelfFocusedT{};
	NucleusT::Focus(Direction);
	Visual.SetClass("flag-focused");
}

void CompositeT::Defocus(void)
{
	TRACE;
	Visual.UnsetClass("flag-focused");
}

void CompositeT::AssumeFocus(void)
{
	TRACE;
	if (Focused && Focused.Is<PartFocusedT>()) Parts[Focused.Get<PartFocusedT>()]->AssumeFocus();
	else Focus(FocusDirectionT::Direct);
}

void CompositeT::Refresh(void)
{
	TRACE;
	if (TypeInfo.DisplayPrefix) 
	{
		PrefixVisual.Start();
		PrefixVisual.Add(*TypeInfo.DisplayPrefix);
	}
	if (TypeInfo.DisplayInfix) 
	{
		InfixVisual.Start();
		InfixVisual.Add(*TypeInfo.DisplayInfix);
	}
	if (TypeInfo.DisplaySuffix) 
	{
		SuffixVisual.Start();
		SuffixVisual.Add(*TypeInfo.DisplaySuffix);
	}
	
	Visual.Start();
	
	if (EffectivelyVertical)
 		Visual.SetClass("flag-vertical");
	else Visual.UnsetClass("flag-vertical");
	
	Visual.Add(PrefixVisual);
	size_t Index = 0;
	for (auto &Part : Parts)
	{
		if (Index == 1) Visual.Add(InfixVisual);
		Visual.Add(Part->Visual);
		++Index;
	}
	Visual.Add(SuffixVisual);
}

std::unique_ptr<ActionT> CompositeT::Set(NucleusT *Nucleus)
{
	TRACE;
	size_t Index = 0;
	for (auto &PartInfo : TypeInfo.Parts)
	{
		if (PartInfo->SetDefault) 
			return Parts[Index]->Set(Nucleus);
		++Index;
	}
	Assert(false);
	return {};
}

std::unique_ptr<ActionT> CompositeT::Set(std::string const &Text)	
{
	TRACE;
	size_t Index = 0;
	for (auto &PartInfo : TypeInfo.Parts)
	{
		if (PartInfo->SetDefault) 
			return Parts[Index]->Set(Text);
		Index += 1;
	}
	Assert(false);
	return {};
}

OptionalT<std::unique_ptr<ActionT>> CompositeT::HandleInput(InputT const &Input)
{
	TRACE;
	if (Input.Main)
	{
		if (Focused.Is<PartFocusedT>())
		{
			if (EffectivelyVertical)
			{
				switch (*Input.Main)
				{
					case InputT::MainT::Up: FocusPrevious(); return {};
					case InputT::MainT::Down: FocusNext(); return {};
					default: break;
				}
			}
			else
			{
				switch (*Input.Main)
				{
					case InputT::MainT::Left: FocusPrevious(); return {};
					case InputT::MainT::Right: FocusNext(); return {};
					default: break;
				}
			}
			switch (*Input.Main)
			{
				case InputT::MainT::Exit: Focus(FocusDirectionT::Direct); return {};
				default: break;
			}
		}
		if (Focused.Is<SelfFocusedT>())
		{
			switch (*Input.Main)
			{
				case InputT::MainT::Delete:
				{
					auto RValueThis = this;
					if (!AsProtoatom(RValueThis))
					{
						if (Atom) return std::unique_ptr<ActionT>(new AtomT::SetT(*Atom, Core.ProtoatomType->Generate(Core)));
						return {};
					}
					else break;
				}
				case InputT::MainT::Enter:
					FocusDefault();
					return {};
				default: break;
			}
		}
	}
	if (Parent) return Parent->HandleInput(Input);
	return {};
}

void CompositeT::FocusPrevious(void)
{
	TRACE;
	Assert(Focused.Is<PartFocusedT>());
	auto &Index = Focused.Get<PartFocusedT>();
	if (Index == 0) { if (Parent) Parent->FocusPrevious(); }
	else 
	{
		Index -= 1;
		Parts[Index]->Focus(FocusDirectionT::FromAhead);
	}
}

void CompositeT::FocusNext(void)
{
	TRACE;
	Assert(Focused.Is<PartFocusedT>());
	auto &Index = Focused.Get<PartFocusedT>();
	if (Index + 1 == Parts.size()) 
	{ 
		if (Parent) Parent->FocusNext(); 
	}
	else 
	{
		Index += 1;
		Parts[Index]->Focus(FocusDirectionT::FromBehind);
	}
}

bool CompositeT::FocusDefault(void)
{
	TRACE;
	size_t Index = 0;
	for (auto &PartInfo : TypeInfo.Parts)
	{
		if (PartInfo->FocusDefault) 
		{
			Focused = PartFocusedT{Index};
			Parts[Index]->AssumeFocus();
			return true;
		}
		++Index;
	}
	return false;
}

Serial::ReadErrorT CompositeTypeT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	{
		auto Error = AtomTypeT::Deserialize(Object);
		if (Error) return Error;
	}
	Object.String("DisplayPrefix", [this](std::string &&Value) -> Serial::ReadErrorT 
		{ DisplayPrefix = std::move(Value); return {}; });
	Object.String("DisplayInfix", [this](std::string &&Value) -> Serial::ReadErrorT 
		{ DisplayInfix = std::move(Value); return {}; });
	Object.String("DisplaySuffix", [this](std::string &&Value) -> Serial::ReadErrorT 
		{ DisplaySuffix = std::move(Value); return {}; });
	Object.Array("Parts", [this](Serial::ReadArrayT &Array) -> Serial::ReadErrorT
	{
		Array.Polymorph([this](std::string &&Type, Serial::ReadObjectT &Object) -> Serial::ReadErrorT 
		{
			CompositeTypePartT *Out = nullptr;
			if (Type == "Atom") Out = new AtomPartTypeT(*this);
			else if (Type == "AtomList") Out = new AtomListPartTypeT(*this);
			else if (Type == "String") Out = new StringPartTypeT(*this);
			else if (Type == "Enum") Out = new EnumPartTypeT(*this);
			else if (Type == "Protoatom") Out = new ProtoatomPartTypeT(*this);
			else return (::StringT() << "Unknown part type \"" << Type << "\"").str();
			Parts.push_back(std::unique_ptr<CompositeTypePartT>(Out));
			return Out->Deserialize(Object);
		});
		return {};
	});
	return {};
}
	
void CompositeTypeT::Serialize(Serial::WriteObjectT &Object) const
{
	TRACE;
	AtomTypeT::Serialize(Object);
	if (DisplayPrefix) Object.String("DisplayPrefix", *DisplayPrefix);
	if (DisplayInfix) Object.String("DisplayInfix", *DisplayInfix);
	if (DisplaySuffix) Object.String("DisplaySuffix", *DisplaySuffix);
	auto Array = Object.Array("Parts");
	for (auto &Part : Parts) Part->Serialize(Array.Polymorph());
}

NucleusT *CompositeTypeT::Generate(CoreT &Core)
{
	TRACE;
	auto Out = new CompositeT(Core, *this);
	for (auto &Part : Parts) 
	{
		Out->Parts.emplace_back(Core);
		Out->Parts.back().Parent = Out;
		Out->Parts.back().Set(Part->Generate(Core));
	}
	return Out;
}

CompositeTypePartT::CompositeTypePartT(CompositeTypeT &Parent) : Parent(Parent) { TRACE; }

Serial::ReadErrorT CompositeTypePartT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.String("Tag", [this](std::string &&Value) -> Serial::ReadErrorT 
		{ Tag = std::move(Value); return {}; });
	Object.Bool("SpatiallyVertical", [this](bool Value) -> Serial::ReadErrorT 
		{ SpatiallyVertical = Value; return {}; });
	Object.Bool("FocusDefault", [this](bool Value) -> Serial::ReadErrorT 
		{ FocusDefault = Value; return {}; });
	Object.Bool("SetDefault", [this](bool Value) -> Serial::ReadErrorT 
		{ SetDefault = Value; return {}; });
	Object.String("DisplayPrefix", [this](std::string &&Value) -> Serial::ReadErrorT 
		{ DisplayPrefix = std::move(Value); return {}; });
	Object.String("DisplaySuffix", [this](std::string &&Value) -> Serial::ReadErrorT 
		{ DisplaySuffix = std::move(Value); return {}; });
	return {};
}

void CompositeTypePartT::Serialize(Serial::WriteObjectT &Object) const
{
	TRACE;
	Object.String("Tag", Tag);
	Object.Bool("SpatiallyVertical", SpatiallyVertical);
	Object.Bool("FocusDefault", FocusDefault);
	Object.Bool("SetDefault", SetDefault);
	if (DisplayPrefix) Object.String("DisplayPrefix", *DisplayPrefix);
	if (DisplaySuffix) Object.String("DisplaySuffix", *DisplaySuffix);
}

Serial::ReadErrorT AtomPartTypeT::Deserialize(Serial::ReadObjectT &Object) 
{
	TRACE;
	auto Error = CompositeTypePartT::Deserialize(Object);
	if (Error) return Error;
	Object.Bool("StartEmpty", [this](bool Value) -> Serial::ReadErrorT { StartEmpty = true; return {}; });
	return {};
}

void AtomPartTypeT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	TRACE;
	Serial::WritePolymorphT Polymorph("Atom", std::move(Prepolymorph));
	Serialize(Polymorph);
}

void AtomPartTypeT::Serialize(Serial::WriteObjectT &Object) const
{
	TRACE;
	AtomTypeT::Serialize(Object);
	Object.Bool("StartEmpty", StartEmpty);
}

NucleusT *AtomPartTypeT::Generate(CoreT &Core)
{
	TRACE;
	return new AtomPartT(Core, *this);
}

AtomPartT::AtomPartT(CoreT &Core, AtomPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), PrefixVisual(Core.RootVisual.Root), SuffixVisual(Core.RootVisual.Root), Data(Core)
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
	PrefixVisual.SetClass("affix-inner");
	SuffixVisual.SetClass("affix-inner");
	Data.Parent = this;
	Data.Callback = [this, &Core](NucleusT *Nucleus) 
	{ 
		if (Data)
		{
			auto Protoatom = AsProtoatom(Data);
			if (Protoatom) VectorRemove(
				Protoatom->FocusDependents, 
				[this](HoldT &Hold) { return Hold.Nucleus == this; });
		}
		if (Nucleus) 
		{
			auto Protoatom = AsProtoatom(Nucleus);
			if (Protoatom) Protoatom->FocusDependents.emplace_back(Core, this);
		}
		FlagRefresh(); 
	};
	if (!TypeInfo.StartEmpty)
		Data.Set(Core.ProtoatomType->Generate(Core));
}

Serial::ReadErrorT AtomPartT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.Polymorph(TypeInfo.Tag, [this](std::string &&Type, Serial::ReadObjectT &Object) -> Serial::ReadErrorT
	{
		return Core.Deserialize(Data, Type, Object);
	});
	return {};
}

void AtomPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	TRACE; 
	/*Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);*/
	if (Data) Data->Serialize(Polymorph.Polymorph(TypeInfo.Tag)); 
}

AtomTypeT const &AtomPartT::GetTypeInfo(void) const { return TypeInfo; }

void AtomPartT::Focus(FocusDirectionT Direction) 
{
	TRACE;
	if (Data) Data->Focus(Direction); 
	else
	{
		switch (Direction)
		{
			case FocusDirectionT::FromAhead: 
				Parent->FocusPrevious(); 
				break;
			case FocusDirectionT::FromBehind: 
			case FocusDirectionT::Direct: 
				Parent->FocusNext(); 
				break;
		}
	}
}

void AtomPartT::Defocus(void) { TRACE; }

void AtomPartT::AssumeFocus(void) 
{
	TRACE; 
	if (Data) Data->AssumeFocus(); 
	else Parent->FocusNext(); 
}

void AtomPartT::Refresh(void) 
{
	TRACE;
	Visual.Start();
	
	if (!Data) return;
	
	if (TypeInfo.DisplayPrefix) 
	{
		PrefixVisual.Start();
		PrefixVisual.Add(*TypeInfo.DisplayPrefix);
	}
	if (TypeInfo.DisplaySuffix) 
	{
		SuffixVisual.Start();
		SuffixVisual.Add(*TypeInfo.DisplaySuffix);
	}
	
	auto Protoatom = AsProtoatom(Data);
	if (Protoatom && Protoatom->IsEmpty()) return;
	Visual.Add(PrefixVisual);
	Visual.Add(Data->Visual);
	Visual.Add(SuffixVisual);
}

std::unique_ptr<ActionT> AtomPartT::Set(NucleusT *Nucleus) { TRACE; return std::unique_ptr<ActionT>(new AtomT::SetT(Data, Nucleus)); }

OptionalT<std::unique_ptr<ActionT>> AtomPartT::HandleInput(InputT const &Input) { TRACE; return Parent->HandleInput(Input); }

void AtomPartT::FocusPrevious(void) { TRACE; Parent->FocusPrevious(); }

void AtomPartT::FocusNext(void) { TRACE; Parent->FocusNext(); }

void AtomListPartTypeT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	TRACE;
	Serial::WritePolymorphT Polymorph("AtomList", std::move(Prepolymorph));
	Serialize(Polymorph);
}

NucleusT *AtomListPartTypeT::Generate(CoreT &Core)
{
	TRACE;
	return new AtomListPartT(Core, *this);
}

AtomListPartT::AtomListPartT(CoreT &Core, AtomListPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), FocusIndex(0), EffectivelyVertical(TypeInfo.SpatiallyVertical)
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
	Add(0, Core.ProtoatomType->Generate(Core));
}

Serial::ReadErrorT AtomListPartT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.Array(TypeInfo.Tag, [this](Serial::ReadArrayT &Array) -> Serial::ReadErrorT
	{
		FocusIndex = 0;
		Data.clear();
		Array.Polymorph([this](std::string &&Type, Serial::ReadObjectT &Object) -> Serial::ReadErrorT
		{
			Add(Data.size(), nullptr, false);
			return Core.Deserialize(Data.back()->Atom, Type, Object);
		});
		return {};
	});
	return {};
}

void AtomListPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	TRACE;
	/*Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);*/
	auto Array = Polymorph.Array(TypeInfo.Tag);
	for (auto &Atom : Data)
		{ Atom->Atom->Serialize(Array.Polymorph()); }
}

AtomTypeT const &AtomListPartT::GetTypeInfo(void) const { return TypeInfo; }

void AtomListPartT::Focus(FocusDirectionT Direction) 
{
	TRACE; 
	switch (Direction)
	{
		case FocusDirectionT::FromAhead: FocusIndex = Data.size() - 1; break;
		case FocusDirectionT::FromBehind: FocusIndex = 0; break;
		case FocusDirectionT::Direct: 
			if (FocusIndex + 1 >= Data.size()) FocusIndex = Data.size() - 1;
			break;
	}
	Data[FocusIndex]->Atom->Focus(Direction);
}

void AtomListPartT::Defocus(void) { TRACE; Assert(false); }

void AtomListPartT::AssumeFocus(void) 
{
	TRACE;
	Assert(!Data.empty());
	FocusIndex = std::min(FocusIndex, Data.size() - 1);
	Data[FocusIndex]->Atom->AssumeFocus();
}

void AtomListPartT::Refresh(void) 
{
	TRACE;
	Visual.Start();
	for (auto &Atom : Data)
	{
		if (TypeInfo.DisplayPrefix) 
		{
			Atom->PrefixVisual.Start();
			Atom->PrefixVisual.Add(*TypeInfo.DisplayPrefix);
		}
		if (TypeInfo.DisplaySuffix) 
		{
			Atom->SuffixVisual.Start();
			Atom->SuffixVisual.Add(*TypeInfo.DisplaySuffix);
		}
		Atom->Visual.Start();
		Atom->Visual.Add(Atom->PrefixVisual);
		Atom->Visual.Add(Atom->Atom->Visual);
		Atom->Visual.Add(Atom->SuffixVisual);
		Visual.Add(Atom->Visual);
	}
	if (EffectivelyVertical)
 		Visual.SetClass("flag-vertical");
	else Visual.UnsetClass("flag-vertical");
}

void AtomListPartT::Add(size_t Position, NucleusT *Nucleus, bool ShouldFocus)
{
	TRACE;
	Data.emplace(Data.begin() + Position, new ItemT{{Core.RootVisual.Root}, {Core.RootVisual.Root}, {Core.RootVisual.Root}, {Core}});
	Data[Position]->PrefixVisual.SetClass("affix-inner");
	Data[Position]->SuffixVisual.SetClass("affix-inner");
	Data[Position]->Atom.Parent = this;
	Data[Position]->Atom.Callback = [this](NucleusT *Nucleus)
	{
		FlagRefresh();
	};
	Data[Position]->Atom.Set(Nucleus);
	
	if (ShouldFocus)
	{
		FocusIndex = Position;
		if (Data[Position]->Atom)
			Data[Position]->Atom->Focus(FocusDirectionT::FromBehind);
	}
	FlagRefresh();
}

void AtomListPartT::Remove(size_t Position)
{
	TRACE;
	Assert(Position < Data.size());
	Data.erase(Data.begin() + Position);
	Core.AssumeFocus();
	FlagRefresh();
}

AtomListPartT::AddRemoveT::AddRemoveT(AtomListPartT &Base, bool Add, size_t Position, NucleusT *Nucleus) : Base(Base), Add(Add), Position(Position), Nucleus(Base.Core, Nucleus) { TRACE; }

std::unique_ptr<ActionT> AtomListPartT::AddRemoveT::Apply(void)
{
	TRACE;
	std::unique_ptr<ActionT> Out;
	if (Add) 
	{
		Out.reset(new AddRemoveT(Base, false, Position, nullptr));
		Base.Add(Position, Nucleus.Nucleus, true);
	}
	else 
	{
		Out.reset(new AddRemoveT(Base, true, Position, Base.Data[Position]->Atom.Nucleus));
		Base.Remove(Position);
	}
	return std::move(Out);
}

std::unique_ptr<ActionT> AtomListPartT::Set(NucleusT *Nucleus) 
{
	TRACE;
	return std::unique_ptr<ActionT>(new AddRemoveT(*this, true, Data.size(), Nucleus));
}

std::unique_ptr<ActionT> AtomListPartT::Set(std::string const &Text) { TRACE; Assert(false); return {}; }

OptionalT<std::unique_ptr<ActionT>> AtomListPartT::HandleInput(InputT const &Input) 
{
	TRACE; 
	if (Input.Main)
	{
		if (EffectivelyVertical)
		{
			switch (*Input.Main)
			{
				case InputT::MainT::Up: FocusPrevious(); return {};
				case InputT::MainT::Down: FocusNext(); return {};
				default: break;
			}
		}
		else
		{
			switch (*Input.Main)
			{
				case InputT::MainT::Left: FocusPrevious(); return {};
				case InputT::MainT::Right: FocusNext(); return {};
				default: break;
			}
		}

		switch (*Input.Main)
		{
			case InputT::MainT::NewStatementBefore:
				Core.TextMode = true;
				return std::unique_ptr<ActionT>(new AddRemoveT(*this, true, FocusIndex, Core.ProtoatomType->Generate(Core)));
			case InputT::MainT::NewStatement:
				Core.TextMode = true;
				return std::unique_ptr<ActionT>(new AddRemoveT(*this, true, FocusIndex + 1, Core.ProtoatomType->Generate(Core)));
			case InputT::MainT::Delete:
				if (Data.size() > 1)
					return std::unique_ptr<ActionT>(new AddRemoveT(*this, false, FocusIndex, {}));
				break;
			default: break;
		}
	}
	return Parent->HandleInput(Input);
}

void AtomListPartT::FocusPrevious(void) 
{
	TRACE; 
	if (FocusIndex == 0) Parent->FocusPrevious();
	else 
	{
		FocusIndex -= 1;
		Data[FocusIndex]->Atom->Focus(Core.TextMode ? FocusDirectionT::FromAhead : FocusDirectionT::Direct);
	}
}

void AtomListPartT::FocusNext(void) 
{
	TRACE; 
	if (FocusIndex + 1 == Data.size()) Parent->FocusNext();
	else 
	{
		FocusIndex += 1;
		Data[FocusIndex]->Atom->Focus(Core.TextMode ? FocusDirectionT::FromBehind : FocusDirectionT::Direct);
	}
}

void StringPartTypeT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	TRACE;
	Serial::WritePolymorphT Polymorph("String", std::move(Prepolymorph));
	Serialize(Polymorph);
}

NucleusT *StringPartTypeT::Generate(CoreT &Core)
{
	TRACE;
	return new StringPartT(Core, *this);
}

StringPartT::StringPartT(CoreT &Core, StringPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), PrefixVisual(Core.RootVisual.Root), SuffixVisual(Core.RootVisual.Root), Focused(FocusedT::Off), Position(0)
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
	PrefixVisual.SetClass("affix-inner");
	SuffixVisual.SetClass("affix-inner");
}

Serial::ReadErrorT StringPartT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.String(TypeInfo.Tag, [this](std::string &&Value) -> Serial::ReadErrorT 
		{ Data = std::move(Value); return {}; });
	return {};
}

void StringPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	TRACE; 
	/*Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);*/
	Polymorph.String(TypeInfo.Tag, Data); 
}

AtomTypeT const &StringPartT::GetTypeInfo(void) const { return TypeInfo; }

void StringPartT::Focus(FocusDirectionT Direction) 
{
	TRACE; 
	if (Core.TextMode)
	{
		switch (Direction)
		{
			case FocusDirectionT::FromAhead: Position = Data.size(); break;
			case FocusDirectionT::FromBehind: Position = 0; break;
			case FocusDirectionT::Direct: Position = 0; break;
		}
		Focused = FocusedT::Text;
	}
	else
	{
		Visual.SetClass("flag-focused");
		Focused = FocusedT::On;
	}
	NucleusT::Focus(Direction);
	FlagRefresh();
}

void StringPartT::Defocus(void) 
{
	TRACE;
	Focused = FocusedT::Off;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
}

void StringPartT::AssumeFocus(void) 
{
	TRACE; 
	Focus(FocusDirectionT::Direct);
}

void StringPartT::Refresh(void) 
{
	TRACE;
	if (TypeInfo.DisplayPrefix) 
	{
		PrefixVisual.Start();
		PrefixVisual.Add(*TypeInfo.DisplayPrefix);
	}
	if (TypeInfo.DisplaySuffix) 
	{
		SuffixVisual.Start();
		SuffixVisual.Add(*TypeInfo.DisplaySuffix);
	}
	
	Visual.Start();
	Visual.Add(PrefixVisual);
	if (Focused == FocusedT::Text)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
	Visual.Add(SuffixVisual);
}

std::unique_ptr<ActionT> StringPartT::Set(NucleusT *Nucleus) { TRACE; Assert(false); return {}; }

StringPartT::SetT::SetT(StringPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) { TRACE; }

std::unique_ptr<ActionT> StringPartT::SetT::Apply(void)
{
	TRACE;
	auto Out = new SetT(Base, Base.Position, Base.Data);
	Base.Data = Data;
	Base.Position = Position;
	Base.FlagRefresh();
	return std::unique_ptr<ActionT>(Out);
}

bool StringPartT::SetT::Combine(std::unique_ptr<ActionT> &Other)
{
	TRACE;
	auto Set = dynamic_cast<SetT *>(Other.get());
	if (!Set) return false;
	if (&Set->Base != &Base) return false;
	Position = Set->Position;
	Data = Set->Data;
	return true;
}

std::unique_ptr<ActionT> StringPartT::Set(std::string const &Text) 
{
	TRACE; 
	return std::unique_ptr<ActionT>(new SetT(*this, Position, Text));
}

OptionalT<std::unique_ptr<ActionT>> StringPartT::HandleInput(InputT const &Input) 
{
	TRACE; 
	AssertNE(Focused, FocusedT::Off);
	if (Focused == FocusedT::Text)
	{
		if (Input.Text)
		{
			return std::unique_ptr<ActionT>(new SetT(
				*this, 
				Position + 1, 
				Data.substr(0, Position) + *Input.Text + Data.substr(Position)));
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
					return {};
				}
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
					return {};
				}
				case InputT::MainT::TextBackspace:
				{
					if (Position == 0) return {};
					auto NewData = Data;
					NewData.erase(Position - 1, 1);
					auto NewPosition = Position - 1;
					return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, NewData));
				}
				case InputT::MainT::Delete:
				{
					if (Position == Data.size()) return {};
					auto NewData = Data;
					NewData.erase(Position, 1);
					return std::unique_ptr<ActionT>(new SetT(*this, Position, NewData));
				}
				case InputT::MainT::Exit:
				{
					Core.TextMode = false;
					Focus(FocusDirectionT::Direct);
					return {};
				}
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
				case InputT::MainT::Enter:
					Core.TextMode = true;
					Focus(FocusDirectionT::Direct);
					return {};
				case InputT::MainT::Delete:
					return std::unique_ptr<ActionT>(new SetT(*this, 0, ""));
				default: break;
			}
		}
	}
	return Parent->HandleInput(Input);
}

Serial::ReadErrorT EnumPartTypeT::Deserialize(Serial::ReadObjectT &Object) 
{
	TRACE;
	CompositeTypePartT::Deserialize(Object);
	Object.Array("Values", [this](Serial::ReadArrayT &Array) -> Serial::ReadErrorT
	{
		Array.String([this](std::string &&Value) -> Serial::ReadErrorT 
		{ 
			ValueLookup[Value] = Values.size();
			Values.emplace_back(std::move(Value)); 
			return {};
		});
		return {};
	});
	return {};
}

void EnumPartTypeT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	TRACE;
	Serial::WritePolymorphT Polymorph("Enum", std::move(Prepolymorph));
	Serialize(Polymorph);
}

void EnumPartTypeT::Serialize(Serial::WriteObjectT &Object) const
{
	TRACE;
	AtomTypeT::Serialize(Object);
	auto Array = Object.Array("Values");
	for (auto &Value : Values) Array.String(Value);
}

NucleusT *EnumPartTypeT::Generate(CoreT &Core)
{
	TRACE;
	return new EnumPartT(Core, *this);
}

EnumPartT::EnumPartT(CoreT &Core, EnumPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), PrefixVisual(Core.RootVisual.Root), SuffixVisual(Core.RootVisual.Root), Index(0) 
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
	PrefixVisual.SetClass("affix-inner");
	SuffixVisual.SetClass("affix-inner");
}

Serial::ReadErrorT EnumPartT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.String(TypeInfo.Tag, [this](std::string &&Value) -> Serial::ReadErrorT 
	{
		auto Found = TypeInfo.ValueLookup.find(Value);
		if (Found == TypeInfo.ValueLookup.end())
			Index = std::numeric_limits<decltype(Index)>::max();
		else Index = Found->second;
		return {};
	});
	return {};
}

void EnumPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	TRACE; 
	/*Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);*/
	if (Index >= TypeInfo.Values.size())
	{
		Assert(false);
		Polymorph.String(TypeInfo.Tag, "Invalid");
	}
	else
	{
		Polymorph.String(TypeInfo.Tag, TypeInfo.Values[Index]);
	}
}

AtomTypeT const &EnumPartT::GetTypeInfo(void) const { return TypeInfo; }

void EnumPartT::Focus(FocusDirectionT Direction)
{
	TRACE; 
	NucleusT::Focus(Direction);
	Visual.SetClass("flag-focused");
}

void EnumPartT::Defocus(void)
{
	TRACE;
	Visual.UnsetClass("flag-focused");
}

void EnumPartT::AssumeFocus(void)
{
	TRACE; 
	Focus(FocusDirectionT::Direct);
}

void EnumPartT::Refresh(void)
{
	TRACE; 
	if (TypeInfo.DisplayPrefix) 
	{
		PrefixVisual.Start();
		PrefixVisual.Add(*TypeInfo.DisplayPrefix);
	}
	if (TypeInfo.DisplaySuffix) 
	{
		SuffixVisual.Start();
		SuffixVisual.Add(*TypeInfo.DisplaySuffix);
	}
	
	Visual.Start();
	Visual.Add(PrefixVisual);
	if (Index >= TypeInfo.Values.size())
	{
		Assert(false);
		Visual.Add("Invalid");
	}
	else
	{
		Visual.Add(TypeInfo.Values[Index]);
	}
	Visual.Add(SuffixVisual);
}
	
EnumPartT::SetT::SetT(EnumPartT &Base, size_t Index) : Base(Base), Index(Index)
{
	TRACE;
}

std::unique_ptr<ActionT> EnumPartT::SetT::Apply(void)
{
	TRACE;
	auto Out = new SetT(Base, Index);
	Base.Index = Index;
	Base.FlagRefresh();
	return std::unique_ptr<ActionT>(Out);
}
	
std::unique_ptr<ActionT> EnumPartT::Set(NucleusT *Nucleus) { TRACE; Assert(false); return {}; }

std::unique_ptr<ActionT> EnumPartT::Set(std::string const &Text) { TRACE; Assert(false); return {}; }

OptionalT<std::unique_ptr<ActionT>> EnumPartT::HandleInput(InputT const &Input)
{
	TRACE; 
	if (Input.Main)
	{
		switch (*Input.Main)
		{
			case InputT::MainT::Enter: 
			{
				return std::unique_ptr<ActionT>(new SetT(*this, (Index + 1) % TypeInfo.Values.size()));
			}
			default: break;
		}
	}
	return Parent->HandleInput(Input);
}

}
