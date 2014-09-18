#include "composite.h"

#include "logging.h"
#include "protoatom.h"

namespace Core
{

CompositeT::CompositeT(CoreT &Core, CompositeTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), OperatorVisual(Core.RootVisual.Root), EffectivelyVertical(TypeInfo.SpatiallyVertical)
{
	TRACE;
	Visual.Tag().Add(TypeInfo.Tag);
	Visual.SetClass("type");
	Visual.SetClass(StringT() << "type-" << TypeInfo.Tag);
	OperatorVisual.SetClass("operator");
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
			void Apply(void)
			{
				TRACE;
				auto &Index = Base.Focused.Get<PartFocusedT>();
				if (Index > 0)
				{
					Index -= 1;
					Base.Parts[Index]->Focus(FocusDirectionT::FromAhead);
				}
			}
		};

		struct FocusNextT : ActionT
		{
			CompositeT &Base;
			FocusNextT(std::string const &Name, CompositeT &Base) : ActionT(Name), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				auto &Index = Base.Focused.Get<PartFocusedT>();
				if (Index + 1 < Base.Parts.size()) 
				{
					Index += 1;
					Base.Parts[Index]->Focus(FocusDirectionT::FromBehind);
				}
			}
		};
		if (EffectivelyVertical)
		{
			Core.RegisterAction(std::make_shared<FocusPreviousT>("Up", *this));
			Core.RegisterAction(std::make_shared<FocusNextT>("Down", *this));
		}
		else
		{
			Core.RegisterAction(std::make_shared<FocusPreviousT>("Left", *this));
			Core.RegisterAction(std::make_shared<FocusNextT>("Right", *this));
		}

		struct ExitT : ActionT
		{
			CompositeT &Base;
			ExitT(CompositeT &Base) : ActionT("Exit"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				Base.Core.TextMode = false;
				Base.Focus(FocusDirectionT::Direct);
			}
		};
		Core.RegisterAction(std::make_shared<ExitT>(*this));
	}
	else if (Focused.Is<SelfFocusedT>())
	{
		struct DeleteT : ActionT
		{
			CompositeT &Base;
			DeleteT(CompositeT &Base) : ActionT("Delete"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				auto Protoatom = Base.As<SoloProtoatomT>();
				if (!Protoatom || !Protoatom->IsEmpty())
				{
					if (Base.Atom) 
						Base.Atom->Set(Base.Core.SoloProtoatomType->Generate(Base.Core));
				}
			}
		};
		Core.RegisterAction(std::make_shared<DeleteT>(*this));

		struct WedgeT : ActionT
		{
			CompositeT &Base;
			bool const Before;
			WedgeT(CompositeT &Base, bool Before) : ActionT(Before ? "Insert before" : "Insert after"), Base(Base), Before(Before) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Atom)
				{
					Base.Core.TextMode = true;
					auto Replacement = (Before ? Base.Core.InsertProtoatomType : Base.Core.AppendProtoatomType)->Generate(Base.Core);
					auto BaseAtom = Base.Atom;
					Replacement->As<WedgeProtoatomT>()->SetLifted(&Base);
					BaseAtom->Set(Replacement);
				}
			}
		};
		Core.RegisterAction(std::make_shared<WedgeT>(*this, true));
		Core.RegisterAction(std::make_shared<WedgeT>(*this, false));

		struct ReplaceParentT : ActionT
		{
			CompositeT &Base;
			ReplaceParentT(CompositeT &Base) : ActionT("Replace parent"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				auto Replacee = Base.PartParent();
				if (Replacee && Replacee->Atom)
					Replacee->Atom->Set(&Base);
			}
		};
		Core.RegisterAction(std::make_shared<ReplaceParentT>(*this));

		struct EnterT : ActionT
		{
			CompositeT &Base;
			EnterT(CompositeT &Base) : ActionT("Enter"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				Base.FocusDefault();
			}
		};
		Core.RegisterAction(std::make_shared<EnterT>(*this));
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

	if (TypeInfo.Operator)
	{
		OperatorVisual.Start();
		OperatorVisual.Add(*TypeInfo.Operator);
	}
	
	Visual.Start();
	
	if (EffectivelyVertical)
 		Visual.SetClass("flag-vertical");
	else Visual.UnsetClass("flag-vertical");
	
	size_t Index = 0;
	for (auto &Part : Parts)
	{
		if (TypeInfo.Operator && (Index == TypeInfo.OperatorPosition)) Visual.Add(OperatorVisual);
		Visual.Add(Part->Visual);
		++Index;
	}
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
	for (auto &Part : Parts)
	{
		if (dynamic_cast<CompositePartTypeT const *>(&Part->GetTypeInfo())->FocusDefault) 
		{
			Focused = PartFocusedT{Index};
			Part->AssumeFocus();
			return true;
		}
		++Index;
	}
	return false;
}
	
OptionalT<AtomT *> CompositeT::GetOperand(OperatorDirectionT Direction, size_t Offset)
{
	size_t Position = 0;
	bool StartSkipOperator = false;
	size_t PartIndex = 0;
	if (Direction == OperatorDirectionT::Right)
	{
		StartSkipOperator = true;
		PartIndex = TypeInfo.OperatorPosition;
	}
	for (; PartIndex < Parts.size(); ++PartIndex)
	{
		auto &Part = Parts[PartIndex];

		if (StartSkipOperator) StartSkipOperator = false;
		else if (PartIndex == TypeInfo.OperatorPosition) break;

		if (auto AtomPart = Part->As<AtomPartT>())
		{
			if (Position == Offset) return &AtomPart->Data;
			++Position;
		}
		else if (auto AtomListPart = Part->As<AtomListPartT>())
		{
			for (auto &Part : AtomListPart->Data)
			{
				if (Position == Offset) return &Part->Atom;
				++Position;
			}
		}
	}
	return {};
}

OptionalT<NucleusT *> CompositeT::GetAnyOperand(OperatorDirectionT Direction, size_t Offset)
{
	size_t Position = 0;
	bool StartSkipOperator = false;
	size_t PartIndex = 0;
	if (Direction == OperatorDirectionT::Right)
	{
		StartSkipOperator = true;
		PartIndex = TypeInfo.OperatorPosition;
	}
	for (; PartIndex < Parts.size(); ++PartIndex)
	{
		auto &Part = Parts[PartIndex];

		if (StartSkipOperator) StartSkipOperator = false;
		else if (PartIndex == TypeInfo.OperatorPosition) break;

		if (auto AtomPart = Part->As<AtomPartT>())
		{
			if (Position == Offset) return AtomPart->Data.Nucleus;
			++Position;
		}
		else if (auto AtomListPart = Part->As<AtomListPartT>())
		{
			for (auto &Part : AtomListPart->Data)
			{
				if (Position == Offset) return Part->Atom.Nucleus;
				++Position;
			}
		}
		else
		{
			if (Position == Offset) return Part.Nucleus;
			++Position;
		}
	}
	return {};
}

OptionalT<OperatorDirectionT> CompositeT::GetOperandSide(AtomT *Operand)
{
	auto Out = OperatorDirectionT::Left;
	for (auto PartIndex = 0; PartIndex < Parts.size(); ++PartIndex)
	{
		auto &Part = Parts[PartIndex];

		if (PartIndex >= TypeInfo.OperatorPosition) Out = OperatorDirectionT::Right;

		if (auto AtomPart = Part->As<AtomPartT>())
		{
			if (&AtomPart->Data == Operand) return Out;
		}
		else if (auto AtomListPart = Part->As<AtomListPartT>())
		{
			for (auto &Part : AtomListPart->Data)
				if (&Part->Atom == Operand) return Out;
		}
	}
	return {};
}

Serial::ReadErrorT CompositeTypeT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	{
		auto Error = AtomTypeT::Deserialize(Object);
		if (Error) return Error;
	}
	Object.Array("Parts", [this](Serial::ReadArrayT &Array) -> Serial::ReadErrorT
	{
		Array.Polymorph([this](std::string &&Type, Serial::ReadObjectT &Object) -> Serial::ReadErrorT 
		{
			CompositePartTypeT *Out = nullptr;
			if (Type == "Operator") Out = new OperatorPartTypeT(*this);
			else if (Type == "Atom") Out = new AtomPartTypeT(*this);
			else if (Type == "AtomList") Out = new AtomListPartTypeT(*this);
			else if (Type == "String") Out = new StringPartTypeT(*this);
			else if (Type == "Enum") Out = new EnumPartTypeT(*this);
			else return (::StringT() << "Unknown part type \"" << Type << "\"").str();
			Parts.push_back(std::unique_ptr<CompositePartTypeT>(Out));
			return Out->Deserialize(Object);
		});
		Array.Finally([this](void) -> Serial::ReadErrorT
		{
			size_t Position = 0;
			for (auto &Part : Parts) 
			{
				if (auto OperatorType = dynamic_cast<OperatorPartTypeT *>(Part.get()))
				{
					OperatorPosition = Position;
					Operator = OperatorType->Pattern;
					return {};
				}
				++Position;
			}
			return {};
		});
		return {};
	});
	return {};
}
	
void CompositeTypeT::Serialize(Serial::WriteObjectT &Object) const
{
	TRACE;
	AtomTypeT::Serialize(Object);
	auto Array = Object.Array("Parts");
	for (auto &Part : Parts) Part->Serialize(Array.Polymorph());
}

NucleusT *CompositeTypeT::Generate(CoreT &Core) { TRACE; return GenerateComposite<CompositeT>(*this, Core); }

bool CompositeTypeT::IsAssociative(OperatorDirectionT Direction) const
{
	return LeftAssociative == (Direction == OperatorDirectionT::Left);
}
	
CompositePartTypeT::CompositePartTypeT(CompositeTypeT &Parent) : Parent(Parent) { TRACE; }

Serial::ReadErrorT CompositePartTypeT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.String("Tag", [this](std::string &&Value) -> Serial::ReadErrorT 
		{ Tag = std::move(Value); return {}; });
	Object.Bool("SpatiallyVertical", [this](bool Value) -> Serial::ReadErrorT 
		{ SpatiallyVertical = Value; return {}; });
	Object.Bool("FocusDefault", [this](bool Value) -> Serial::ReadErrorT 
		{ FocusDefault = Value; return {}; });
	return {};
}

void CompositePartTypeT::Serialize(Serial::WriteObjectT &Object) const
{
	TRACE;
	Object.String("Tag", Tag);
	Object.Bool("SpatiallyVertical", SpatiallyVertical);
	Object.Bool("FocusDefault", FocusDefault);
}
	
Serial::ReadErrorT OperatorPartTypeT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.String("Pattern", [this](std::string &&Value) -> Serial::ReadErrorT 
	{ 
		Pattern = std::move(Value); 
		return {}; 
	});
	return {};
}

void OperatorPartTypeT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	TRACE;
	Serial::WritePolymorphT Polymorph("Atom", std::move(Prepolymorph));
	Serialize(Polymorph);
}

void OperatorPartTypeT::Serialize(Serial::WriteObjectT &Object) const 
{
	TRACE;
	AtomTypeT::Serialize(Object);
	Object.String("Pattern", Pattern);
}

NucleusT *OperatorPartTypeT::Generate(CoreT &Core)
	{ Assert(false); return nullptr; }

Serial::ReadErrorT AtomPartTypeT::Deserialize(Serial::ReadObjectT &Object) 
{
	TRACE;
	auto Error = CompositePartTypeT::Deserialize(Object);
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

AtomPartT::AtomPartT(CoreT &Core, AtomPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), Data(Core)
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
	Data.Parent = this;
	Data.Callback = [this, &Core](NucleusT *Nucleus) 
	{ 
		if (Data) Data->IgnoreStatus((uintptr_t)this);
		if (Nucleus) Nucleus->WatchStatus((uintptr_t)this, [](NucleusT *Changed) { Changed->Parent->FlagRefresh(); });
		FlagRefresh(); 
	};
	if (!TypeInfo.StartEmpty)
		Data.Set(Core.SoloProtoatomType->Generate(Core));
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
	
	if (!IsPrecedent(Data, Data.Nucleus)) Visual.SetClass("flag-antiprecedent");
	else Visual.UnsetClass("flag-antiprecedent");
	
	{
		auto Protoatom = Data->As<SoloProtoatomT>();
		if (Protoatom && Protoatom->IsEmpty() && !Protoatom->IsFocused()) return;
	}
	Visual.Add(Data->Visual);
}

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
	Add(0, Core.SoloProtoatomType->Generate(Core));
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
		void Apply(void)
		{
			TRACE;
			if (Base.FocusIndex > 0)
			{
				Base.FocusIndex -= 1;
				Base.Data[Base.FocusIndex]->Atom->Focus(FocusDirectionT::FromAhead);
			}
		}
	};

	struct FocusNextT : ActionT
	{
		AtomListPartT &Base;
		FocusNextT(std::string const &Name, AtomListPartT &Base) : ActionT(Name), Base(Base) {}
		void Apply(void)
		{
			TRACE;
			if (Base.FocusIndex + 1 < Base.Data.size()) 
			{
				Base.FocusIndex += 1;
				Base.Data[Base.FocusIndex]->Atom->Focus(FocusDirectionT::FromBehind);
			}
		}
	};
	if (EffectivelyVertical)
	{
		Core.RegisterAction(std::make_shared<FocusPreviousT>("Up", *this));
		Core.RegisterAction(std::make_shared<FocusNextT>("Down", *this));
	}
	else
	{
		Core.RegisterAction(std::make_shared<FocusPreviousT>("Left", *this));
		Core.RegisterAction(std::make_shared<FocusNextT>("Right", *this));
	}

	struct NewStatementBeforeT : ActionT
	{
		AtomListPartT &Base;
		NewStatementBeforeT(AtomListPartT &Base) : ActionT("Insert statement before"), Base(Base) {}
		void Apply(void)
		{
			TRACE;
			Base.Core.TextMode = true;
			Base.Add(Base.FocusIndex, Base.Core.SoloProtoatomType->Generate(Base.Core), true);
		}
	};
	Core.RegisterAction(std::make_shared<NewStatementBeforeT>(*this));

	struct NewStatementAfterT : ActionT
	{
		AtomListPartT &Base;
		NewStatementAfterT(AtomListPartT &Base) : ActionT("Insert statement after"), Base(Base) {}
		void Apply(void)
		{
			TRACE;
			Base.Core.TextMode = true;
			Base.Add(Base.FocusIndex + 1, Base.Core.SoloProtoatomType->Generate(Base.Core), true);
		}
	};
	Core.RegisterAction(std::make_shared<NewStatementAfterT>(*this));

	if (Core.TextMode)
	{
		struct NewStatementAfterT : ActionT
		{
			AtomListPartT &Base;
			NewStatementAfterT(AtomListPartT &Base) : ActionT("Finish and insert statement after"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				OptionalT<ProtoatomPartT *> ProtoatomPart;
				{
					auto Temp = Base.Core.Focused->As<ProtoatomPartT>();
					if (Temp) ProtoatomPart = Temp;
				}
				if (!ProtoatomPart)
				{
					auto Protoatom = Base.Core.Focused->As<ProtoatomT>();
					if (Protoatom) ProtoatomPart = Protoatom->GetProtoatomPart();
				}
				if (ProtoatomPart) ProtoatomPart->Finish({}, ProtoatomPart->Data);
				Base.Add(Base.FocusIndex + 1, Base.Core.SoloProtoatomType->Generate(Base.Core), true);
			}
		};
		Core.RegisterAction(std::make_shared<NewStatementAfterT>(*this));
	}

	if (IsFocused())
	{
		struct DeleteT : ActionT
		{
			AtomListPartT &Base;
			DeleteT(AtomListPartT &Base) : ActionT("Delete"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Data.size() > 1)
					Base.Remove(Base.FocusIndex);
			}
		};
		Core.RegisterAction(std::make_shared<DeleteT>(*this));
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
		Atom->Visual.Start();
		Atom->Visual.Add(Atom->Atom->Visual);
		Visual.Add(Atom->Visual);
	}
	if (EffectivelyVertical)
 		Visual.SetClass("flag-vertical");
	else Visual.UnsetClass("flag-vertical");
}

void AtomListPartT::Add(size_t Position, NucleusT *Nucleus, bool ShouldFocus)
{
	TRACE;
	Core.AddUndoReaction(make_unique<AddRemoveT>(*this, false, Position, nullptr));
	Data.emplace(Data.begin() + Position, new ItemT{{Core.RootVisual.Root}, {Core}});
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
	Core.AddUndoReaction(make_unique<AddRemoveT>(*this, true, Position, Data[Position]->Atom.Nucleus));
	Assert(Position < Data.size());
	Data.erase(Data.begin() + Position);
	Core.AssumeFocus();
	FlagRefresh();
}

AtomListPartT::AddRemoveT::AddRemoveT(AtomListPartT &Base, bool Add, size_t Position, NucleusT *Nucleus) : Base(Base), Add(Add), Position(Position), Nucleus(Base.Core, Nucleus) { TRACE; }

void AtomListPartT::AddRemoveT::Apply(void)
{
	TRACE;
	if (Add) Base.Add(Position, Nucleus.Nucleus, true);
	else Base.Remove(Position);
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

StringPartT::StringPartT(CoreT &Core, StringPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), Focused(FocusedT::Off), Position(0)
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
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
	if (Parent->As<CompositeT>()->Parts.size() == 1) Core.TextMode = true;
	if (Core.TextMode)
	{
		if (Focused != FocusedT::Text)
		{
			switch (Direction)
			{
				case FocusDirectionT::FromAhead: Position = Data.size(); break;
				case FocusDirectionT::FromBehind: Position = 0; break;
				case FocusDirectionT::Direct: Position = 0; break;
			}
			Focused = FocusedT::Text;
		}
	}
	else
	{
		if (Focused != FocusedT::On)
		{
			Visual.SetClass("flag-focused");
			Focused = FocusedT::On;
		}
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

			void Apply(void)
			{
				TRACE;
				Base.Set(
					Base.Position + 1,
					Base.Data.substr(0, Base.Position) + 
						Argument.Data + 
						Base.Data.substr(Base.Position));
			}
		};
		Core.RegisterAction(std::make_shared<TextT>(*this));

		struct FocusPreviousT : ActionT
		{
			StringPartT &Base;
			FocusPreviousT(StringPartT &Base) : ActionT("Left"), Base(Base) {}
			void Apply(void)
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
			}
		};
		Core.RegisterAction(std::make_shared<FocusPreviousT>(*this));

		struct FocusNextT : ActionT
		{
			StringPartT &Base;
			FocusNextT(StringPartT &Base) : ActionT("Right"), Base(Base) {}
			void Apply(void)
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
			}
		};
		Core.RegisterAction(std::make_shared<FocusNextT>(*this));

		struct BackspaceT : ActionT
		{
			StringPartT &Base;
			BackspaceT(StringPartT &Base) : ActionT("Backspace"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Position == 0) return;
				auto NewData = Base.Data;
				NewData.erase(Base.Position - 1, 1);
				auto NewPosition = Base.Position - 1;
				Base.Set(NewPosition, NewData);
			}
		};
		Core.RegisterAction(std::make_shared<BackspaceT>(*this));

		struct DeleteT : ActionT
		{
			StringPartT &Base;
			DeleteT(StringPartT &Base) : ActionT("Delete"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Position == Base.Data.size()) return;
				auto NewData = Base.Data;
				NewData.erase(Base.Position, 1);
				Base.Set(Base.Position, NewData);
			}
		};
		Core.RegisterAction(std::make_shared<DeleteT>(*this));

		if (Parent->As<CompositeT>()->Parts.size() > 1)
		{
			struct ExitT : ActionT
			{
				StringPartT &Base;
				ExitT(StringPartT &Base) : ActionT("Exit"), Base(Base) {}
				void Apply(void)
				{
					TRACE;
					Base.Core.TextMode = false;
					Base.Focus(FocusDirectionT::Direct);
				}
			};
			Core.RegisterAction(std::make_shared<ExitT>(*this));
		}
	}
	else if (Focused == FocusedT::On)
	{
		struct DeleteT : ActionT
		{
			StringPartT &Base;
			DeleteT(StringPartT &Base) : ActionT("Delete"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				Base.Set(0, "");
			}
		};
		Core.RegisterAction(std::make_shared<DeleteT>(*this));

		struct EnterT : ActionT
		{
			StringPartT &Base;
			EnterT(StringPartT &Base) : ActionT("Enter"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				Base.Core.TextMode = true;
				Base.Focus(FocusDirectionT::Direct);
			}
		};
		Core.RegisterAction(std::make_shared<EnterT>(*this));
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
	Visual.Start();
	if (Focused == FocusedT::Text)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
}

void StringPartT::Set(size_t Position, std::string const &Data)
{
	TRACE;
	struct SetT : ReactionT
	{
		StringPartT &Base;
		unsigned int Position;
		std::string Data;
		
		SetT(StringPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) { TRACE; }

		void Apply(void) 
		{ 
			TRACE;
			Base.Set(Position, Data); 
		}

		bool Combine(std::unique_ptr<ReactionT> &Other) override
		{
			auto Set = dynamic_cast<SetT *>(Other.get());
			if (!Set) return false;
			if (&Set->Base != &Base) return false;
			Position = Set->Position;
			Data = Set->Data;
			return true;
		}
	};

	Core.AddUndoReaction(make_unique<SetT>(*this, this->Position, this->Data));
	this->Data = Data;
	this->Position = Position;
	FlagRefresh();
}

Serial::ReadErrorT EnumPartTypeT::Deserialize(Serial::ReadObjectT &Object) 
{
	TRACE;
	CompositePartTypeT::Deserialize(Object);
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

EnumPartT::EnumPartT(CoreT &Core, EnumPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), Index(0) 
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
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
		void Apply(void)
		{
			TRACE;
			struct SetT : ReactionT
			{
				EnumPartT &Base;
				size_t Index;

				SetT(EnumPartT &Base, size_t Index) : Base(Base), Index(Index)
				{
					TRACE;
				}

				void Apply(void)
				{
					TRACE;
					Base.Core.AddUndoReaction(make_unique<SetT>(Base, Base.Index));
					Base.Index = Index;
					Base.FlagRefresh();
				}
			};
			SetT(Base, (Base.Index + 1) % Base.TypeInfo.Values.size()).Apply();
		}
	};
	Core.RegisterAction(std::make_shared<AdvanceValueT>(*this));
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
	
	Visual.Start();
	if (Index >= TypeInfo.Values.size())
	{
		Assert(false);
		Visual.Add("Invalid");
	}
	else
	{
		Visual.Add(TypeInfo.Values[Index]);
	}
}
	
void CheckStringType(AtomTypeT *Type)
{
	auto Composite = dynamic_cast<CompositeTypeT *>(Type);
	Assert(Composite);
	if (!(
		(Composite->Parts.size() == 2) &&
		(dynamic_cast<OperatorPartTypeT *>(Composite->Parts[0].get())) &&
		(dynamic_cast<StringPartTypeT *>(Composite->Parts[1].get()))
	)) throw ConstructionErrorT() << "String has unusable definition.";
}

StringPartT *GetStringPart(NucleusT *Nucleus)
{
	return *Nucleus->As<CompositeT>()->Parts[0]->As<StringPartT>();
}

void CheckElementType(AtomTypeT *Type)
{
	auto Composite = dynamic_cast<CompositeTypeT *>(Type);
	Assert(Composite);
	if (!(
		(Composite->Parts.size() == 3) &&
		(dynamic_cast<AtomPartTypeT *>(Composite->Parts[0].get())) &&
		(dynamic_cast<OperatorPartTypeT *>(Composite->Parts[1].get())) &&
		(dynamic_cast<AtomPartTypeT *>(Composite->Parts[2].get()))
	)) throw ConstructionErrorT() << "Element has unusable definition.";
}

AtomPartT *GetElementLeftPart(NucleusT *Nucleus)
{
	return *Nucleus->As<CompositeT>()->Parts[0]->As<AtomPartT>();
}

AtomPartT *GetElementRightPart(NucleusT *Nucleus)
{
	return *Nucleus->As<CompositeT>()->Parts[1]->As<AtomPartT>();
}

bool IsPrecedent(AtomT &ParentAtom, NucleusT *Child)
{
	// Answers "If ChildType were in ParentAtom, would it be precedent compared to Parent containing ParentAtom?"
	auto Parent = *ParentAtom->PartParent()->As<CompositeT>();
	auto ParentSide = Parent->GetOperandSide(&ParentAtom);
	Assert(ParentSide);
	bool AreFacing = Child->As<CompositeT>()->GetOperand(
		*ParentSide == OperatorDirectionT::Left ? 
			OperatorDirectionT::Right : OperatorDirectionT::Left,
		0);
	if (!AreFacing) return true;
	if (Child->GetTypeInfo().Precedence > Parent->GetTypeInfo().Precedence) return true;
	if (Child->GetTypeInfo().Precedence < Parent->GetTypeInfo().Precedence) return false;
	return Parent->TypeInfo.IsAssociative(*ParentSide);
}

}
