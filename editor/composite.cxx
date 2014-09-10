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
	FlagStatusChange();
	Visual.SetClass("flag-focused");
}
	
void CompositeT::RegisterActions(void)
{
	TRACE;

	if (Focused.Is<PartFocusedT>())
	{
		struct FocusPreviousT : ActionT
		{
			CompositeT &Base;
			FocusPreviousT(std::string const &Name, CompositeT &Base) : ActionT(Name), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				auto &Index = Base.Focused.Get<PartFocusedT>();
				if (Index > 0)
				{
					Index -= 1;
					Base.Parts[Index]->Focus(FocusDirectionT::FromAhead);
				}
				return {};
			}
		};

		struct FocusNextT : ActionT
		{
			CompositeT &Base;
			FocusNextT(std::string const &Name, CompositeT &Base) : ActionT(Name), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				auto &Index = Base.Focused.Get<PartFocusedT>();
				if (Index + 1 < Base.Parts.size()) 
				{
					Index += 1;
					Base.Parts[Index]->Focus(FocusDirectionT::FromBehind);
				}
				return {};
			}
		};
		if (EffectivelyVertical)
		{
			Core.RegisterAction(make_unique<FocusPreviousT>("Up", *this));
			Core.RegisterAction(make_unique<FocusNextT>("Down", *this));
		}
		else
		{
			Core.RegisterAction(make_unique<FocusPreviousT>("Left", *this));
			Core.RegisterAction(make_unique<FocusNextT>("Right", *this));
		}

		struct ExitT : ActionT
		{
			CompositeT &Base;
			ExitT(CompositeT &Base) : ActionT("Exit"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				Base.Core.TextMode = false;
				Base.Focus(FocusDirectionT::Direct);
				return {};
			}
		};
		Core.RegisterAction(make_unique<ExitT>(*this));
	}
	else if (Focused.Is<SelfFocusedT>())
	{
		struct DeleteT : ActionT
		{
			CompositeT &Base;
			DeleteT(CompositeT &Base) : ActionT("Delete"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				auto ThisRef = &Base;
				if (!AsProtoatom(ThisRef) || !Base.IsEmpty())
				{
					if (Base.Atom) return std::unique_ptr<ReactionT>(new AtomT::SetT(*Base.Atom, Base.Core.ProtoatomType->Generate(Base.Core)));
				}
				return {};
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));

		struct WedgeT : ActionT
		{
			CompositeT &Base;
			WedgeT(CompositeT &Base) : ActionT("Wedge"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Atom)
				{
					Base.Core.TextMode = true;
					auto Reactions = new ReactionGroupT;
					auto Replacement = Base.Core.ProtoatomType->Generate(Base.Core);
					Reactions->Add(Replacement->Set(&Base));
					Reactions->Add(std::unique_ptr<ReactionT>(new AtomT::SetT(*Base.Atom, Replacement)));
					return std::unique_ptr<ReactionT>(Reactions);
				}
				return {};
			}
		};
		Core.RegisterAction(make_unique<WedgeT>(*this));

		struct ReplaceParentT : ActionT
		{
			CompositeT &Base;
			ReplaceParentT(CompositeT &Base) : ActionT("Replace parent"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				auto Replacee = Base.PartParent();
				if (Replacee && Replacee->Atom)
					return std::unique_ptr<ReactionT>(new AtomT::SetT(*Replacee->Atom, &Base));
				return {};
			}
		};
		Core.RegisterAction(make_unique<ReplaceParentT>(*this));

		struct EnterT : ActionT
		{
			CompositeT &Base;
			EnterT(CompositeT &Base) : ActionT("Enter"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				Base.FocusDefault();
				return {};
			}
		};
		Core.RegisterAction(make_unique<EnterT>(*this));
	}
	else Assert(false);

	if (Parent) Parent->RegisterActions();
}

void CompositeT::Defocus(void)
{
	TRACE;
	Visual.UnsetClass("flag-focused");
	FlagStatusChange();
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

std::unique_ptr<ReactionT> CompositeT::Set(NucleusT *Nucleus)
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

std::unique_ptr<ReactionT> CompositeT::Set(std::string const &Text)	
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
	
bool CompositeT::IsEmpty(void) const
{
	TRACE;
	for (auto &Part : Parts)
		if (!Part->IsEmpty()) return false;
	return true;
}

bool CompositeT::IsFocused(void) const
{
	TRACE;
	if (NucleusT::IsFocused()) return true;
	for (auto &Part : Parts)
		if (Part->IsFocused()) return true;
	return false;
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
	
bool CompositeT::HasOnePart(void)
{
	TRACE;
	size_t Count = 0;
	for (auto &Part : Parts)
	{
		// These conditions are kind of a hack, designed to make strings (1 part) and protoatoms (1 potentially empty atom part, 1 potentially empty text part) autoselect the text part when that's the obvious choice
		if (!Part->IsEmpty() || 
			dynamic_cast<CompositeTypePartT const *>(&Part->GetTypeInfo())->FocusDefault) 
			++Count;
	}
	return Count == 1;
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
		Out->Parts.back().Callback = [](NucleusT *) { Assert(false); }; // Parts can't be replaced
		Out->Parts.back()->WatchStatus((uintptr_t)Out, [](NucleusT *Nucleus) { Nucleus->Parent->FlagStatusChange(); });
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
		if (Data) Data->IgnoreStatus((uintptr_t)this);
		if (Nucleus) Nucleus->WatchStatus((uintptr_t)this, [](NucleusT *Changed) { Changed->Parent->FlagRefresh(); });
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
	
void AtomPartT::RegisterActions(void)
{
	TRACE;
	Parent->RegisterActions();
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
	
	if (Data->IsEmpty() && !Data->IsFocused()) return;
	Visual.Add(PrefixVisual);
	Visual.Add(Data->Visual);
	Visual.Add(SuffixVisual);
}

std::unique_ptr<ReactionT> AtomPartT::Set(NucleusT *Nucleus) { TRACE; return std::unique_ptr<ReactionT>(new AtomT::SetT(Data, Nucleus)); }

void AtomPartT::FocusPrevious(void) { TRACE; Parent->FocusPrevious(); }

void AtomPartT::FocusNext(void) { TRACE; Parent->FocusNext(); }
	
bool AtomPartT::IsEmpty(void) const { TRACE; return !Data; }

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
	
void AtomListPartT::RegisterActions(void)
{
	TRACE;

	struct FocusPreviousT : ActionT
	{
		AtomListPartT &Base;
		FocusPreviousT(std::string const &Name, AtomListPartT &Base) : ActionT(Name), Base(Base) {}
		OptionalT<std::unique_ptr<ReactionT>> Apply(void)
		{
			TRACE;
			if (Base.FocusIndex > 0)
			{
				Base.FocusIndex -= 1;
				Base.Data[Base.FocusIndex]->Atom->Focus(FocusDirectionT::FromAhead);
			}
			return {};
		}
	};

	struct FocusNextT : ActionT
	{
		AtomListPartT &Base;
		FocusNextT(std::string const &Name, AtomListPartT &Base) : ActionT(Name), Base(Base) {}
		OptionalT<std::unique_ptr<ReactionT>> Apply(void)
		{
			TRACE;
			if (Base.FocusIndex + 1 < Base.Data.size()) 
			{
				Base.FocusIndex += 1;
				Base.Data[Base.FocusIndex]->Atom->Focus(FocusDirectionT::FromBehind);
			}
			return {};
		}
	};
	if (EffectivelyVertical)
	{
		Core.RegisterAction(make_unique<FocusPreviousT>("Up", *this));
		Core.RegisterAction(make_unique<FocusNextT>("Down", *this));
	}
	else
	{
		Core.RegisterAction(make_unique<FocusPreviousT>("Left", *this));
		Core.RegisterAction(make_unique<FocusNextT>("Right", *this));
	}

	struct NewStatementBeforeT : ActionT
	{
		AtomListPartT &Base;
		NewStatementBeforeT(AtomListPartT &Base) : ActionT("Insert statement before"), Base(Base) {}
		OptionalT<std::unique_ptr<ReactionT>> Apply(void)
		{
			TRACE;
			Base.Core.TextMode = true;
			return std::unique_ptr<ReactionT>(new AddRemoveT(Base, true, Base.FocusIndex, Base.Core.ProtoatomType->Generate(Base.Core)));
		}
	};
	Core.RegisterAction(make_unique<NewStatementBeforeT>(*this));

	struct NewStatementAfterT : ActionT
	{
		AtomListPartT &Base;
		NewStatementAfterT(AtomListPartT &Base) : ActionT("Insert statement after"), Base(Base) {}
		OptionalT<std::unique_ptr<ReactionT>> Apply(void)
		{
			TRACE;
			Base.Core.TextMode = true;
			return std::unique_ptr<ReactionT>(new AddRemoveT(Base, true, Base.FocusIndex + 1, Base.Core.ProtoatomType->Generate(Base.Core)));
		}
	};
	Core.RegisterAction(make_unique<NewStatementAfterT>(*this));

	if (IsFocused())
	{
		struct DeleteT : ActionT
		{
			AtomListPartT &Base;
			DeleteT(AtomListPartT &Base) : ActionT("Delete"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Data.size() > 1)
					return std::unique_ptr<ReactionT>(new AddRemoveT(Base, false, Base.FocusIndex, {}));
				return {};
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));
	}

	Parent->RegisterActions();
}

void AtomListPartT::Defocus(void) { TRACE; Assert(false); }

void AtomListPartT::AssumeFocus(void) 
{
	TRACE;
	Assert(!Data.empty());
	FocusIndex = std::min(FocusIndex, Data.size() - 1);
	if (Data[FocusIndex]->Atom)
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
	if (Nucleus)
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

std::unique_ptr<ReactionT> AtomListPartT::AddRemoveT::Apply(void)
{
	TRACE;
	std::unique_ptr<ReactionT> Out;
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

std::unique_ptr<ReactionT> AtomListPartT::Set(NucleusT *Nucleus) 
{
	TRACE;
	return std::unique_ptr<ReactionT>(new AddRemoveT(*this, true, Data.size(), Nucleus));
}

std::unique_ptr<ReactionT> AtomListPartT::Set(std::string const &Text) { TRACE; Assert(false); return {}; }

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
	if (Parent->As<CompositeT>()->HasOnePart()) Core.TextMode = true;
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

void StringPartT::RegisterActions(void)
{
	TRACE;
	if (Focused == FocusedT::Text)
	{
		struct TextT : ActionT
		{
			StringPartT &Base;
			ActionT::TextArgumentT Argument;
			TextT(StringPartT &Base) : ActionT("Enter text"), Base(Base)
				{ Arguments.push_back(&Argument); }

			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				return std::unique_ptr<ReactionT>(new SetT(
					Base, 
					Base.Position + 1, 
					Base.Data.substr(0, Base.Position) + 
						Argument.Data + 
						Base.Data.substr(Base.Position)));
			}
		};
		Core.RegisterAction(make_unique<TextT>(*this));

		struct FocusPreviousT : ActionT
		{
			StringPartT &Base;
			FocusPreviousT(StringPartT &Base) : ActionT("Left"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == 0) 
				{
					Base.Parent->FocusPrevious();
				}
				else
				{
					Base.Position -= 1;
					Base.FlagRefresh();
				}
				return {};
			}
		};
		Core.RegisterAction(make_unique<FocusPreviousT>(*this));

		struct FocusNextT : ActionT
		{
			StringPartT &Base;
			FocusNextT(StringPartT &Base) : ActionT("Right"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == Base.Data.size()) 
				{
					Base.Parent->FocusNext();
				}
				else
				{
					Base.Position += 1;
					Base.FlagRefresh();
				}
				return {};
			}
		};
		Core.RegisterAction(make_unique<FocusNextT>(*this));

		struct BackspaceT : ActionT
		{
			StringPartT &Base;
			BackspaceT(StringPartT &Base) : ActionT("Backspace"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == 0) return {};
				auto NewData = Base.Data;
				NewData.erase(Base.Position - 1, 1);
				auto NewPosition = Base.Position - 1;
				return std::unique_ptr<ReactionT>(new SetT(Base, NewPosition, NewData));
			}
		};
		Core.RegisterAction(make_unique<BackspaceT>(*this));

		struct DeleteT : ActionT
		{
			StringPartT &Base;
			DeleteT(StringPartT &Base) : ActionT("Delete"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				if (Base.Position == Base.Data.size()) return {};
				auto NewData = Base.Data;
				NewData.erase(Base.Position, 1);
				return std::unique_ptr<ReactionT>(new SetT(Base, Base.Position, NewData));
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));

		if (!Parent->As<CompositeT>()->HasOnePart())
		{
			struct ExitT : ActionT
			{
				StringPartT &Base;
				ExitT(StringPartT &Base) : ActionT("Exit"), Base(Base) {}
				OptionalT<std::unique_ptr<ReactionT>> Apply(void)
				{
					TRACE;
					Base.Core.TextMode = false;
					Base.Focus(FocusDirectionT::Direct);
					return {};
				}
			};
			Core.RegisterAction(make_unique<ExitT>(*this));
		}
	}
	else if (Focused == FocusedT::On)
	{
		struct DeleteT : ActionT
		{
			StringPartT &Base;
			DeleteT(StringPartT &Base) : ActionT("Delete"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				return std::unique_ptr<ReactionT>(new SetT(Base, 0, ""));
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));

		struct EnterT : ActionT
		{
			StringPartT &Base;
			EnterT(StringPartT &Base) : ActionT("Enter"), Base(Base) {}
			OptionalT<std::unique_ptr<ReactionT>> Apply(void)
			{
				TRACE;
				Base.Core.TextMode = true;
				Base.Focus(FocusDirectionT::Direct);
				return {};
			}
		};
		Core.RegisterAction(make_unique<EnterT>(*this));
	}
	Parent->RegisterActions();
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

std::unique_ptr<ReactionT> StringPartT::Set(NucleusT *Nucleus) { TRACE; Assert(false); return {}; }

StringPartT::SetT::SetT(StringPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) { TRACE; }

std::unique_ptr<ReactionT> StringPartT::SetT::Apply(void)
{
	TRACE;
	auto Out = new SetT(Base, Base.Position, Base.Data);
	Base.Data = Data;
	Base.Position = Position;
	Base.FlagRefresh();
	return std::unique_ptr<ReactionT>(Out);
}

bool StringPartT::SetT::Combine(std::unique_ptr<ReactionT> &Other)
{
	TRACE;
	auto Set = dynamic_cast<SetT *>(Other.get());
	if (!Set) return false;
	if (&Set->Base != &Base) return false;
	Position = Set->Position;
	Data = Set->Data;
	return true;
}

std::unique_ptr<ReactionT> StringPartT::Set(std::string const &Text) 
{
	TRACE; 
	return std::unique_ptr<ReactionT>(new SetT(*this, Position, Text));
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

void EnumPartT::RegisterActions(void)
{
	TRACE;
	struct AdvanceValueT : ActionT
	{
		EnumPartT &Base;
		AdvanceValueT(EnumPartT &Base) : ActionT("Advance value"), Base(Base) {}
		OptionalT<std::unique_ptr<ReactionT>> Apply(void)
		{
			TRACE;
			return std::unique_ptr<ReactionT>(new SetT(Base, (Base.Index + 1) % Base.TypeInfo.Values.size()));
		}
	};
	Core.RegisterAction(make_unique<AdvanceValueT>(*this));
	Parent->RegisterActions();
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

std::unique_ptr<ReactionT> EnumPartT::SetT::Apply(void)
{
	TRACE;
	auto Out = new SetT(Base, Index);
	Base.Index = Index;
	Base.FlagRefresh();
	return std::unique_ptr<ReactionT>(Out);
}
	
std::unique_ptr<ReactionT> EnumPartT::Set(NucleusT *Nucleus) { TRACE; Assert(false); return {}; }

std::unique_ptr<ReactionT> EnumPartT::Set(std::string const &Text) { TRACE; Assert(false); return {}; }

}
